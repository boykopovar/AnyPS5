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
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include <climits>

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

}
