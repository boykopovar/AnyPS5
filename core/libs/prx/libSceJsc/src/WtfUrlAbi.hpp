#pragma once

#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::WtfAbi {

struct URL {
    const StringImpl* string;
    unsigned bits;
    unsigned userStart;
    unsigned userEnd;
    unsigned passwordEnd;
    unsigned hostEnd;
    unsigned pathAfterLastSlash;
    unsigned pathEnd;
    unsigned queryEnd;
};

inline constexpr unsigned URLValidBit = 1u;
inline constexpr unsigned URLPortLengthShift = 3;
inline constexpr unsigned URLPortLengthMask = 7u;
inline constexpr unsigned URLSchemeEndShift = 6;

struct StringBuilder {
    const StringImpl* string;
    const StringImpl* buffer;
    const void* bufferCharacters;
    bool overflowed;
    bool shouldCrashOnOverflow;
    std::int32_t length;
    bool is8Bit;
};

struct OptionalUInt16 {
    bool hasValue{};
    std::uint16_t value{};

    OptionalUInt16() = default;
    OptionalUInt16(bool engaged, std::uint16_t number) : hasValue(engaged), value(number) {}
    OptionalUInt16(const OptionalUInt16& other) : hasValue(other.hasValue), value(other.value) {}
    OptionalUInt16& operator=(const OptionalUInt16& other) = default;
};

enum class ClockType : std::int32_t {
    Wall,
    Monotonic,
    Approximate,
};

struct Seconds {
    double value;
};

struct TimeWithDynamicClockType {
    double value;
    ClockType type;
};

static_assert(sizeof(URL) == 40);
static_assert(offsetof(URL, string) == 0);
static_assert(offsetof(URL, bits) == 8);
static_assert(offsetof(URL, passwordEnd) == 20);
static_assert(offsetof(URL, hostEnd) == 24);
static_assert(offsetof(URL, pathEnd) == 32);
static_assert(offsetof(URL, queryEnd) == 36);
static_assert(sizeof(StringBuilder) == 40);
static_assert(offsetof(StringBuilder, string) == 0);
static_assert(offsetof(StringBuilder, buffer) == 8);
static_assert(offsetof(StringBuilder, bufferCharacters) == 16);
static_assert(offsetof(StringBuilder, overflowed) == 24);
static_assert(offsetof(StringBuilder, shouldCrashOnOverflow) == 25);
static_assert(offsetof(StringBuilder, length) == 28);
static_assert(offsetof(StringBuilder, is8Bit) == 32);
static_assert(sizeof(OptionalUInt16) == 4);
static_assert(offsetof(OptionalUInt16, hasValue) == 0);
static_assert(offsetof(OptionalUInt16, value) == 2);
static_assert(sizeof(Seconds) == 8);
static_assert(sizeof(TimeWithDynamicClockType) == 16);
static_assert(offsetof(TimeWithDynamicClockType, type) == 8);

} // namespace Jsc::WtfAbi
