// Derived from Source/WebCore/platform/graphics/RoundedRect.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/EnclosingRectExports.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreLayoutUnit.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace WebCore {

static_assert(sizeof(LayoutUnit) == 4);
static_assert(sizeof(LayoutPoint) == 8);
static_assert(sizeof(LayoutSize) == 8);
static_assert(sizeof(LayoutRect) == 16);
static_assert(sizeof(RoundedRect::Radii) == 32);
static_assert(sizeof(RoundedRect) == 48);
static_assert(offsetof(RoundedRect, m_rect) == 0);
static_assert(offsetof(RoundedRect, m_radii) == 16);
static_assert(offsetof(RoundedRect::Radii, m_topLeft) == 0);
static_assert(offsetof(RoundedRect::Radii, m_topRight) == 8);
static_assert(offsetof(RoundedRect::Radii, m_bottomLeft) == 16);
static_assert(offsetof(RoundedRect::Radii, m_bottomRight) == 24);

namespace {

constexpr int kLayoutUnitFractionalBits = 6;
constexpr int kFixedPointDenominator = 1 << kLayoutUnitFractionalBits;
constexpr double kPiDouble = 3.14159265358979323846;

bool IsRectEmpty(const LayoutRect& rect) {
    return rect.m_size.m_width.m_value <= 0 || rect.m_size.m_height.m_value <= 0;
}

bool IsSizeZero(const LayoutSize& size) {
    return size.m_width.m_value == 0 && size.m_height.m_value == 0;
}

bool IsRounded(const RoundedRect& roundedRect) {
    return !IsSizeZero(roundedRect.m_radii.m_topLeft)
        || !IsSizeZero(roundedRect.m_radii.m_topRight)
        || !IsSizeZero(roundedRect.m_radii.m_bottomLeft)
        || !IsSizeZero(roundedRect.m_radii.m_bottomRight);
}

int RoundToInt(LayoutUnit value) {
    return SaturatedSum(value.m_value, kFixedPointDenominator / 2) >> kLayoutUnitFractionalBits;
}

LayoutUnit LayoutUnitFromDouble(double value) {
    return LayoutUnit{ClampToInteger(value * kFixedPointDenominator)};
}

double LayoutUnitToDouble(LayoutUnit value) {
    return static_cast<double>(value.m_value) / kFixedPointDenominator;
}

LayoutPoint AddPoints(LayoutPoint a, LayoutPoint b) {
    return LayoutPoint{
        LayoutUnit{SaturatedSum(a.m_x.m_value, b.m_x.m_value)},
        LayoutUnit{SaturatedSum(a.m_y.m_value, b.m_y.m_value)}
    };
}

IntRect MakeCornerIntRect(LayoutPoint a, LayoutPoint b) {
    const int minX = std::min(a.m_x.m_value, b.m_x.m_value);
    const int minY = std::min(a.m_y.m_value, b.m_y.m_value);
    const int maxX = std::max(a.m_x.m_value, b.m_x.m_value);
    const int maxY = std::max(a.m_y.m_value, b.m_y.m_value);
    const LayoutRect layoutRect{
        LayoutPoint{LayoutUnit{minX}, LayoutUnit{minY}},
        LayoutSize{LayoutUnit{SaturatedDifference(maxX, minX)}, LayoutUnit{SaturatedDifference(maxY, minY)}}
    };
    return _ZN7WebCore16enclosingIntRectERKNS_10LayoutRectE(layoutRect);
}

}  // namespace

}  // namespace WebCore

