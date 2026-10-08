#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" int APS5_VABI sscanf_s_nid_postfix(const char*, const char*, ...);

static int failures = 0;

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        ++failures;
    }
}

template<class TValue> static void CheckCount(const char* modifier, int count) {
    struct alignas(8) Destination {
        TValue value;
        std::array<unsigned char, 8> guard;
    } destination{static_cast<TValue>(-1), {}};
    destination.guard.fill(0xa5);
    const auto guard = destination.guard;
    const std::string input(static_cast<std::size_t>(count), 'x');
    const std::string format = input + "%" + modifier + "n";
    const int assigned = sscanf_s_nid_postfix(input.c_str(), format.c_str(), &destination.value);
    if (destination.value != count || destination.guard != guard || assigned != 0) {
        std::fprintf(stderr, "%%%sn after %d bytes: count=%lld, guard=%s, assignments=%d\n",
            modifier, count, static_cast<long long>(destination.value),
            destination.guard == guard ? "unchanged" : "overwritten", assigned);
        ++failures;
    }
}

int main() {
    for (const int count : {0, 3, 127}) {
        CheckCount<signed char>("hh", count);
        CheckCount<short>("h", count);
        CheckCount<int>("", count);
        CheckCount<std::int64_t>("l", count);
        CheckCount<long long>("ll", count);
        CheckCount<std::intmax_t>("j", count);
        CheckCount<std::int64_t>("z", count);
        CheckCount<std::ptrdiff_t>("t", count);
    }
    CheckCount<short>("h", 257);
    CheckCount<std::int64_t>("l", 257);

    int number = -1;
    std::int64_t before = -1;
    std::int64_t after = -1;
    char word[4] = {};
    Require(sscanf_s_nid_postfix(" 12 abc!", "%ln%d %3[a-z]%jn!", &before, &number,
        word, 4u, &after) == 2, "Count conversions must not increase assignments");
    Require(before == 0 && number == 12 && std::strcmp(word, "abc") == 0 && after == 7,
        "Counts must include leading whitespace and earlier conversions");

    after = -1;
    Require(sscanf_s_nid_postfix("123x", "%*d%tn", &after) == 0 && after == 3,
        "Count must include suppressed input without consuming a capacity argument");
    after = -1;
    Require(sscanf_s_nid_postfix("x", "%d%lln", &number, &after) == 0 && after == -1,
        "Matching failure must leave a later count untouched");
    Require(sscanf_s_nid_postfix("", "%d%lln", &number, &after) == EOF && after == -1,
        "Input failure must leave a later count untouched");
    return failures == 0 ? 0 : 1;
}
