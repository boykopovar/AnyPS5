// Derived from Source/WebCore/page/LocalFrameView.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

// Derived from Source/WebCore/platform/ScrollableArea.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2008, 2011, 2014-2016 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Source/WebCore/platform/graphics/LayoutRect.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Source/WebCore/platform/LayoutUnit.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (c) 2012-2014, Google Inc. All rights reserved.
 * Copyright (c) 2012-2023, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreLayoutUnit.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include <algorithm>
#include <limits>

namespace WebCore {

namespace {

constexpr int FixedPointDenominator = 64;

LayoutUnit FromInt(int value) {
    if (value > std::numeric_limits<int>::max() / FixedPointDenominator) {
        return LayoutUnit{std::numeric_limits<int>::max()};
    }
    if (value < std::numeric_limits<int>::min() / FixedPointDenominator) {
        return LayoutUnit{std::numeric_limits<int>::min()};
    }
    return LayoutUnit{value * FixedPointDenominator};
}

LayoutUnit FromDouble(double value) {
    return LayoutUnit{ClampToInteger(value * FixedPointDenominator)};
}

float ToFloat(LayoutUnit value) {
    return static_cast<float>(value.m_value) / FixedPointDenominator;
}

double ToDouble(LayoutUnit value) {
    return static_cast<double>(value.m_value) / FixedPointDenominator;
}

LayoutUnit Add(LayoutUnit a, LayoutUnit b) {
    return LayoutUnit{SaturatedSum(a.m_value, b.m_value)};
}

LayoutUnit Subtract(LayoutUnit a, LayoutUnit b) {
    return LayoutUnit{SaturatedDifference(a.m_value, b.m_value)};
}

LayoutUnit Negate(LayoutUnit value) {
    return LayoutUnit{static_cast<int>(0u - static_cast<unsigned>(value.m_value))};
}

LayoutUnit Min(LayoutUnit a, LayoutUnit b) {
    return b.m_value < a.m_value ? b : a;
}

LayoutUnit Max(LayoutUnit a, LayoutUnit b) {
    return a.m_value < b.m_value ? b : a;
}

LayoutUnit MaxX(const LayoutRect& rect) {
    return Add(rect.m_location.m_x, rect.m_size.m_width);
}

LayoutUnit MaxY(const LayoutRect& rect) {
    return Add(rect.m_location.m_y, rect.m_size.m_height);
}

float ClampFloat(float value, float min, float max) {
    if (value >= max) {
        return max;
    }
    if (value <= min) {
        return min;
    }
    return value;
}

void Intersect(LayoutRect& rect, const LayoutRect& other) {
    LayoutUnit left = Max(rect.m_location.m_x, other.m_location.m_x);
    LayoutUnit top = Max(rect.m_location.m_y, other.m_location.m_y);
    LayoutUnit right = Min(MaxX(rect), MaxX(other));
    LayoutUnit bottom = Min(MaxY(rect), MaxY(other));
    if (left.m_value >= right.m_value || top.m_value >= bottom.m_value) {
        left = top = right = bottom = LayoutUnit{};
    }
    rect.m_location = LayoutPoint{left, top};
    rect.m_size = LayoutSize{Subtract(right, left), Subtract(bottom, top)};
}

LayoutPoint ConstrainScrollPositionForOverhang(const LayoutRect& visibleContentRect, const LayoutSize& totalContentsSize,
    const LayoutPoint& scrollPosition, const LayoutPoint& scrollOrigin, int headerHeight, int footerHeight) {
    LayoutSize idealScrollRectSize{Min(visibleContentRect.m_size.m_width, totalContentsSize.m_width),
        Min(visibleContentRect.m_size.m_height, totalContentsSize.m_height)};

    LayoutRect scrollRect{
        LayoutPoint{Subtract(Add(scrollPosition.m_x, scrollOrigin.m_x), FromInt(0)),
            Subtract(Add(scrollPosition.m_y, scrollOrigin.m_y), FromInt(headerHeight))},
        idealScrollRectSize};
    LayoutRect documentRect{LayoutPoint{},
        LayoutSize{totalContentsSize.m_width,
            Subtract(Subtract(totalContentsSize.m_height, FromInt(headerHeight)), FromInt(footerHeight))}};

    Intersect(scrollRect, documentRect);

    if (scrollRect.m_size.m_width.m_value != idealScrollRectSize.m_width.m_value
        || scrollRect.m_size.m_height.m_value != idealScrollRectSize.m_height.m_value) {
        scrollRect.m_size = idealScrollRectSize;

        Intersect(scrollRect, documentRect);
        if (scrollRect.m_size.m_width.m_value < idealScrollRectSize.m_width.m_value) {
            scrollRect.m_location.m_x = Add(scrollRect.m_location.m_x,
                Negate(Subtract(idealScrollRectSize.m_width, scrollRect.m_size.m_width)));
        }
        if (scrollRect.m_size.m_height.m_value < idealScrollRectSize.m_height.m_value) {
            scrollRect.m_location.m_y = Add(scrollRect.m_location.m_y,
                Negate(Subtract(idealScrollRectSize.m_height, scrollRect.m_size.m_height)));
        }
    }

    return LayoutPoint{Subtract(scrollRect.m_location.m_x, scrollOrigin.m_x),
        Subtract(scrollRect.m_location.m_y, scrollOrigin.m_y)};
}

LayoutUnit LayoutViewportOriginAxis(LayoutUnit visual, LayoutUnit visualExtent, LayoutUnit layout, LayoutUnit layoutExtent,
    LayoutUnit stableMin, LayoutUnit stableMax, bool allowRubberBanding) {
    LayoutUnit origin = layout;
    if (visualExtent.m_value > layoutExtent.m_value) {
        origin = visual;
        if (!allowRubberBanding) {
            if (origin.m_value < stableMin.m_value) {
                origin = stableMin;
            } else if (origin.m_value > stableMax.m_value) {
                origin = stableMax;
            }
        }
        return origin;
    }

    LayoutUnit visualMax = Add(visual, visualExtent);
    LayoutUnit visualMaxOrigin = Subtract(visualMax, layoutExtent);
    bool rubberBandingAtStart = allowRubberBanding && visual.m_value < stableMin.m_value;
    bool rubberBandingAtEnd = allowRubberBanding && visualMaxOrigin.m_value > stableMax.m_value;

    if (visual.m_value < layout.m_value || rubberBandingAtStart) {
        origin = visual;
    }
    if (visualMax.m_value > Add(layout, layoutExtent).m_value || rubberBandingAtEnd) {
        origin = visualMaxOrigin;
    }
    if (!rubberBandingAtStart && origin.m_value < stableMin.m_value) {
        origin = stableMin;
    }
    if (!rubberBandingAtEnd && origin.m_value > stableMax.m_value) {
        origin = stableMax;
    }
    return origin;
}

float YPositionForHeaderLayer(const FloatPoint& scrollPosition, float topContentInset) {
    if (!topContentInset) {
        return 0;
    }
    float scrollY = std::max<float>(0, scrollPosition.m_y);
    if (scrollY >= topContentInset) {
        return topContentInset;
    }
    return scrollY;
}

}

}

