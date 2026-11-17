#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <array>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

static int APS5_VABI ScanList(const char* input, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vsscanf_s_nid_postfix(input, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
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
        Expect(false);
    }
}

static int handlerCalls = 0;
static void APS5_VABI CountHandler(const char*, void*, int) { ++handlerCalls; }

int main() {
    set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(&CountHandler));
    for (const auto scan : {sscanf_s_nid_postfix, ScanList}) {
        const int sizedNull = scan == ScanList ? -23 : EOF;
        handlerCalls = 0;
        Expect(scan("7", "%d", static_cast<int*>(nullptr)) == EOF, "Null integer destination must return EOF");
        Expect(scan("1.5", "%f", static_cast<float*>(nullptr)) == EOF, "Null real destination must return EOF");
        Expect(scan("abc", "%s", static_cast<char*>(nullptr), std::size_t{4}) == sizedNull, "Null string destination must return -23 for vsscanf_s and EOF for sscanf_s");
        Expect(scan == ScanList || scan("a", "%c", static_cast<char*>(nullptr), std::size_t{1}) == EOF, "Null character destination must return EOF");
        Expect(scan == ScanList || scan("abc", "%[a-z]", static_cast<char*>(nullptr), std::size_t{4}) == EOF, "Null scanset destination must return EOF");
        Expect(handlerCalls == (scan == ScanList ? 1 : 0), "Only vsscanf_s calls the constraint handler for a null destination");
        Expect(scan("", "%n", static_cast<int*>(nullptr)) == EOF, "Null count destination must return EOF");
        int first = -1;
        int later = -1;
        Expect(scan("7 8 9", "%d%d%d", &first, static_cast<int*>(nullptr), &later) == EOF && first == 7 && later == -1,
            "Null destination after an assignment must stop with EOF");
        first = -1;
        Expect(scan("7", "%d%n%d", &first, static_cast<int*>(nullptr), &later) == EOF && first == 7 && later == -1,
            "Null count after an assignment must stop with EOF");
        first = -1;
        Expect(scan("x", "%d%d", &first, static_cast<int*>(nullptr)) == 0 && first == -1,
            "Matching failure must stop before an unreachable null destination");
        Expect(scan("7 8", "%*d%d", &first) == 1 && first == 8, "Suppressed conversions must not consume destinations");
    }
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
    Expect(sscanf_s_nid_postfix(" 12 abc!", "%ln%d %3[a-z]%jn!", &before, &number,
        word, std::size_t{4}, &after) == 2, "Count conversions must not increase assignments");
    Expect(before == 0 && number == 12 && std::strcmp(word, "abc") == 0 && after == 7,
        "Counts must include leading whitespace and earlier conversions");

    after = -1;
    Expect(sscanf_s_nid_postfix("123x", "%*d%tn", &after) == 0 && after == 3,
        "Count must include suppressed input without consuming a capacity argument");
    after = -1;
    Expect(sscanf_s_nid_postfix("x", "%d%lln", &number, &after) == 0 && after == -1,
        "Matching failure must leave a later count untouched");
    Expect(sscanf_s_nid_postfix("", "%d%lln", &number, &after) == EOF && after == -1,
        "Input failure must leave a later count untouched");
    return Failures() == 0 ? 0 : 1;
}
