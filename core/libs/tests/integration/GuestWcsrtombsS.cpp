#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

extern "C" int APS5_VABI wcsrtombs_s_nid_postfix(
    std::size_t*, char*, std::size_t, const std::uint16_t**, std::size_t, void*);

namespace {

using namespace std::string_view_literals;
using Testing::Case;
using Testing::RequireEqual;

constexpr auto Failed = static_cast<std::size_t>(-1);
constexpr auto Huge = std::size_t{1} << 63;
constexpr std::uint16_t text[] = {'a', 'b', 'c', 0};
constexpr std::uint16_t wide[] = {'a', 0x100, 'b', 0};

struct Run {
    int status;
    std::size_t result = 7;
    const std::uint16_t* source;
    char out[8];
};

Run Convert(const std::uint16_t* input, std::size_t capacity, std::size_t limit, bool toBuffer = true) {
    Run run{};
    std::memset(run.out, 'x', sizeof(run.out));
    run.source = input;
    std::uint64_t state[2] = {};
    run.status = wcsrtombs_s_nid_postfix(&run.result, toBuffer ? run.out : nullptr, capacity, &run.source, limit, state);
    return run;
}

std::string Label(std::size_t capacity, std::size_t limit) {
    return "capacity " + std::to_string(capacity) + " limit " + std::to_string(limit);
}

void RequireRun(const Run& run, int status, std::size_t result, const std::string& label) {
    RequireEqual(run.status, status, label + " status");
    RequireEqual(run.result, result, label + " result");
}

void RequireSource(const Run& run, const std::uint16_t* source, const std::string& label) {
    RequireEqual(run.source == source, true, label + " source");
}

void RequireOutput(const Run& run, std::string_view expected, const std::string& label) {
    RequireEqual(std::string_view(run.out, expected.size()), expected, label + " output");
}

const Case lengthQuery{"WcsrtombsS_NullDestination_ReturnsLength", [] {
    const auto run = Convert(text, 0, 0, false);
    RequireRun(run, 0, 3, "length query");
    RequireSource(run, text, "length query");
}};

const Case lengthQueryInvalid{"WcsrtombsS_NullDestinationWithInvalidCharacter_FailsWithEilseq", [] {
    const auto run = Convert(wide, 0, Huge, false);
    RequireRun(run, 86, Failed, "invalid length query");
    RequireSource(run, wide, "invalid length query");
}};

const Case lengthQueryCapacity{"WcsrtombsS_NullDestinationWithCapacity_FailsWithEinval", [] {
    const auto run = Convert(text, 5, 1, false);
    RequireRun(run, 22, Failed, "length query with capacity");
    RequireSource(run, text, "length query with capacity");
}};

const Case enoughCapacity{"WcsrtombsS_EnoughCapacity_ConvertsAndTerminates", [] {
    for (const std::size_t size : {std::size_t{8}, std::size_t{4}}) {
        const auto label = Label(size, size);
        const auto run = Convert(text, size, size);
        RequireRun(run, 0, 3, label);
        RequireSource(run, nullptr, label);
        RequireOutput(run, "abc\0x"sv, label);
    }
}};

const Case limitedCount{"WcsrtombsS_LimitBelowLength_ConvertsLimitAndTerminates", [] {
    auto run = Convert(text, 8, 2);
    RequireRun(run, 0, 2, Label(8, 2));
    RequireSource(run, text + 2, Label(8, 2));
    RequireOutput(run, "ab\0x"sv, Label(8, 2));
    run = Convert(text, 8, 0);
    RequireRun(run, 0, 0, Label(8, 0));
    RequireSource(run, text, Label(8, 0));
    RequireOutput(run, "\0x"sv, Label(8, 0));
}};

const Case insufficientCapacity{"WcsrtombsS_InsufficientCapacity_FailsWithErangeAndClearsOutput", [] {
    struct Example {
        std::size_t capacity;
        std::size_t limit;
        std::size_t consumed;
        std::string_view output;
    };
    const Example examples[] = {{3, 8, 3, "\0bcx"sv}, {3, 3, 3, "\0bcx"sv}, {1, 8, 1, "\0x"sv}};
    for (const auto& example : examples) {
        const auto label = Label(example.capacity, example.limit);
        const auto run = Convert(text, example.capacity, example.limit);
        RequireRun(run, 34, Failed, label);
        RequireSource(run, text + example.consumed, label);
        RequireOutput(run, example.output, label);
    }
}};

const Case invalidCharacter{"WcsrtombsS_InvalidCharacter_FailsWithEilseqAndClearsOutput", [] {
    const auto run = Convert(wide, 8, 8);
    RequireRun(run, 86, Failed, "invalid character");
    RequireSource(run, wide, "invalid character");
    RequireOutput(run, "a\0xx"sv, "invalid character");
}};

const Case zeroCapacity{"WcsrtombsS_ZeroCapacity_FailsWithEinvalWithoutWriting", [] {
    const auto run = Convert(text, 0, 8);
    RequireRun(run, 22, Failed, "zero capacity");
    RequireSource(run, text, "zero capacity");
    RequireEqual(run.out[0], 'x', "zero capacity output untouched");
}};

const Case hugeCapacity{"WcsrtombsS_HugeCapacity_FailsWithEinvalWithoutWriting", [] {
    const auto run = Convert(text, Huge, 8);
    RequireRun(run, 22, Failed, "huge capacity");
    RequireEqual(run.out[0], 'x', "huge capacity output untouched");
}};

const Case hugeLimit{"WcsrtombsS_HugeLimit_FailsWithEinvalAndClearsOutput", [] {
    const auto run = Convert(text, 8, Huge);
    RequireRun(run, 22, Failed, "huge limit");
    RequireSource(run, text, "huge limit");
    RequireEqual(run.out[0], '\0', "huge limit output cleared");
}};

const Case nullSourceString{"WcsrtombsS_NullSourceString_FailsWithEinvalAndClearsOutput", [] {
    const auto run = Convert(nullptr, 8, 8);
    RequireRun(run, 22, Failed, "null source string");
    RequireEqual(run.out[0], '\0', "null source string output cleared");
}};

const Case nullResult{"WcsrtombsS_NullResultPointer_FailsWithEinvalAndClearsOutput", [] {
    char out[4] = {'x', 'x', 'x', 'x'};
    const std::uint16_t* source = text;
    std::uint64_t state[2] = {};
    RequireEqual(wcsrtombs_s_nid_postfix(nullptr, out, 4, &source, 4, state), 22, "status");
    RequireEqual(out[0], '\0', "output cleared");
    RequireEqual(source == text, true, "source kept");
}};

const Case nullSourcePointer{"WcsrtombsS_NullSourcePointer_FailsWithEinvalAndClearsOutput", [] {
    char out[4] = {'x', 'x', 'x', 'x'};
    std::size_t result = 7;
    std::uint64_t state[2] = {};
    RequireEqual(wcsrtombs_s_nid_postfix(&result, out, 4, nullptr, 4, state), 22, "status");
    RequireEqual(result, Failed, "result");
    RequireEqual(out[0], '\0', "output cleared");
}};

const Case nullState{"WcsrtombsS_NullState_FailsWithEinvalAndClearsOutput", [] {
    char out[4] = {'x', 'x', 'x', 'x'};
    std::size_t result = 7;
    const std::uint16_t* source = text;
    RequireEqual(wcsrtombs_s_nid_postfix(&result, out, 4, &source, 4, nullptr), 22, "status");
    RequireEqual(result, Failed, "result");
    RequireEqual(source == text, true, "source kept");
    RequireEqual(std::string_view(out, 4), "\0xxx"sv, "only the first byte cleared");
}};

} // namespace
