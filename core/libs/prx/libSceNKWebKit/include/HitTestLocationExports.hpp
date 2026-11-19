#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>

namespace WebCore {

struct HitTestLocation {
    LayoutPoint m_point;
    IntRect m_boundingBox;
    FloatPoint m_transformedPoint;
    FloatQuad m_transformedRect;
    bool m_isRectBased{false};
    bool m_isRectilinear{true};
};

static_assert(offsetof(HitTestLocation, m_boundingBox) == 8);
static_assert(offsetof(HitTestLocation, m_transformedPoint) == 24);
static_assert(offsetof(HitTestLocation, m_transformedRect) == 32);
static_assert(offsetof(HitTestLocation, m_isRectBased) == 64);
static_assert(offsetof(HitTestLocation, m_isRectilinear) == 65);
static_assert(sizeof(HitTestLocation) == 68);

}  // namespace WebCore

extern "C" {

void APS5_VABI _ZN7WebCore15HitTestLocationC1Ev(WebCore::HitTestLocation* self);
void APS5_VABI _ZN7WebCore15HitTestLocationC2Ev(WebCore::HitTestLocation* self);
void APS5_VABI _ZN7WebCore15HitTestLocationC1ERKS0_(WebCore::HitTestLocation* self, const WebCore::HitTestLocation* other);
void APS5_VABI _ZN7WebCore15HitTestLocationC2ERKS0_(WebCore::HitTestLocation* self, const WebCore::HitTestLocation* other);
void APS5_VABI _ZN7WebCore15HitTestLocationC1ERKNS_10FloatPointE(WebCore::HitTestLocation* self, const WebCore::FloatPoint* point);
void APS5_VABI _ZN7WebCore15HitTestLocationC2ERKNS_10FloatPointE(WebCore::HitTestLocation* self, const WebCore::FloatPoint* point);

}
