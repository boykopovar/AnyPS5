// Derived from Source/WebCore/rendering/HitTestLocation.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2006, 2008, 2011, 2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
 *
*/
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/HitTestLocationExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include <climits>
#include <cmath>
#include <new>

namespace {

constexpr int FixedPointDenominator = 64;
constexpr int LayoutUnitFractionalBits = 6;
constexpr int IntMinForLayoutUnit = INT_MIN / FixedPointDenominator;

int Floor(WebCore::LayoutUnit value) {
    if (value.m_value <= INT_MIN + FixedPointDenominator - 1) {
        return IntMinForLayoutUnit;
    }
    return value.m_value >> LayoutUnitFractionalBits;
}

int Subtract(int value, unsigned amount) {
    return static_cast<int>(static_cast<unsigned>(value) - amount);
}

int Extent(unsigned first, unsigned second) {
    return static_cast<int>(first + second + 1u);
}

WebCore::LayoutUnit FromFloatFloor(float value, const char* caller) {
    const float scaled = std::floor(value * static_cast<float>(FixedPointDenominator));
    if (std::isnan(scaled)) {
        NotImplemented_nid_no_patch(caller);
    }
    if (scaled >= static_cast<float>(INT_MAX)) {
        return {INT_MAX};
    }
    if (scaled <= static_cast<float>(INT_MIN)) {
        return {INT_MIN};
    }
    return {static_cast<int>(scaled)};
}

float ToFloat(WebCore::LayoutUnit value) {
    return static_cast<float>(value.m_value) / FixedPointDenominator;
}

WebCore::FloatQuad QuadOf(const WebCore::IntRect& rect) {
    const float x = static_cast<float>(rect.m_location.m_x);
    const float y = static_cast<float>(rect.m_location.m_y);
    const float maxX = x + static_cast<float>(rect.m_size.m_width);
    const float maxY = y + static_cast<float>(rect.m_size.m_height);
    return {{x, y}, {maxX, y}, {maxX, maxY}, {x, maxY}};
}

}  // namespace

extern "C" {

WebCore::IntRect APS5_VABI _ZN7WebCore15HitTestLocation12rectForPointERKNS_11LayoutPointEjjjj(const WebCore::LayoutPoint& point, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding) {
    WebCore::IntRect rect;
    rect.m_location.m_x = Subtract(Floor(point.m_x), leftPadding);
    rect.m_location.m_y = Subtract(Floor(point.m_y), topPadding);
    rect.m_size.m_width = Extent(leftPadding, rightPadding);
    rect.m_size.m_height = Extent(topPadding, bottomPadding);
    return rect;
}

void APS5_VABI _ZN7WebCore15HitTestLocationC2Ev(WebCore::HitTestLocation* self) {
    if (self == nullptr) {
        NotImplemented_nid_no_patch(__func__);
    }
    new (self) WebCore::HitTestLocation{};
}

void APS5_VABI _ZN7WebCore15HitTestLocationC1Ev(WebCore::HitTestLocation* self) {
    _ZN7WebCore15HitTestLocationC2Ev(self);
}

void APS5_VABI _ZN7WebCore15HitTestLocationC2ERKS0_(WebCore::HitTestLocation* self, const WebCore::HitTestLocation* other) {
    if (self == nullptr || other == nullptr) {
        NotImplemented_nid_no_patch(__func__);
    }
    new (self) WebCore::HitTestLocation{*other};
}

void APS5_VABI _ZN7WebCore15HitTestLocationC1ERKS0_(WebCore::HitTestLocation* self, const WebCore::HitTestLocation* other) {
    _ZN7WebCore15HitTestLocationC2ERKS0_(self, other);
}

void APS5_VABI _ZN7WebCore15HitTestLocationC2ERKNS_10FloatPointE(WebCore::HitTestLocation* self, const WebCore::FloatPoint* point) {
    if (self == nullptr || point == nullptr) {
        NotImplemented_nid_no_patch(__func__);
    }
    const WebCore::LayoutPoint floored{FromFloatFloor(point->m_x, __func__), FromFloatFloor(point->m_y, __func__)};
    const WebCore::IntRect box = _ZN7WebCore15HitTestLocation12rectForPointERKNS_11LayoutPointEjjjj(floored, 0, 0, 0, 0);
    new (self) WebCore::HitTestLocation{floored, box, {ToFloat(floored.m_x), ToFloat(floored.m_y)}, QuadOf(box)};
}

void APS5_VABI _ZN7WebCore15HitTestLocationC1ERKNS_10FloatPointE(WebCore::HitTestLocation* self, const WebCore::FloatPoint* point) {
    _ZN7WebCore15HitTestLocationC2ERKNS_10FloatPointE(self, point);
}

}
