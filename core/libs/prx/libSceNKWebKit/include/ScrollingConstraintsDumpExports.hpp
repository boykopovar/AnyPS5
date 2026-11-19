#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"
#include <cstddef>

namespace WebCore {

struct AbsolutePositionConstraints {
    FloatSize m_alignmentOffset;
    FloatPoint m_layerPositionAtLastLayout;
};

struct FixedPositionViewportConstraints {
    const void* m_vtable;
    FloatSize m_alignmentOffset;
    unsigned m_anchorEdges;
    FloatRect m_viewportRectAtLastLayout;
    FloatPoint m_layerPositionAtLastLayout;
};

struct StickyPositionViewportConstraints {
    const void* m_vtable;
    FloatSize m_alignmentOffset;
    unsigned m_anchorEdges;
    float m_leftOffset;
    float m_rightOffset;
    float m_topOffset;
    float m_bottomOffset;
    FloatRect m_constrainingRectAtLastLayout;
    FloatRect m_containingBlockRect;
    FloatRect m_stickyBoxRect;
    FloatSize m_stickyOffsetAtLastLayout;
    FloatPoint m_layerPositionAtLastLayout;
};

static_assert(sizeof(AbsolutePositionConstraints) == 16);
static_assert(offsetof(AbsolutePositionConstraints, m_layerPositionAtLastLayout) == 8);
static_assert(offsetof(FixedPositionViewportConstraints, m_alignmentOffset) == 8);
static_assert(offsetof(FixedPositionViewportConstraints, m_anchorEdges) == 16);
static_assert(offsetof(FixedPositionViewportConstraints, m_viewportRectAtLastLayout) == 20);
static_assert(offsetof(FixedPositionViewportConstraints, m_layerPositionAtLastLayout) == 36);
static_assert(sizeof(FixedPositionViewportConstraints) == 48);
static_assert(offsetof(StickyPositionViewportConstraints, m_leftOffset) == 20);
static_assert(offsetof(StickyPositionViewportConstraints, m_constrainingRectAtLastLayout) == 36);
static_assert(offsetof(StickyPositionViewportConstraints, m_containingBlockRect) == 52);
static_assert(offsetof(StickyPositionViewportConstraints, m_stickyBoxRect) == 68);
static_assert(offsetof(StickyPositionViewportConstraints, m_stickyOffsetAtLastLayout) == 84);
static_assert(offsetof(StickyPositionViewportConstraints, m_layerPositionAtLastLayout) == 92);
static_assert(sizeof(StickyPositionViewportConstraints) == 104);

}  // namespace WebCore

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_27AbsolutePositionConstraintsE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::AbsolutePositionConstraints& constraints);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_32FixedPositionViewportConstraintsE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::FixedPositionViewportConstraints& constraints);
NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_33StickyPositionViewportConstraintsE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::StickyPositionViewportConstraints& constraints);

}
