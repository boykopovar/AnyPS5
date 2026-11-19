// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/RoundedRect.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2003, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Xidorn Quan (quanxunzhen@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/RoundedRectRegionExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreLayoutUnit.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {

using WebCore::IntPoint;
using WebCore::IntRect;
using WebCore::IntSize;
using WebCore::LayoutPoint;
using WebCore::LayoutRect;
using WebCore::LayoutSize;
using WebCore::LayoutUnit;

constexpr int FractionalBits = 6;
constexpr int FixedPointDenominator = 1 << FractionalBits;
constexpr int IntMaxForLayoutUnit = std::numeric_limits<int>::max() / FixedPointDenominator;
constexpr int IntMinForLayoutUnit = std::numeric_limits<int>::min() / FixedPointDenominator;
constexpr unsigned MaximumCount = 20;

LayoutUnit Add(LayoutUnit a, LayoutUnit b) {
    return LayoutUnit{WebCore::SaturatedSum(a.m_value, b.m_value)};
}

LayoutUnit Subtract(LayoutUnit a, LayoutUnit b) {
    return LayoutUnit{WebCore::SaturatedDifference(a.m_value, b.m_value)};
}

LayoutUnit FromDouble(double value) {
    return LayoutUnit{WebCore::ClampToInteger(value * FixedPointDenominator)};
}

double ToDouble(LayoutUnit value) {
    return static_cast<double>(value.m_value) / FixedPointDenominator;
}

int Floor(LayoutUnit value) {
    if (value.m_value <= std::numeric_limits<int>::min() + FixedPointDenominator - 1)
        return IntMinForLayoutUnit;
    return value.m_value >> FractionalBits;
}

int Ceil(LayoutUnit value) {
    if (value.m_value >= std::numeric_limits<int>::max() - FixedPointDenominator + 1)
        return IntMaxForLayoutUnit;
    if (value.m_value >= 0)
        return (value.m_value + FixedPointDenominator - 1) / FixedPointDenominator;
    return value.m_value / FixedPointDenominator;
}

int Round(LayoutUnit value) {
    return WebCore::SaturatedSum(value.m_value, FixedPointDenominator / 2) >> FractionalBits;
}

bool IsEmpty(const LayoutSize& size) {
    return size.m_width.m_value <= 0 || size.m_height.m_value <= 0;
}

bool IsZero(const LayoutSize& size) {
    return size.m_width.m_value == 0 && size.m_height.m_value == 0;
}

IntRect EnclosingIntRect(const LayoutRect& rect) {
    IntPoint location{Floor(rect.m_location.m_x), Floor(rect.m_location.m_y)};
    int maxX = rect.m_size.m_width.m_value ? Ceil(Add(rect.m_location.m_x, rect.m_size.m_width)) : location.m_x;
    int maxY = rect.m_size.m_height.m_value ? Ceil(Add(rect.m_location.m_y, rect.m_size.m_height)) : location.m_y;
    return IntRect{location, IntSize{maxX - location.m_x, maxY - location.m_y}};
}

IntRect MakeIntRect(const LayoutPoint& a, const LayoutPoint& b) {
    LayoutPoint topLeft{LayoutUnit{std::min(a.m_x.m_value, b.m_x.m_value)}, LayoutUnit{std::min(a.m_y.m_value, b.m_y.m_value)}};
    LayoutPoint bottomRight{LayoutUnit{std::max(a.m_x.m_value, b.m_x.m_value)}, LayoutUnit{std::max(a.m_y.m_value, b.m_y.m_value)}};
    LayoutRect rect{topLeft, LayoutSize{Subtract(bottomRight.m_x, topLeft.m_x), Subtract(bottomRight.m_y, topLeft.m_y)}};
    return EnclosingIntRect(rect);
}

void UniteRect(WebCore::Region& region, const IntRect& rect) {
    WebCore::Region rectRegion;
    rectRegion.m_bounds = rect;
    _ZN7WebCore6Region5uniteERKS0_(&region, rectRegion);
}