extern "C" {

WebCore::LayoutPoint APS5_VABI _ZN7WebCore9FrameView27computeLayoutViewportOriginERKNS_10LayoutRectERKNS_11LayoutPointES6_S3_NS_30ScrollBehaviorForFixedElementsE(
    const WebCore::LayoutRect& visualViewport, const WebCore::LayoutPoint& stableLayoutViewportOriginMin,
    const WebCore::LayoutPoint& stableLayoutViewportOriginMax, const WebCore::LayoutRect& layoutViewport,
    WebCore::ScrollBehaviorForFixedElements fixedBehavior) {
    bool allowRubberBanding = fixedBehavior == WebCore::ScrollBehaviorForFixedElements::StickToViewportBounds;
    return WebCore::LayoutPoint{
        WebCore::LayoutViewportOriginAxis(visualViewport.m_location.m_x, visualViewport.m_size.m_width,
            layoutViewport.m_location.m_x, layoutViewport.m_size.m_width,
            stableLayoutViewportOriginMin.m_x, stableLayoutViewportOriginMax.m_x, allowRubberBanding),
        WebCore::LayoutViewportOriginAxis(visualViewport.m_location.m_y, visualViewport.m_size.m_height,
            layoutViewport.m_location.m_y, layoutViewport.m_size.m_height,
            stableLayoutViewportOriginMin.m_y, stableLayoutViewportOriginMax.m_y, allowRubberBanding)};
}

WebCore::LayoutRect APS5_VABI _ZN7WebCore9FrameView32computeUpdatedLayoutViewportRectERKNS_10LayoutRectES3_RKNS_10LayoutSizeES3_S6_RKNS_11LayoutPointES9_NS0_24LayoutViewportConstraintE(
    const WebCore::LayoutRect& layoutViewport, const WebCore::LayoutRect& documentRect,
    const WebCore::LayoutSize& unobscuredContentSize, const WebCore::LayoutRect& unobscuredContentRect,
    const WebCore::LayoutSize& baseLayoutViewportSize, const WebCore::LayoutPoint& stableLayoutViewportOriginMin,
    const WebCore::LayoutPoint& stableLayoutViewportOriginMax, WebCore::LayoutViewportConstraint constraint) {
    WebCore::LayoutRect layoutViewportRect = layoutViewport;
    layoutViewportRect.m_size = WebCore::LayoutSize{
        WebCore::Max(baseLayoutViewportSize.m_width, unobscuredContentSize.m_width),
        WebCore::Max(baseLayoutViewportSize.m_height, unobscuredContentSize.m_height)};

    WebCore::LayoutPoint layoutViewportOrigin = _ZN7WebCore9FrameView27computeLayoutViewportOriginERKNS_10LayoutRectERKNS_11LayoutPointES6_S3_NS_30ScrollBehaviorForFixedElementsE(
        unobscuredContentRect, stableLayoutViewportOriginMin, stableLayoutViewportOriginMax, layoutViewportRect,
        WebCore::ScrollBehaviorForFixedElements::StickToViewportBounds);

    if (constraint == WebCore::LayoutViewportConstraint::ConstrainedToDocumentRect) {
        layoutViewportOrigin.m_x = WebCore::FromDouble(WebCore::ClampFloat(WebCore::ToFloat(layoutViewportOrigin.m_x), 0,
            WebCore::ToFloat(WebCore::Subtract(documentRect.m_size.m_width, layoutViewportRect.m_size.m_width))));
        layoutViewportOrigin.m_y = WebCore::FromDouble(WebCore::ClampFloat(WebCore::ToFloat(layoutViewportOrigin.m_y), 0,
            WebCore::ToFloat(WebCore::Subtract(documentRect.m_size.m_height, layoutViewportRect.m_size.m_height))));
    }
    layoutViewportRect.m_location = layoutViewportOrigin;
    return layoutViewportRect;
}

WebCore::LayoutSize APS5_VABI _ZN7WebCore9FrameView26expandedLayoutViewportSizeERKNS_10LayoutSizeES3_d(
    const WebCore::LayoutSize& baseLayoutViewportSize, const WebCore::LayoutSize& documentSize, double heightExpansionFactor) {
    if (!heightExpansionFactor) {
        return baseLayoutViewportSize;
    }
    WebCore::LayoutUnit documentHeight = documentSize.m_height;
    WebCore::LayoutUnit layoutViewportHeight = baseLayoutViewportSize.m_height;
    if (layoutViewportHeight.m_value > documentHeight.m_value) {
        return baseLayoutViewportSize;
    }
    WebCore::LayoutUnit expandedHeight = WebCore::FromDouble((1 + heightExpansionFactor) * WebCore::ToDouble(layoutViewportHeight));
    return WebCore::LayoutSize{baseLayoutViewportSize.m_width, WebCore::Min(documentHeight, expandedHeight)};
}

WebCore::LayoutPoint APS5_VABI _ZN7WebCore9FrameView30scrollPositionForFixedPositionERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_fbNS_30ScrollBehaviorForFixedElementsEii(
    const WebCore::LayoutRect& visibleContentRect, const WebCore::LayoutSize& totalContentsSize,
    const WebCore::LayoutPoint& scrollPosition, const WebCore::LayoutPoint& scrollOrigin, float frameScaleFactor,
    bool fixedElementsLayoutRelativeToFrame, WebCore::ScrollBehaviorForFixedElements behaviorForFixed, int headerHeight,
    int footerHeight) {
    WebCore::LayoutPoint position;
    if (behaviorForFixed == WebCore::ScrollBehaviorForFixedElements::StickToDocumentBounds) {
        position = WebCore::ConstrainScrollPositionForOverhang(visibleContentRect, totalContentsSize, scrollPosition,
            scrollOrigin, headerHeight, footerHeight);
    } else {
        position = scrollPosition;
        position.m_y = WebCore::Subtract(position.m_y, WebCore::FromInt(headerHeight));
    }

    WebCore::LayoutSize maxSize{WebCore::Subtract(totalContentsSize.m_width, visibleContentRect.m_size.m_width),
        WebCore::Subtract(totalContentsSize.m_height, visibleContentRect.m_size.m_height)};

    float dragFactorX = (fixedElementsLayoutRelativeToFrame || !maxSize.m_width.m_value)
        ? 1
        : (WebCore::ToFloat(totalContentsSize.m_width) - WebCore::ToFloat(visibleContentRect.m_size.m_width) * frameScaleFactor)
            / WebCore::ToFloat(maxSize.m_width);
    float dragFactorY = (fixedElementsLayoutRelativeToFrame || !maxSize.m_height.m_value)
        ? 1
        : (WebCore::ToFloat(totalContentsSize.m_height) - WebCore::ToFloat(visibleContentRect.m_size.m_height) * frameScaleFactor)
            / WebCore::ToFloat(maxSize.m_height);

    float x = WebCore::ToFloat(position.m_x) * dragFactorX / frameScaleFactor;
    float y = WebCore::ToFloat(position.m_y) * dragFactorY / frameScaleFactor;
    return WebCore::LayoutPoint{WebCore::FromDouble(x), WebCore::FromDouble(y)};
}

WebCore::FloatPoint APS5_VABI _ZN7WebCore9FrameView27positionForRootContentLayerERKNS_10FloatPointES3_ff(
    const WebCore::FloatPoint& scrollPosition, const WebCore::FloatPoint& scrollOrigin, float topContentInset, float headerHeight) {
    float y = WebCore::YPositionForHeaderLayer(scrollPosition, topContentInset) + headerHeight;
    return WebCore::FloatPoint{0.0f - scrollOrigin.m_x, y - scrollOrigin.m_y};
}

}
