// Derived from Source/WebCore/platform/graphics/transforms/AffineTransform.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2005, 2006 Apple Inc.  All rights reserved.
 *               2010 Dirk Schulze <krit@webkit.org>
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

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include <cmath>

namespace WebCore {

namespace {

bool IsIdentityOrTranslation(const AffineTransform& t) {
    return t.m_transform[0] == 1.0 && t.m_transform[1] == 0.0
        && t.m_transform[2] == 0.0 && t.m_transform[3] == 1.0;
}

}  // namespace
}  // namespace WebCore

extern "C" {

void APS5_VABI _ZN7WebCore15AffineTransformC1Ev(WebCore::AffineTransform* self) {
    self->m_transform = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
}

void APS5_VABI _ZN7WebCore15AffineTransformC2Ev(WebCore::AffineTransform* self) {
    _ZN7WebCore15AffineTransformC1Ev(self);
}

void APS5_VABI _ZN7WebCore15AffineTransformC1Edddddd(
    WebCore::AffineTransform* self, double a, double b, double c, double d, double e, double f) {
    self->m_transform = {a, b, c, d, e, f};
}

void APS5_VABI _ZN7WebCore15AffineTransformC2Edddddd(
    WebCore::AffineTransform* self, double a, double b, double c, double d, double e, double f) {
    _ZN7WebCore15AffineTransformC1Edddddd(self, a, b, c, d, e, f);
}

WebCore::OptionalAffineTransform APS5_VABI _ZNK7WebCore15AffineTransform7inverseEv(const WebCore::AffineTransform* self) {
    const auto& m = self->m_transform;
    WebCore::OptionalAffineTransform result{};
    const double determinant = m[0] * m[3] - m[1] * m[2];
    if (!std::isfinite(determinant) || determinant == 0.0) {
        return result;
    }
    result.m_engaged = true;
    auto& r = result.m_value.m_transform;
    if (WebCore::IsIdentityOrTranslation(*self)) {
        r[4] = -m[4];
        r[5] = -m[5];
        return result;
    }
    r[0] = m[3] / determinant;
    r[1] = -m[1] / determinant;
    r[2] = -m[2] / determinant;
    r[3] = m[0] / determinant;
    r[4] = (m[2] * m[5] - m[3] * m[4]) / determinant;
    r[5] = (m[1] * m[4] - m[0] * m[5]) / determinant;
    return result;
}

}
