#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <string_view>

extern "C" int APS5_VABI sscanf_s_nid_postfix(const char*, const char*, ...);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

template<class TValue>
void RequireCount(const char* modifier, int count) {
    struct alignas(8) Destination {
        TValue value;
        std::array<unsigned char, 8> guard;
    } destination{static_cast<TValue>(-1), {}};
    destination.guard.fill(0xa5);
    const auto guard = destination.guard;
    const std::string input(static_cast<std::size_t>(count), 'x');
    const std::string format = input + "%" + modifier + "n";
    const int assigned = sscanf_s_nid_postfix(input.c_str(), format.c_str(), &destination.value);
    const std::string label = std::string("%") + modifier + "n after " + std::to_string(count) + " bytes";
    RequireEqual(static_cast<long long>(destination.value), static_cast<long long>(count), label + ": count");
    Require(destination.guard == guard, label + ": guard bytes after the destination are unchanged");
    RequireEqual(assigned, 0, label + ": assignments");
}

template<class TValue>
void RequireCounts(const char* modifier, std::initializer_list<int> counts) {
    for (const int count : counts) RequireCount<TValue>(modifier, count);
}

const Case charCount{"SscanfS_HhCount_StoresSignedCharWithoutOverrun", [] {
    RequireCounts<signed char>("hh", {0, 3, 127});
}};

const Case shortCount{"SscanfS_HCount_StoresShortWithoutOverrun", [] {
    RequireCounts<short>("h", {0, 3, 127, 257});
}};

const Case intCount{"SscanfS_PlainCount_StoresIntWithoutOverrun", [] {
    RequireCounts<int>("", {0, 3, 127});
}};

const Case longCount{"SscanfS_LCount_StoresInt64WithoutOverrun", [] {
    RequireCounts<std::int64_t>("l", {0, 3, 127, 257});
}};

const Case longLongCount{"SscanfS_LlCount_StoresLongLongWithoutOverrun", [] {
    RequireCounts<long long>("ll", {0, 3, 127});
}};

const Case intmaxCount{"SscanfS_JCount_StoresIntmaxWithoutOverrun", [] {
    RequireCounts<std::intmax_t>("j", {0, 3, 127});
}};

const Case sizeCount{"SscanfS_ZCount_StoresInt64WithoutOverrun", [] {
    RequireCounts<std::int64_t>("z", {0, 3, 127});
}};

const Case ptrdiffCount{"SscanfS_TCount_StoresPtrdiffWithoutOverrun", [] {
    RequireCounts<std::ptrdiff_t>("t", {0, 3, 127});
}};

const Case mixedCounts{"SscanfS_CountsAroundConversions_IncludeWhitespaceAndDoNotAssign", [] {
    int number = -1;
    std::int64_t before = -1;
    std::int64_t after = -1;
    char word[4] = {};
    RequireEqual(sscanf_s_nid_postfix(" 12 abc!", "%ln%d %3[a-z]%jn!", &before, &number, word, 4u, &after), 2,
        "count conversions do not increase assignments");
    RequireEqual(before, std::int64_t{0}, "count before leading whitespace");
    RequireEqual(number, 12, "number");
    RequireEqual(std::string_view(word), std::string_view("abc"), "word");
    RequireEqual(after, std::int64_t{7}, "count after earlier conversions");
}};

const Case suppressedCount{"SscanfS_CountAfterSuppressedInput_IncludesSuppressedBytes", [] {
    std::int64_t after = -1;
    RequireEqual(sscanf_s_nid_postfix("123x", "%*d%tn", &after), 0, "assignments");
    RequireEqual(after, std::int64_t{3}, "count without consuming a capacity argument");
}};

const Case matchingFailure{"SscanfS_MatchingFailure_LeavesLaterCountUntouched", [] {
    int number = -1;
    std::int64_t after = -1;
    RequireEqual(sscanf_s_nid_postfix("x", "%d%lln", &number, &after), 0, "assignments");
    RequireEqual(after, std::int64_t{-1}, "later count");
}};

const Case inputFailure{"SscanfS_InputFailure_ReturnsEofAndLeavesLaterCountUntouched", [] {
    int number = -1;
    std::int64_t after = -1;
    RequireEqual(sscanf_s_nid_postfix("", "%d%lln", &number, &after), EOF, "result");
    RequireEqual(after, std::int64_t{-1}, "later count");
}};

} // namespace
