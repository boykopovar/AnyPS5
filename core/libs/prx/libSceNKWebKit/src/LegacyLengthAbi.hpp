#pragma once

#include <cstddef>
#include <cstdint>

namespace WebCore {

enum class LegacyLengthType : std::uint8_t {
    Auto,
    Relative,
    Percent,
    Fixed,
    Intrinsic,
    MinIntrinsic,
    MinContent,
    MaxContent,
    FillAvailable,
    FitContent,
    Calculated,
    Undefined
};

struct LegacyLength {
    union {
        int m_intValue;
        float m_floatValue;
        unsigned m_calculationValueHandle;
    };
    bool m_hasQuirk;
    LegacyLengthType m_type;
    bool m_isFloat;
};

static_assert(sizeof(LegacyLength) == 8);
static_assert(offsetof(LegacyLength, m_hasQuirk) == 4);
static_assert(offsetof(LegacyLength, m_type) == 5);
static_assert(offsetof(LegacyLength, m_isFloat) == 6);

}
