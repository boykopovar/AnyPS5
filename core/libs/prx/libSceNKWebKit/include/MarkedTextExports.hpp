#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::MarkedTextAbi {

inline constexpr std::uint32_t OverlapStrategyNone = 0;
inline constexpr std::uint32_t OverlapStrategyFrontmost = 1;

struct MarkedText {
    unsigned startOffset;
    unsigned endOffset;
    std::uint32_t type;
    const void* marker;
    NKWebKit::WtfAbi::StringImpl* highlightName;
};

struct MarkedTextVector {
    MarkedText* buffer;
    unsigned capacity;
    unsigned size;
};

static_assert(sizeof(MarkedText) == 32);
static_assert(offsetof(MarkedText, endOffset) == 4);
static_assert(offsetof(MarkedText, type) == 8);
static_assert(offsetof(MarkedText, marker) == 16);
static_assert(offsetof(MarkedText, highlightName) == 24);
static_assert(sizeof(MarkedTextVector) == 16);
static_assert(offsetof(MarkedTextVector, capacity) == 8);
static_assert(offsetof(MarkedTextVector, size) == 12);

}

extern "C" {

NKWebKit::MarkedTextAbi::MarkedTextVector* APS5_VABI _ZN7WebCore9subdivideERKN3WTF6VectorINS_10MarkedTextELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEENS_15OverlapStrategyE(NKWebKit::MarkedTextAbi::MarkedTextVector* result, const NKWebKit::MarkedTextAbi::MarkedTextVector* markedTexts, std::uint32_t overlapStrategy);

}
