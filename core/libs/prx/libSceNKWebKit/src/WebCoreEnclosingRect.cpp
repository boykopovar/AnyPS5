// Derived from Source/WebCore/platform/graphics/FloatRect.cpp@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Source/WebCore/platform/graphics/LayoutRect.cpp@e12a212a531aeddc11259e8515671cf5188e038d
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/EnclosingRectExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreLayoutUnit.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include <cmath>
#include <limits>

namespace {

constexpr int kFixedPointDenominator = 64;

int LayoutUnitFloor(int value) {
    if (value <= std::numeric_limits<int>::min() + kFixedPointDenominator - 1) {
        return std::numeric_limits<int>::min() / kFixedPointDenominator;
    }
    if (value >= 0) {
        return value / kFixedPointDenominator;
    }
    return (value - kFixedPointDenominator + 1) / kFixedPointDenominator;
}

int LayoutUnitCeil(int value) {
    if (value >= std::numeric_limits<int>::max() - kFixedPointDenominator + 1) {
        return std::numeric_limits<int>::max() / kFixedPointDenominator;
    }
    if (value >= 0) {
        return (value + kFixedPointDenominator - 1) / kFixedPointDenominator;
    }
    return value / kFixedPointDenominator;
}

}

extern "C" {

WebCore::IntRect APS5_VABI _ZN7WebCore16enclosingIntRectERKNS_9FloatRectE(const WebCore::FloatRect& rect) {
    float minX = static_cast<float>(WebCore::ClampToInteger(std::floor(rect.x())));
    float minY = static_cast<float>(WebCore::ClampToInteger(std::floor(rect.y())));
    float maxX = static_cast<float>(WebCore::ClampToInteger(std::ceil(rect.x() + rect.width())));
    float maxY = static_cast<float>(WebCore::ClampToInteger(std::ceil(rect.y() + rect.height())));
    return WebCore::IntRect{
        {WebCore::ClampToInteger(minX), WebCore::ClampToInteger(minY)},
        {WebCore::ClampToInteger(maxX - minX), WebCore::ClampToInteger(maxY - minY)}};
}

WebCore::IntRect APS5_VABI _ZN7WebCore16enclosingIntRectERKNS_10LayoutRectE(const WebCore::LayoutRect& rect) {
    int x = rect.m_location.m_x.m_value;
    int y = rect.m_location.m_y.m_value;
    int width = rect.m_size.m_width.m_value;
    int height = rect.m_size.m_height.m_value;
    int minX = LayoutUnitFloor(x);
    int minY = LayoutUnitFloor(y);
    int maxX = width ? LayoutUnitCeil(WebCore::SaturatedSum(x, width)) : minX;
    int maxY = height ? LayoutUnitCeil(WebCore::SaturatedSum(y, height)) : minY;
    return WebCore::IntRect{{minX, minY}, {maxX - minX, maxY - minY}};
}

WebCore::LayoutRect APS5_VABI _ZN7WebCore19enclosingLayoutRectERKNS_9FloatRectE(const WebCore::FloatRect& rect) {
    int minX = WebCore::ClampToInteger(std::floor(rect.x() * kFixedPointDenominator));
    int minY = WebCore::ClampToInteger(std::floor(rect.y() * kFixedPointDenominator));
    int maxX = WebCore::ClampToInteger(std::ceil((rect.x() + rect.width()) * kFixedPointDenominator));
    int maxY = WebCore::ClampToInteger(std::ceil((rect.y() + rect.height()) * kFixedPointDenominator));
    return WebCore::LayoutRect{
        {{minX}, {minY}},
        {{WebCore::SaturatedDifference(maxX, minX)}, {WebCore::SaturatedDifference(maxY, minY)}}};
}

WebCore::FloatRect APS5_VABI _ZN7WebCore25encloseRectToDevicePixelsERKNS_9FloatRectEf(
    const WebCore::FloatRect& rect, float deviceScaleFactor) {
    float minX = std::floor(rect.x() * deviceScaleFactor) / deviceScaleFactor;
    float minY = std::floor(rect.y() * deviceScaleFactor) / deviceScaleFactor;
    float maxX = std::ceil((rect.x() + rect.width()) * deviceScaleFactor) / deviceScaleFactor;
    float maxY = std::ceil((rect.y() + rect.height()) * deviceScaleFactor) / deviceScaleFactor;
    return WebCore::FloatRect{{minX, minY}, {maxX - minX, maxY - minY}};
}

}
