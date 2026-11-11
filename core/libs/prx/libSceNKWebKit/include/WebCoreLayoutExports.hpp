#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>
#include <cstdint>

namespace WebCore {

enum GridPositionType : int {
    AutoPosition,
    ExplicitPosition,
    SpanPosition,
    NamedGridAreaPosition
};

struct GridPosition {
    GridPositionType m_type;
    int m_integerPosition;
    NKWebKit::WtfAbi::String m_namedGridLine;
};

struct FloatQuadVector {
    const FloatQuad* m_buffer;
    unsigned m_capacity;
    unsigned m_size;
};

static_assert(sizeof(GridPosition) == 16);
static_assert(offsetof(GridPosition, m_integerPosition) == 4);
static_assert(offsetof(GridPosition, m_namedGridLine) == 8);
static_assert(offsetof(Length, m_type) == 4);
static_assert(offsetof(Length, m_hasQuirk) == 5);
static_assert(offsetof(Length, m_isFloat) == 6);
static_assert(sizeof(FloatQuadVector) == 16);
static_assert(offsetof(FloatQuadVector, m_capacity) == 8);
static_assert(offsetof(FloatQuadVector, m_size) == 12);

}  // namespace WebCore

extern "C" {

void APS5_VABI _ZN7WebCore12GridPosition15setSpanPositionEiRKN3WTF6StringE(WebCore::GridPosition* self, int position, const NKWebKit::WtfAbi::String& namedGridLine);
void APS5_VABI _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(WebCore::GridPosition* self, int position, const NKWebKit::WtfAbi::String& namedGridLine);
void APS5_VABI _ZN7WebCore12GridPosition24setMaxPositionForTestingEj(unsigned maxPosition);
WebCore::LayoutUnit APS5_VABI _ZN7WebCore14valueForLengthERKNS_6LengthENS_10LayoutUnitE(const WebCore::Length& length, WebCore::LayoutUnit maximumValue);
float APS5_VABI _ZN7WebCore19floatValueForLengthERKNS_6LengthEf(const WebCore::Length& length, float maximumValue);
WebCore::FloatRect APS5_VABI _ZN7WebCore19unitedBoundingBoxesERKN3WTF6VectorINS_9FloatQuadELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(const WebCore::FloatQuadVector& quads);

}
