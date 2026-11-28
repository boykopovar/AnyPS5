#pragma once

#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct LegacyHitTestLocation {
    WebCore::LayoutPoint point;
    WebCore::IntRect boundingBox;
    WebCore::FloatPoint transformedPoint;
    WebCore::FloatQuad transformedRect;
    bool isRectBased;
    bool isRectilinear;
};

static_assert(sizeof(LegacyHitTestLocation) == 68);
static_assert(offsetof(LegacyHitTestLocation, boundingBox) == 8);
static_assert(offsetof(LegacyHitTestLocation, transformedPoint) == 24);
static_assert(offsetof(LegacyHitTestLocation, transformedRect) == 32);
static_assert(offsetof(LegacyHitTestLocation, isRectBased) == 64);
static_assert(offsetof(LegacyHitTestLocation, isRectilinear) == 65);

}  // namespace NKWebKit::WebCoreAbi
