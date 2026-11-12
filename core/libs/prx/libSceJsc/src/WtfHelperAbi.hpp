#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace Jsc::WtfAbi {

using LChar = std::uint8_t;

enum HexConversionMode { Lowercase, Uppercase };

using HexResult = std::pair<LChar*, unsigned>;

using SixCharacterHash = std::array<char, 7>;

struct MediaTime {
    enum : std::uint8_t {
        Valid = 1 << 0,
        HasBeenRounded = 1 << 1,
        PositiveInfinite = 1 << 2,
        NegativeInfinite = 1 << 3,
        Indefinite = 1 << 4,
        DoubleValue = 1 << 5,
    };

    enum ComparisonFlags {
        LessThan = -1,
        EqualTo = 0,
        GreaterThan = 1,
    };

    union {
        std::int64_t timeValue;
        double timeValueAsDouble;
    };
    std::uint32_t timeScale;
    std::uint8_t timeFlags;
};

static_assert(sizeof(HexConversionMode) == 4);
static_assert(sizeof(HexResult) == 16);
static_assert(offsetof(HexResult, first) == 0);
static_assert(offsetof(HexResult, second) == 8);
static_assert(std::is_trivially_copy_constructible_v<HexResult>);
static_assert(std::is_trivially_move_constructible_v<HexResult>);
static_assert(std::is_trivially_destructible_v<HexResult>);
static_assert(sizeof(SixCharacterHash) == 7);
static_assert(std::is_trivially_copyable_v<SixCharacterHash>);
static_assert(sizeof(MediaTime) == 16);
static_assert(alignof(MediaTime) == 8);
static_assert(offsetof(MediaTime, timeValue) == 0);
static_assert(offsetof(MediaTime, timeScale) == 8);
static_assert(offsetof(MediaTime, timeFlags) == 12);

} // namespace Jsc::WtfAbi
