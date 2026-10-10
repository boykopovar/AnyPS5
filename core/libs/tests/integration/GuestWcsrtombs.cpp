#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

extern "C" std::size_t APS5_VABI wcsrtombs_nid_postfix(char*, const std::uint16_t**, std::size_t, void*);
extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

using namespace std::string_view_literals;
using Testing::Case;
using Testing::RequireEqual;

constexpr auto Failed = static_cast<std::size_t>(-1);
constexpr std::uint16_t text[] = {'a', 'b', 'c', 0};
constexpr std::uint16_t empty[] = {0};
constexpr std::uint16_t invalid[] = {'a', 0x100, 'b', 0};
constexpr std::uint16_t highBytes[] = {0x7f, 0x80, 0xe9, 0xff, 0};

struct Output {
    Output() { std::memset(bytes, 'x', sizeof(bytes)); }

    std::string_view View(std::size_t size = sizeof(bytes)) const { return std::string_view(bytes, size); }

    char bytes[8];
};

void RequireSource(const std::uint16_t* actual, const std::uint16_t* expected, const char* message) {
    RequireEqual(actual == expected, true, message);
}

const Case fullConversion{"Wcsrtombs_FullConversion_WritesTerminatorAndKeepsErrno", [] {
    std::uint64_t state[2]{};
    Output out;
    const auto* source = text;
    *__error_nid_postfix() = 7;
    RequireEqual(wcsrtombs_nid_postfix(out.bytes, &source, sizeof(out.bytes), state), std::size_t{3}, "full conversion length");
    RequireSource(source, nullptr, "full conversion consumes the source");
    RequireEqual(out.View(), "abc\0xxxx"sv, "full conversion and terminator");
    RequireEqual(*__error_nid_postfix(), 7, "successful conversion preserves errno");
}};

const Case zeroCapacity{"Wcsrtombs_ZeroCapacity_LeavesInputAndOutputAlone", [] {
    Output out;
    const auto* source = text;
    RequireEqual(wcsrtombs_nid_postfix(out.bytes, &source, 0, nullptr), std::size_t{0}, "zero capacity length");
    RequireSource(source, text, "zero capacity leaves input alone");
    RequireEqual(out.bytes[0], 'x', "zero capacity leaves output alone");
}};

const Case resumedConversion{"Wcsrtombs_LimitedCapacity_ResumesUntilTerminated", [] {
    Output out;
    const auto* source = text;
    RequireEqual(wcsrtombs_nid_postfix(out.bytes, &source, 2, nullptr), std::size_t{2}, "partial conversion length");
    RequireSource(source, text + 2, "partial conversion source");
    RequireEqual(out.View(), "abxxxxxx"sv, "partial conversion has no terminator");
    RequireEqual(wcsrtombs_nid_postfix(out.bytes + 2, &source, 1, nullptr), std::size_t{1}, "exact capacity length");
    RequireSource(source, text + 3, "exact capacity source");
    RequireEqual(out.View(), "abcxxxxx"sv, "exact capacity leaves terminator pending");
    RequireEqual(wcsrtombs_nid_postfix(out.bytes + 3, &source, 1, nullptr), std::size_t{0}, "resume at terminator");
    RequireSource(source, nullptr, "resumed conversion consumes the source");
    RequireEqual(out.View(), "abc\0xxxx"sv, "resumed conversion terminates");
}};

const Case emptyString{"Wcsrtombs_EmptyString_WritesOnlyTerminator", [] {
    std::uint64_t state[2]{};
    Output out;
    const auto* source = empty;
    RequireEqual(wcsrtombs_nid_postfix(out.bytes, &source, 1, state), std::size_t{0}, "empty string length");
    RequireSource(source, nullptr, "empty string consumes the source");
    RequireEqual(out.View(2), "\0x"sv, "empty string terminator");
}};

const Case guestHighBytes{"Wcsrtombs_GuestHighBytes_ConvertToSingleBytes", [] {
    std::uint64_t state[2]{};
    Output out;
    const auto* source = highBytes;
    RequireEqual(wcsrtombs_nid_postfix(out.bytes, &source, sizeof(out.bytes), state), std::size_t{4}, "guest single-byte conversion length");
    RequireSource(source, nullptr, "guest high bytes consume the source");
    RequireEqual(out.View(5), "\x7f\x80\xe9\xff\0"sv, "guest high bytes");
}};

const Case lengthQuery{"Wcsrtombs_NullDestination_ReturnsLengthAndKeepsSource", [] {
    std::uint64_t state[2]{};
    const auto* source = text;
    RequireEqual(wcsrtombs_nid_postfix(nullptr, &source, 0, state), std::size_t{3}, "length query ignores capacity");
    RequireSource(source, text, "length query preserves source");
    RequireEqual(wcsrtombs_nid_postfix(nullptr, &source, 1, nullptr), std::size_t{3}, "length query with implicit state");
    RequireSource(source, text, "length query with implicit state preserves source");
    source = empty;
    RequireEqual(wcsrtombs_nid_postfix(nullptr, &source, 0, state), std::size_t{0}, "empty length query");
    RequireSource(source, empty, "empty length query preserves source");
}};

const Case invalidCharacter{"Wcsrtombs_InvalidCharacter_FailsWithGuestEilseq", [] {
    std::uint64_t state[2]{};
    Output out;
    const auto* source = invalid;
    *__error_nid_postfix() = 0;
    RequireEqual(wcsrtombs_nid_postfix(out.bytes, &source, sizeof(out.bytes), state), Failed, "invalid character fails conversion");
    RequireEqual(*__error_nid_postfix(), 86, "invalid character uses guest EILSEQ");
    RequireSource(source, invalid + 1, "error identifies invalid character");
    RequireEqual(out.View(), "axxxxxxx"sv, "error preserves prefix");
}};

const Case zeroCapacityAtInvalid{"Wcsrtombs_ZeroCapacityAtInvalidCharacter_ConvertsNothing", [] {
    std::uint64_t state[2]{};
    Output out;
    const auto* source = invalid + 1;
    *__error_nid_postfix() = 7;
    RequireEqual(wcsrtombs_nid_postfix(out.bytes, &source, 0, state), std::size_t{0}, "zero capacity does not convert invalid character");
    RequireSource(source, invalid + 1, "zero capacity keeps source");
    RequireEqual(*__error_nid_postfix(), 7, "zero capacity preserves errno");
}};

const Case invalidLengthQuery{"Wcsrtombs_LengthQueryWithInvalidCharacter_FailsWithGuestEilseq", [] {
    std::uint64_t state[2]{};
    const auto* source = invalid;
    RequireEqual(wcsrtombs_nid_postfix(nullptr, &source, 0, state), Failed, "length query rejects invalid character");
    RequireEqual(*__error_nid_postfix(), 86, "failed length query reports guest EILSEQ");
    RequireSource(source, invalid, "failed length query preserves source");
}};

const Case capacityBeforeInvalid{"Wcsrtombs_CapacityEndsBeforeInvalidCharacter_SucceedsWithoutErrno", [] {
    std::uint64_t state[2]{};
    Output out;
    const auto* source = invalid;
    *__error_nid_postfix() = 7;
    RequireEqual(wcsrtombs_nid_postfix(out.bytes, &source, 1, state), std::size_t{1}, "capacity stops before invalid character");
    RequireSource(source, invalid + 1, "source stops at invalid character");
    RequireEqual(*__error_nid_postfix(), 7, "unconverted invalid character does not set errno");
}};

} // namespace