extern "C" {

WebCore::Region APS5_VABI _ZN7WebCore19approximateAsRegionERKNS_11RoundedRectEj(
    const WebCore::RoundedRect& roundedRect, unsigned stepLength) {
    const int savedErrno = errno;
    WebCore::RoundedRect source{};
    const bool copied = GuestCopyIn_nid_no_patch(&source, &roundedRect, sizeof(source));
    errno = savedErrno;
    if (!copied) {
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }

    WebCore::Region region;
    if (WebCore::IsRectEmpty(source.m_rect)) {
        return region;
    }

    WebCore::Region boundsRegion;
    boundsRegion.m_bounds = _ZN7WebCore16enclosingIntRectERKNS_10LayoutRectE(source.m_rect);
    _ZN7WebCore6Region5uniteERKS0_(&region, boundsRegion);

    if (!WebCore::IsRounded(source)) {
        return region;
    }

    if (stepLength == 0) {
        throw std::invalid_argument("WebCore::approximateAsRegion: zero stepLength");
    }

    auto subtractCornerRects = [&](WebCore::LayoutPoint corner, WebCore::LayoutPoint ellipsisCenter,
                                   WebCore::LayoutSize axes, double fromAngle) {
        const double toAngle = fromAngle + WebCore::kPiDouble / 2;
        const int arcLengthFactor = WebCore::RoundToInt(
            WebCore::LayoutUnit{std::min(axes.m_width.m_value, axes.m_height.m_value)});
        unsigned count = (static_cast<unsigned>(arcLengthFactor) + (stepLength / 2)) / stepLength;
        constexpr unsigned maximumCount = 20u;
        count = std::min(maximumCount, count);

        for (unsigned i = 0u; i < count; ++i) {
            const double angle = fromAngle + (i + 1) * (toAngle - fromAngle) / (count + 1);
            const WebCore::LayoutPoint ellipsisPoint{
                WebCore::LayoutUnitFromDouble(WebCore::LayoutUnitToDouble(axes.m_width) * std::cos(angle)),
                WebCore::LayoutUnitFromDouble(WebCore::LayoutUnitToDouble(axes.m_height) * std::sin(angle))
            };
            WebCore::Region cornerRegion;
            cornerRegion.m_bounds = WebCore::MakeCornerIntRect(corner, WebCore::AddPoints(ellipsisCenter, ellipsisPoint));
            _ZN7WebCore6Region8subtractERKS0_(&region, cornerRegion);
        }
    };

    const WebCore::LayoutUnit minX = source.m_rect.m_location.m_x;
    const WebCore::LayoutUnit minY = source.m_rect.m_location.m_y;
    const WebCore::LayoutUnit maxX{WebCore::SaturatedSum(minX.m_value, source.m_rect.m_size.m_width.m_value)};
    const WebCore::LayoutUnit maxY{WebCore::SaturatedSum(minY.m_value, source.m_rect.m_size.m_height.m_value)};

    {
        const WebCore::LayoutPoint corner{maxX, maxY};
        const WebCore::LayoutSize axes = source.m_radii.m_bottomRight;
        const WebCore::LayoutPoint ellipsisCenter{
            WebCore::LayoutUnit{WebCore::SaturatedDifference(corner.m_x.m_value, axes.m_width.m_value)},
            WebCore::LayoutUnit{WebCore::SaturatedDifference(corner.m_y.m_value, axes.m_height.m_value)}
        };
        subtractCornerRects(corner, ellipsisCenter, axes, 0.0);
    }

    {
        const WebCore::LayoutPoint corner{minX, maxY};
        const WebCore::LayoutSize axes = source.m_radii.m_bottomLeft;
        const WebCore::LayoutPoint ellipsisCenter{
            WebCore::LayoutUnit{WebCore::SaturatedSum(corner.m_x.m_value, axes.m_width.m_value)},
            WebCore::LayoutUnit{WebCore::SaturatedDifference(corner.m_y.m_value, axes.m_height.m_value)}
        };
        subtractCornerRects(corner, ellipsisCenter, axes, WebCore::kPiDouble / 2);
    }

    {
        const WebCore::LayoutPoint corner{minX, minY};
        const WebCore::LayoutSize axes = source.m_radii.m_topLeft;
        const WebCore::LayoutPoint ellipsisCenter{
            WebCore::LayoutUnit{WebCore::SaturatedSum(corner.m_x.m_value, axes.m_width.m_value)},
            WebCore::LayoutUnit{WebCore::SaturatedSum(corner.m_y.m_value, axes.m_height.m_value)}
        };
        subtractCornerRects(corner, ellipsisCenter, axes, WebCore::kPiDouble);
    }

    {
        const WebCore::LayoutPoint corner{maxX, minY};
        const WebCore::LayoutSize axes = source.m_radii.m_topRight;
        const WebCore::LayoutPoint ellipsisCenter{
            WebCore::LayoutUnit{WebCore::SaturatedDifference(corner.m_x.m_value, axes.m_width.m_value)},
            WebCore::LayoutUnit{WebCore::SaturatedSum(corner.m_y.m_value, axes.m_height.m_value)}
        };
        subtractCornerRects(corner, ellipsisCenter, axes, WebCore::kPiDouble * 3 / 2);
    }

    return region;
}

}
