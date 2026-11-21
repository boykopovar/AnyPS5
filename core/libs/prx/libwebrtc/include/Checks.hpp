#pragma once

#include <cstdint>

enum class WebRtcCheckArgType : std::int8_t {
    End = 0,
    Int,
    Long,
    LongLong,
    UInt,
    ULong,
    ULongLong,
    Double,
    LongDouble,
    CharP,
    StdString,
    StringView,
    VoidP,
    CheckOp,
};
