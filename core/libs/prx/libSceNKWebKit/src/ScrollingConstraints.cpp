// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/scrolling/ScrollingConstraints.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/scrolling/ScrollingConstraints.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/include/ScrollingConstraintsExports.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::FixedPositionViewportConstraints;
using NKWebKit::WebCoreAbi::StickyPositionViewportConstraints;
using NKWebKit::WebCoreAbi::ViewportConstraints;
using WebCore::FloatPoint;
using WebCore::FloatRect;
using WebCore::FloatSize;

enum AnchorEdgeFlags : unsigned {
    AnchorEdgeLeft = 1 << 0,
    AnchorEdgeRight = 1 << 1,
    AnchorEdgeTop = 1 << 2,
    AnchorEdgeBottom = 1 << 3,
};

bool HasAnchorEdge(const ViewportConstraints& constraints, AnchorEdgeFlags flag) {
    return constraints.anchorEdges & flag;
}

float MaxX(const FloatRect& rect) {
    return rect.x() + rect.width();
}

float MaxY(const FloatRect& rect) {
    return rect.y() + rect.height();
}

FloatPoint Add(FloatPoint point, FloatSize size) {
    return {point.m_x + size.m_width, point.m_y + size.m_height};
}

FloatPoint Subtract(FloatPoint point, FloatSize size) {
    return {point.m_x - size.m_width, point.m_y - size.m_height};
}

template<typename T> const T& Checked(const T* value, const char* message) {
    if (!value) throw std::invalid_argument(message);
    return *value;
}

FloatSize ComputeStickyOffset(const StickyPositionViewportConstraints& constraints, const FloatRect& constrainingRect) {
    FloatRect boxRect = constraints.stickyBoxRect;
    const FloatRect& stickyBoxRect = constraints.stickyBoxRect;
    const FloatRect& containingBlockRect = constraints.containingBlockRect;

    if (HasAnchorEdge(constraints, AnchorEdgeRight)) {
        float rightLimit = MaxX(constrainingRect) - constraints.rightOffset;
        float rightDelta = std::min<float>(0, rightLimit - MaxX(stickyBoxRect));
        float availableSpace = std::min<float>(0, containingBlockRect.x() - stickyBoxRect.x());
        if (rightDelta < availableSpace)
            rightDelta = availableSpace;
        boxRect.m_location.m_x += rightDelta;
    }

    if (HasAnchorEdge(constraints, AnchorEdgeLeft)) {
        float leftLimit = constrainingRect.x() + constraints.leftOffset;
        float leftDelta = std::max<float>(0, leftLimit - stickyBoxRect.x());
        float availableSpace = std::max<float>(0, MaxX(containingBlockRect) - MaxX(stickyBoxRect));
        if (leftDelta > availableSpace)
            leftDelta = availableSpace;
        boxRect.m_location.m_x += leftDelta;
    }

    if (HasAnchorEdge(constraints, AnchorEdgeBottom)) {
        float bottomLimit = MaxY(constrainingRect) - constraints.bottomOffset;
        float bottomDelta = std::min<float>(0, bottomLimit - MaxY(stickyBoxRect));
        float availableSpace = std::min<float>(0, containingBlockRect.y() - stickyBoxRect.y());
        if (bottomDelta < availableSpace)
            bottomDelta = availableSpace;
        boxRect.m_location.m_y += bottomDelta;
    }

    if (HasAnchorEdge(constraints, AnchorEdgeTop)) {
        float topLimit = constrainingRect.y() + constraints.topOffset;
        float topDelta = std::max<float>(0, topLimit - stickyBoxRect.y());
        float availableSpace = std::max<float>(0, MaxY(containingBlockRect) - MaxY(stickyBoxRect));
        if (topDelta > availableSpace)
            topDelta = availableSpace;
        boxRect.m_location.m_y += topDelta;
    }

    return {boxRect.x() - stickyBoxRect.x(), boxRect.y() - stickyBoxRect.y()};
}

}  // namespace

extern "C" {

FloatPoint APS5_VABI _ZNK7WebCore32FixedPositionViewportConstraints28layerPositionForViewportRectERKNS_9FloatRectE(
    const FixedPositionViewportConstraints* self, const FloatRect* viewportRect) {
    const auto& constraints = Checked(self, "WebCore::FixedPositionViewportConstraints::layerPositionForViewportRect requires constraints");
    const FloatRect& viewport = Checked(viewportRect, "WebCore::FixedPositionViewportConstraints::layerPositionForViewportRect requires a rect");
    const FloatRect& lastLayout = constraints.viewportRectAtLastLayout;
    FloatSize offset;

    if (HasAnchorEdge(constraints, AnchorEdgeLeft))
        offset.m_width = viewport.x() - lastLayout.x();
    else if (HasAnchorEdge(constraints, AnchorEdgeRight))
        offset.m_width = MaxX(viewport) - MaxX(lastLayout);

    if (HasAnchorEdge(constraints, AnchorEdgeTop))
        offset.m_height = viewport.y() - lastLayout.y();
    else if (HasAnchorEdge(constraints, AnchorEdgeBottom))
        offset.m_height = MaxY(viewport) - MaxY(lastLayout);

    return Add(constraints.layerPositionAtLastLayout, offset);
}

FloatPoint APS5_VABI _ZNK7WebCore33StickyPositionViewportConstraints32layerPositionForConstrainingRectERKNS_9FloatRectE(
    const StickyPositionViewportConstraints* self, const FloatRect* constrainingRect) {
    const auto& constraints = Checked(self, "WebCore::StickyPositionViewportConstraints::layerPositionForConstrainingRect requires constraints");
    const FloatRect& rect = Checked(constrainingRect, "WebCore::StickyPositionViewportConstraints::layerPositionForConstrainingRect requires a rect");
    FloatSize offset = ComputeStickyOffset(constraints, rect);
    return Subtract(Add(constraints.layerPositionAtLastLayout, offset), constraints.stickyOffsetAtLastLayout);
}

}
