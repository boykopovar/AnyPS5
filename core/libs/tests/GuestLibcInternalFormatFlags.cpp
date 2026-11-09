#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal format flags: %s\n", message);
    std::abort();
}

using MakeManipulator = StreamFlagManipulator (APS5_VABI*)(std::int32_t);

std::uint32_t Expected(std::uint32_t flags, std::uint32_t mask, bool setting) {
    std::uint32_t result = 0;
    for (unsigned bit = 0; bit < 32; ++bit) {
        const auto flag = std::uint32_t{1} << bit;
        const bool value = (mask & flag) ? setting && bit < 16 : (flags & flag) != 0;
        if (value) result |= flag;
    }
    return result;
}

void Check(MakeManipulator make, bool setting, std::uint32_t flags, std::uint32_t mask) {
    GuestLocale::IosBase stream{};
    stream.standardStream = 0x123456789abcdef;
    stream.state = 0x13579;
    stream.exceptions = 0x2468;
    stream.flags = std::bit_cast<std::int32_t>(flags);
    stream.precision = 7;
    stream.width = 19;
    stream.reserved = 0xdeadbeef;
    std::array<unsigned char, sizeof(stream)> expected{};
    std::memcpy(expected.data(), &stream, sizeof(stream));
    const auto expectedFlags = std::bit_cast<std::int32_t>(Expected(flags, mask, setting));
    std::memcpy(expected.data() + offsetof(GuestLocale::IosBase, flags), &expectedFlags, sizeof(expectedFlags));
    const auto manipulator = make(std::bit_cast<std::int32_t>(mask));
    Require(manipulator.function != nullptr, "callback was not returned");
    Require(std::bit_cast<std::uint32_t>(manipulator.argument) == mask, "mask bits were not preserved");
    manipulator.function(&stream, manipulator.argument);
    Require(std::memcmp(&stream, expected.data(), sizeof(stream)) == 0, "wrong flags or another stream field changed");
    manipulator.function(&stream, manipulator.argument);
    Require(std::memcmp(&stream, expected.data(), sizeof(stream)) == 0, "applying a manipulator twice is not idempotent");
    GuestLocale::IosBase other{};
    other.flags = std::bit_cast<std::int32_t>(~flags);
    manipulator.function(&other, manipulator.argument);
    Require(std::bit_cast<std::uint32_t>(other.flags) == Expected(~flags, mask, setting), "a saved manipulator cannot be reused on another stream");
}

void CheckOperation(MakeManipulator make, bool setting) {
    constexpr std::array<std::uint32_t, 8> patterns{0, 1, 0xffff, 0xffff0000, 0x7fffffff, 0x80000000, 0xffffffff, 0xa5a55a5a};
    for (const auto flags : patterns) {
        for (const auto mask : patterns) Check(make, setting, flags, mask);
        for (unsigned bit = 0; bit < 32; ++bit) {
            Check(make, setting, flags, std::uint32_t{1} << bit);
            Check(make, setting, flags, ~(std::uint32_t{1} << bit));
        }
    }
    std::uint32_t state = 0x193a52e1;
    for (unsigned i = 0; i < 100000; ++i) {
        state = state * 1664525u + 1013904223u;
        const auto flags = state;
        state = state * 1664525u + 1013904223u;
        Check(make, setting, flags, state);
    }
    bool threw = false;
    const auto manipulator = make(1);
    try { manipulator.function(nullptr, manipulator.argument); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw, "null stream did not throw invalid_argument");
}

}

int main() {
    CheckOperation(_ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix, true);
    CheckOperation(_ZSt13resetiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix, false);
    return 0;
}
