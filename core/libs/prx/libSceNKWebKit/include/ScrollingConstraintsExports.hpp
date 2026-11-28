#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct FixedPositionViewportConstraints {
    const void* vptr;
    WebCore::FloatSize alignmentOffset;
    unsigned anchorEdges;
    WebCore::FloatRect viewportRectAtLastLayout;
    WebCore::FloatPoint layerPositionAtLastLayout;
};

struct StickyPositionViewportConstraints {
    const void* vptr;
    WebCore::FloatSize alignmentOffset;
    unsigned anchorEdges;
    float leftOffset;
    float rightOffset;
    float topOffset;
    float bottomOffset;
    WebCore::FloatRect constrainingRectAtLastLayout;
    WebCore::FloatRect containingBlockRect;
    WebCore::FloatRect stickyBoxRect;
    WebCore::FloatSize stickyOffsetAtLastLayout;
    WebCore::FloatPoint layerPositionAtLastLayout;
};

static_assert(std::is_standard_layout_v<FixedPositionViewportConstraints>);
static_assert(sizeof(FixedPositionViewportConstraints) == 48);
static_assert(offsetof(FixedPositionViewportConstraints, alignmentOffset) == 8);
static_assert(offsetof(FixedPositionViewportConstraints, anchorEdges) == 16);
static_assert(offsetof(FixedPositionViewportConstraints, viewportRectAtLastLayout) == 20);
static_assert(offsetof(FixedPositionViewportConstraints, layerPositionAtLastLayout) == 36);
static_assert(std::is_standard_layout_v<StickyPositionViewportConstraints>);
static_assert(sizeof(StickyPositionViewportConstraints) == 104);
static_assert(offsetof(StickyPositionViewportConstraints, alignmentOffset) == 8);
static_assert(offsetof(StickyPositionViewportConstraints, anchorEdges) == 16);
static_assert(offsetof(StickyPositionViewportConstraints, leftOffset) == 20);
static_assert(offsetof(StickyPositionViewportConstraints, constrainingRectAtLastLayout) == 36);
static_assert(offsetof(StickyPositionViewportConstraints, containingBlockRect) == 52);
static_assert(offsetof(StickyPositionViewportConstraints, stickyBoxRect) == 68);
static_assert(offsetof(StickyPositionViewportConstraints, stickyOffsetAtLastLayout) == 84);
static_assert(offsetof(StickyPositionViewportConstraints, layerPositionAtLastLayout) == 92);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

WebCore::FloatPoint APS5_VABI _ZNK7WebCore32FixedPositionViewportConstraints28layerPositionForViewportRectERKNS_9FloatRectE(
    const NKWebKit::WebCoreAbi::FixedPositionViewportConstraints* self, const WebCore::FloatRect* viewportRect);
WebCore::FloatPoint APS5_VABI _ZNK7WebCore33StickyPositionViewportConstraints32layerPositionForConstrainingRectERKNS_9FloatRectE(
    const NKWebKit::WebCoreAbi::StickyPositionViewportConstraints* self, const WebCore::FloatRect* constrainingRect);

}
