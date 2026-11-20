/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
/*
 * Derived from Source/WebCore/platform/graphics/transforms/PerspectiveTransformOperation.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/transforms/PerspectiveTransformOperation.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006, 2008, 2014 Apple Inc. All rights reserved.
    Copyright (C) 2011 Rik Cabanier (cabanier@adobe.com)
    Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    Derived from Source/WebCore/platform/Length.h@c55364d53f971f63a709976f168d932d558b8909
*/

#include "prx/libSceNKWebKit/include/PerspectiveTransformOperationExports.hpp"

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreLayoutExports.hpp"

namespace {

float PerspectiveValue(const WebCore::PerspectiveTransformOperation* self) {
    return _ZN7WebCore19floatValueForLengthERKNS_6LengthEf(self->m_p, 1.0f);
}

float LengthValue(const WebCore::Length& length) {
    return length.m_isFloat ? length.m_floatValue : static_cast<float>(length.m_intValue);
}

}

extern "C" {

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation10isIdentityEv(const WebCore::PerspectiveTransformOperation* self) {
    return !PerspectiveValue(self);
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation27isAffectedByTransformOriginEv(const WebCore::PerspectiveTransformOperation* self) {
    return !_ZNK7WebCore29PerspectiveTransformOperation10isIdentityEv(self);
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation19isRepresentableIn2DEv(const WebCore::PerspectiveTransformOperation*) {
    return false;
}

WebCore::Length* APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation11perspectiveEv(WebCore::Length* result, const WebCore::PerspectiveTransformOperation* self) {
    if (self->m_p.isCalculated()) NotImplemented_nid_no_patch(__func__);
    *result = self->m_p;
    return result;
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::PerspectiveTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize&) {
    _ZN7WebCore20TransformationMatrix16applyPerspectiveEd(&transform, PerspectiveValue(self));
    return false;
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperationeqERKNS_18TransformOperationE(const WebCore::PerspectiveTransformOperation* self, const WebCore::TransformOperation& other) {
    if (other.m_type != self->m_base.m_type) return false;
    const WebCore::Length& left = self->m_p;
    const WebCore::Length& right = reinterpret_cast<const WebCore::PerspectiveTransformOperation&>(other).m_p;
    if (left.m_type != right.m_type || left.m_hasQuirk != right.m_hasQuirk) return false;
    if (left.m_type == WebCore::LengthType::Undefined) return true;
    if (left.isCalculated()) NotImplemented_nid_no_patch(__func__);
    return LengthValue(left) == LengthValue(right);
}

}