void SubtractRect(WebCore::Region& region, const IntRect& rect) {
    WebCore::Region rectRegion;
    rectRegion.m_bounds = rect;
    _ZN7WebCore6Region8subtractERKS0_(&region, rectRegion);
}

void SubtractCornerRects(WebCore::Region& region, unsigned stepLength, const LayoutPoint& corner, const LayoutPoint& ellipsisCenter, const LayoutSize& axes, double fromAngle) {
    if (stepLength == 0)
        throw std::domain_error("WebCore::approximateAsRegion: a step length of 0 divides by zero");
    double toAngle = fromAngle + std::numbers::pi / 2;
    int arcLengthFactor = Round(LayoutUnit{std::min(axes.m_width.m_value, axes.m_height.m_value)});
    unsigned count = (static_cast<unsigned>(arcLengthFactor) + (stepLength / 2)) / stepLength;
    count = std::min(MaximumCount, count);
    for (unsigned i = 0; i < count; ++i) {
        double angle = fromAngle + (i + 1) * (toAngle - fromAngle) / (count + 1);
        LayoutPoint ellipsisPoint{FromDouble(ToDouble(axes.m_width) * std::cos(angle)), FromDouble(ToDouble(axes.m_height) * std::sin(angle))};
        LayoutPoint point{Add(ellipsisCenter.m_x, ellipsisPoint.m_x), Add(ellipsisCenter.m_y, ellipsisPoint.m_y)};
        SubtractRect(region, MakeIntRect(corner, point));
    }
}

}  // namespace

extern "C" {

WebCore::Region APS5_VABI _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(const WebCore::RoundedRect& roundedRect, unsigned stepLength) {
    WebCore::RoundedRect source;
    if (!GuestCopyIn_nid_no_patch(&source, &roundedRect, sizeof(source)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);

    WebCore::Region region;
    const LayoutRect& rect = source.m_rect;
    if (IsEmpty(rect.m_size))
        return region;

    UniteRect(region, EnclosingIntRect(rect));

    const WebCore::RoundedRect::Radii& radii = source.m_radii;
    if (IsZero(radii.m_topLeft) && IsZero(radii.m_topRight) && IsZero(radii.m_bottomLeft) && IsZero(radii.m_bottomRight))
        return region;

    LayoutUnit minX = rect.m_location.m_x;
    LayoutUnit minY = rect.m_location.m_y;
    LayoutUnit maxX = Add(minX, rect.m_size.m_width);
    LayoutUnit maxY = Add(minY, rect.m_size.m_height);

    {
        LayoutPoint corner{maxX, maxY};
        const LayoutSize& axes = radii.m_bottomRight;
        LayoutPoint ellipsisCenter{Subtract(corner.m_x, axes.m_width), Subtract(corner.m_y, axes.m_height)};
        SubtractCornerRects(region, stepLength, corner, ellipsisCenter, axes, 0);
    }

    {
        LayoutPoint corner{minX, maxY};
        const LayoutSize& axes = radii.m_bottomLeft;
        LayoutPoint ellipsisCenter{Add(corner.m_x, axes.m_width), Subtract(corner.m_y, axes.m_height)};
        SubtractCornerRects(region, stepLength, corner, ellipsisCenter, axes, std::numbers::pi / 2);
    }

    {
        LayoutPoint corner{minX, minY};
        const LayoutSize& axes = radii.m_topLeft;
        LayoutPoint ellipsisCenter{Add(corner.m_x, axes.m_width), Add(corner.m_y, axes.m_height)};
        SubtractCornerRects(region, stepLength, corner, ellipsisCenter, axes, std::numbers::pi);
    }

    {
        LayoutPoint corner{maxX, minY};
        const LayoutSize& axes = radii.m_topRight;
        LayoutPoint ellipsisCenter{Subtract(corner.m_x, axes.m_width), Add(corner.m_y, axes.m_height)};
        SubtractCornerRects(region, stepLength, corner, ellipsisCenter, axes, std::numbers::pi * 3 / 2);
    }

    return region;
}

}
