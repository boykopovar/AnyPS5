#include "prx/libSceNpCommon/include/Api.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename TValue, typename TFormat>
static void Check(TValue value, std::uint32_t radix, std::uint32_t flags, TFormat format) {
    std::array<char, 100> reference{};
    const auto converted = std::to_chars(reference.data(), reference.data() + reference.size(), value, radix);
    Require(converted.ec == std::errc{});
    std::string expected(reference.data(), converted.ptr);
    std::string sign;
    if (expected.front() == '-') { sign = "-"; expected.erase(0, 1); }
    else if (value != 0 && (flags & 4)) sign = "+";
    if (flags & 1) {
        for (char& c : expected) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    if ((flags & 2) && (radix == 8 || radix == 16)) expected.insert(0, radix == 8 ? "0" : (flags & 1) ? "0X" : "0x");
    expected.insert(0, sign);
    std::size_t length = 999;
    Require(format(nullptr, 0, value, radix, flags, &length) == 0 && length == expected.size());
    std::array<char, 110> output{};
    for (const auto capacity : {length, length + 1}) {
        output.fill('!');
        std::size_t actual = 777;
        const int status = format(output.data() + 1, capacity, value, radix, flags, &actual);
        if (capacity == length) {
            Require(status == static_cast<int>(0x80558008) && actual == 777);
            Require(std::all_of(output.begin(), output.end(), [](char c) { return c == '!'; }));
        } else {
            Require(status == 0 && actual == expected.size());
            Require(std::string(output.data() + 1) == expected);
            Require(output.front() == '!' && output[capacity + 1] == '!');
        }
    }
}

static void CheckUnwritableOutputs() {
    constexpr int InvalidArgument = static_cast<int>(0x80558009);
    constexpr int BufferTooSmall = static_cast<int>(0x80558008);
    auto* const unwritableText = reinterpret_cast<char*>(std::uintptr_t{0x10});
    auto* const unwritableLength = reinterpret_cast<std::size_t*>(std::uintptr_t{0x10});
    std::size_t length = 999;
    Require(sceNpUInt64ToStr(unwritableText, 8, 42, 10, 0, &length) == InvalidArgument && length == 999);
    Require(sceNpUInt32ToStr(unwritableText, 8, 42, 10, 0, &length) == InvalidArgument && length == 999);
    Require(sceNpInt32ToStr(unwritableText, 8, -42, 10, 0, &length) == InvalidArgument && length == 999);
    Require(sceNpInt64ToStr(unwritableText, 8, -42, 10, 4, &length) == InvalidArgument && length == 999);
    Require(sceNpUInt64ToStr(unwritableText, 2, 42, 10, 0, &length) == BufferTooSmall && length == 999);
    Require(sceNpUInt64ToStr(nullptr, 0, 42, 10, 0, unwritableLength) == InvalidArgument);
    std::array<char, 8> output{};
    Require(sceNpUInt64ToStr(output.data(), output.size(), 42, 10, 0, unwritableLength) == InvalidArgument);
    Require(std::string(output.data()) == "42");
}

int main() {
    CheckUnwritableOutputs();
    for (std::uint32_t radix = 2; radix <= 36; ++radix) {
        for (std::uint32_t flags = 0; flags < 8; ++flags) {
            for (std::uint64_t value : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{35}, std::numeric_limits<std::uint64_t>::max()})
                Check(value, radix, flags, sceNpUInt64ToStr);
            for (std::uint32_t value : {std::uint32_t{0}, std::uint32_t{1}, std::numeric_limits<std::uint32_t>::max()})
                Check(value, radix, flags, sceNpUInt32ToStr);
            for (std::int32_t value : {0, 1, -1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()})
                Check(value, radix, flags, sceNpInt32ToStr);
            for (std::int64_t value : {std::int64_t{0}, std::int64_t{1}, std::numeric_limits<std::int64_t>::max()})
                Check(value, radix, flags, sceNpInt64ToStr);
            if (flags & 4) {
                Check(std::int64_t{-1}, radix, flags, sceNpInt64ToStr);
                Check(std::numeric_limits<std::int64_t>::min(), radix, flags, sceNpInt64ToStr);
            }
        }
    }
}
