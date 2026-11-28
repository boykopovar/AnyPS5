// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/rendering/HitTestLocation.cpp
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/LayoutUnit.h
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
#include "prx/libSceNKWebKit/include/HitTestLocationExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreLayoutUnit.hpp"

#include <climits>
#include <cmath>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::LegacyHitTestLocation;

constexpr int kFixedPointDenominator = 64;

int FromFloatFloor(float value) {
    return WebCore::ClampToInteger(std::floor(value * kFixedPointDenominator));
}

int Floor(int value) {
    if (value <= INT_MIN + kFixedPointDenominator - 1) return INT_MIN / kFixedPointDenominator;
    if (value >= 0) return value / kFixedPointDenominator;
    return (value - kFixedPointDenominator + 1) / kFixedPointDenominator;
}

float ToFloat(int value) {
    return static_cast<float>(value) / kFixedPointDenominator;
}

void Construct(LegacyHitTestLocation* self, const WebCore::FloatPoint* point) {
    if (!self) throw std::invalid_argument("WebCore::HitTestLocation::HitTestLocation: null this");
    if (!point) throw std::invalid_argument("WebCore::HitTestLocation::HitTestLocation: null WebCore::FloatPoint");
    const int x = FromFloatFloor(point->m_x);
    const int y = FromFloatFloor(point->m_y);
    self->point.m_x.m_value = x;
    self->point.m_y.m_value = y;
    self->boundingBox.m_location.m_x = Floor(x);
    self->boundingBox.m_location.m_y = Floor(y);
    self->boundingBox.m_size.m_width = 1;
    self->boundingBox.m_size.m_height = 1;
    self->transformedPoint.m_x = ToFloat(x);
    self->transformedPoint.m_y = ToFloat(y);
    const float left = static_cast<float>(self->boundingBox.m_location.m_x);
    const float top = static_cast<float>(self->boundingBox.m_location.m_y);
    const float right = left + 1.0f;
    const float bottom = top + 1.0f;
    self->transformedRect.m_p1 = {left, top};
    self->transformedRect.m_p2 = {right, top};
    self->transformedRect.m_p3 = {right, bottom};
    self->transformedRect.m_p4 = {left, bottom};
    self->isRectBased = false;
    self->isRectilinear = true;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore15HitTestLocationC1ERKNS_10FloatPointE(LegacyHitTestLocation* self, const WebCore::FloatPoint* point) {
    Construct(self, point);
}

void APS5_VABI _ZN7WebCore15HitTestLocationC2ERKNS_10FloatPointE(LegacyHitTestLocation* self, const WebCore::FloatPoint* point) {
    Construct(self, point);
}

}
