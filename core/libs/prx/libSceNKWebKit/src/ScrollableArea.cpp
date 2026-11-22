// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/ScrollableArea.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/LayoutRect.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/LayoutUnit.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace WebCore {

namespace {

constexpr int kLayoutUnitFractionalBits = 6;
constexpr int kFixedPointDenominator = 1 << kLayoutUnitFractionalBits;
constexpr int kIntMaxForLayoutUnit = std::numeric_limits<int>::max() / kFixedPointDenominator;
constexpr int kIntMinForLayoutUnit = std::numeric_limits<int>::min() / kFixedPointDenominator;

int SaturatedAddRaw(int a, int b) {
    std::int64_t sum = static_cast<std::int64_t>(a) + static_cast<std::int64_t>(b);
    if (sum > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    if (sum < std::numeric_limits<int>::min()) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(sum);
}

int SaturatedSubRaw(int a, int b) {
    std::int64_t diff = static_cast<std::int64_t>(a) - static_cast<std::int64_t>(b);
    if (diff > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    if (diff < std::numeric_limits<int>::min()) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(diff);
}

LayoutUnit LayoutUnitFromPixel(int value) {
    if (value >= kIntMaxForLayoutUnit) {
        return LayoutUnit{std::numeric_limits<int>::max()};
    }
    if (value <= kIntMinForLayoutUnit) {
        return LayoutUnit{std::numeric_limits<int>::min()};
    }
    return LayoutUnit{value * kFixedPointDenominator};
}

LayoutUnit SaturatedAdd(LayoutUnit a, LayoutUnit b) {
    return LayoutUnit{SaturatedAddRaw(a.m_value, b.m_value)};
}

LayoutUnit SaturatedSub(LayoutUnit a, LayoutUnit b) {
    return LayoutUnit{SaturatedSubRaw(a.m_value, b.m_value)};
}

void IntersectLayoutRect(LayoutRect& rect, const LayoutRect& other) {
    int rectMaxX = SaturatedAddRaw(rect.m_location.m_x.m_value, rect.m_size.m_width.m_value);
    int rectMaxY = SaturatedAddRaw(rect.m_location.m_y.m_value, rect.m_size.m_height.m_value);
    int otherMaxX = SaturatedAddRaw(other.m_location.m_x.m_value, other.m_size.m_width.m_value);
    int otherMaxY = SaturatedAddRaw(other.m_location.m_y.m_value, other.m_size.m_height.m_value);

    int newX = std::max(rect.m_location.m_x.m_value, other.m_location.m_x.m_value);
    int newY = std::max(rect.m_location.m_y.m_value, other.m_location.m_y.m_value);
    int newMaxX = std::min(rectMaxX, otherMaxX);
    int newMaxY = std::min(rectMaxY, otherMaxY);

    if (newX >= newMaxX || newY >= newMaxY) {
        rect = LayoutRect{};
        return;
    }

    rect.m_location = LayoutPoint{LayoutUnit{newX}, LayoutUnit{newY}};
    rect.m_size = LayoutSize{
        LayoutUnit{SaturatedSubRaw(newMaxX, newX)},
        LayoutUnit{SaturatedSubRaw(newMaxY, newY)}
    };
}

}  // namespace

}  // namespace WebCore

extern "C" {

WebCore::LayoutPoint APS5_VABI _ZN7WebCore14ScrollableArea34constrainScrollPositionForOverhangERKNS_10LayoutRectERKNS_10LayoutSizeERKNS_11LayoutPointES9_ii(
    const WebCore::LayoutRect& visibleContentRect,
    const WebCore::LayoutSize& totalContentsSize,
    const WebCore::LayoutPoint& scrollPosition,
    const WebCore::LayoutPoint& scrollOrigin,
    int headerHeight,
    int footerHeight) {
    const WebCore::LayoutSize idealScrollRectSize{
        WebCore::LayoutUnit{std::min(visibleContentRect.m_size.m_width.m_value, totalContentsSize.m_width.m_value)},
        WebCore::LayoutUnit{std::min(visibleContentRect.m_size.m_height.m_value, totalContentsSize.m_height.m_value)}
    };

    const WebCore::LayoutUnit headerUnit = WebCore::LayoutUnitFromPixel(headerHeight);
    const WebCore::LayoutUnit footerUnit = WebCore::LayoutUnitFromPixel(footerHeight);

    WebCore::LayoutRect scrollRect{
        WebCore::LayoutPoint{
            WebCore::SaturatedAdd(scrollPosition.m_x, scrollOrigin.m_x),
            WebCore::SaturatedSub(WebCore::SaturatedAdd(scrollPosition.m_y, scrollOrigin.m_y), headerUnit)
        },
        idealScrollRectSize
    };

    const WebCore::LayoutRect documentRect{
        WebCore::LayoutPoint{},
        WebCore::LayoutSize{
            totalContentsSize.m_width,
            WebCore::SaturatedSub(WebCore::SaturatedSub(totalContentsSize.m_height, headerUnit), footerUnit)
        }
    };

    WebCore::IntersectLayoutRect(scrollRect, documentRect);

    if (scrollRect.m_size.m_width.m_value != idealScrollRectSize.m_width.m_value ||
        scrollRect.m_size.m_height.m_value != idealScrollRectSize.m_height.m_value) {
        scrollRect.m_size = idealScrollRectSize;
        WebCore::IntersectLayoutRect(scrollRect, documentRect);
        if (scrollRect.m_size.m_width.m_value < idealScrollRectSize.m_width.m_value) {
            scrollRect.m_location.m_x = WebCore::SaturatedSub(
                scrollRect.m_location.m_x,
                WebCore::SaturatedSub(idealScrollRectSize.m_width, scrollRect.m_size.m_width));
        }
        if (scrollRect.m_size.m_height.m_value < idealScrollRectSize.m_height.m_value) {
            scrollRect.m_location.m_y = WebCore::SaturatedSub(
                scrollRect.m_location.m_y,
                WebCore::SaturatedSub(idealScrollRectSize.m_height, scrollRect.m_size.m_height));
        }
    }

    return WebCore::LayoutPoint{
        WebCore::SaturatedSub(scrollRect.m_location.m_x, scrollOrigin.m_x),
        WebCore::SaturatedSub(scrollRect.m_location.m_y, scrollOrigin.m_y)
    };
}

}
