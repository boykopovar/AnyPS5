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
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstring>
#include <stdexcept>

#include <stdexcept>

namespace {

using WebCore::LegacyLength;
using WebCore::LegacyLengthType;

float LengthValue(const LegacyLength& length) {
    return length.m_isFloat ? length.m_floatValue : static_cast<float>(length.m_intValue);
}

float PerspectiveValue(const WebCore::PerspectiveTransformOperation* self) {
    const LegacyLength& length = self->m_p;
    switch (length.m_type) {
    case LegacyLengthType::Fixed:
        return LengthValue(length);
    case LegacyLengthType::Percent:
        return static_cast<float>(1.0f * LengthValue(length) / 100.0f);
    case LegacyLengthType::FillAvailable:
    case LegacyLengthType::Auto:
        return 1.0f;
    case LegacyLengthType::Calculated:
        throw std::invalid_argument("calculated WebCore length");
    default:
        return 0.0f;
    }
}

constexpr std::uint32_t PerspectiveOperationType = 19;

struct OperationVtable {
    void (APS5_VABI* completeDestructor)(WebCore::PerspectiveTransformOperation*);
    void (APS5_VABI* deletingDestructor)(WebCore::PerspectiveTransformOperation*);
    WebCore::TransformOperation** (APS5_VABI* clone)(WebCore::TransformOperation**, const WebCore::PerspectiveTransformOperation*);
    bool (APS5_VABI* equals)(const WebCore::PerspectiveTransformOperation*, const WebCore::TransformOperation&);
    bool (APS5_VABI* isIdentity)(const WebCore::PerspectiveTransformOperation*);
    bool (APS5_VABI* apply)(const WebCore::PerspectiveTransformOperation*, WebCore::TransformationMatrix&, const WebCore::FloatSize&);
    int (APS5_VABI* blend)();
    bool (APS5_VABI* isAffectedByTransformOrigin)(const WebCore::PerspectiveTransformOperation*);
    bool (APS5_VABI* isRepresentableIn2D)(const WebCore::PerspectiveTransformOperation*);
    int (APS5_VABI* dump)();
};

struct OperationVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    OperationVtable dispatch;
};

const OperationVtableStorage PerspectiveVtable = {
    0,
    nullptr,
    {
        &_ZN7WebCore29PerspectiveTransformOperationD1Ev,
        &_ZN7WebCore29PerspectiveTransformOperationD0Ev,
        &_ZNK7WebCore29PerspectiveTransformOperation5cloneEv,
        &_ZNK7WebCore29PerspectiveTransformOperationeqERKNS_18TransformOperationE,
        &_ZNK7WebCore29PerspectiveTransformOperation10isIdentityEv,
        &_ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE,
        &_ZN7WebCore29PerspectiveTransformOperation5blendEPKNS_18TransformOperationEdb,
        &_ZNK7WebCore29PerspectiveTransformOperation27isAffectedByTransformOriginEv,
        &_ZNK7WebCore29PerspectiveTransformOperation19isRepresentableIn2DEv,
        &_ZNK7WebCore29PerspectiveTransformOperation4dumpERN3WTF10TextStreamE,
    },
};

void Construct(WebCore::PerspectiveTransformOperation* self, const LegacyLength* p) {
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation: null this");
    if (!p) throw std::invalid_argument("WebCore::PerspectiveTransformOperation: null length");
    if (p->m_type == LegacyLengthType::Calculated) throw std::invalid_argument("calculated WebCore length");
    const LegacyLength length = *p;
    std::memset(self, 0, sizeof(*self));
    self->m_base.m_vptr = const_cast<OperationVtable*>(&PerspectiveVtable.dispatch);
    self->m_base.m_refCount = 1;
    self->m_base.m_type = PerspectiveOperationType;
    self->m_p = length;
}

WebCore::PerspectiveTransformOperation* Allocate(const LegacyLength* p) {
    if (!p) throw std::invalid_argument("WebCore::PerspectiveTransformOperation: null length");
    if (p->m_type == LegacyLengthType::Calculated) throw std::invalid_argument("calculated WebCore length");
    auto* operation = static_cast<WebCore::PerspectiveTransformOperation*>(_ZN3WTF10fastMallocEm(sizeof(WebCore::PerspectiveTransformOperation)));
    Construct(operation, p);
    return operation;
}

void Destroy(WebCore::PerspectiveTransformOperation* self) {
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation: null this");
    if (self->m_p.m_type == LegacyLengthType::Calculated) throw std::invalid_argument("calculated WebCore length");
}

}

extern "C" {

WebCore::PerspectiveTransformOperation** APS5_VABI _ZN7WebCore29PerspectiveTransformOperation6createERKNS_6LengthE(WebCore::PerspectiveTransformOperation** result, const LegacyLength* p) {
    if (!result) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::create: null result");
    *result = Allocate(p);
    return result;
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationC1ERKNS_6LengthE(WebCore::PerspectiveTransformOperation* self, const LegacyLength* p) {
    Construct(self, p);
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationC2ERKNS_6LengthE(WebCore::PerspectiveTransformOperation* self, const LegacyLength* p) {
    Construct(self, p);
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationD1Ev(WebCore::PerspectiveTransformOperation* self) {
    Destroy(self);
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationD2Ev(WebCore::PerspectiveTransformOperation* self) {
    Destroy(self);
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationD0Ev(WebCore::PerspectiveTransformOperation* self) {
    Destroy(self);
    _ZN3WTF8fastFreeEPv(self);
}

WebCore::TransformOperation** APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation5cloneEv(WebCore::TransformOperation** result, const WebCore::PerspectiveTransformOperation* self) {
    if (!result) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::clone: null result");
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::clone: null this");
    *result = &Allocate(&self->m_p)->m_base;
    return result;
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation10isIdentityEv(const WebCore::PerspectiveTransformOperation* self) {
    return !PerspectiveValue(self);
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation27isAffectedByTransformOriginEv(const WebCore::PerspectiveTransformOperation* self) {
    return !_ZNK7WebCore29PerspectiveTransformOperation10isIdentityEv(self);
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation19isRepresentableIn2DEv(const WebCore::PerspectiveTransformOperation*) {
    return false;
}

LegacyLength* APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation11perspectiveEv(LegacyLength* result, const WebCore::PerspectiveTransformOperation* self) {
    if (self->m_p.m_type == LegacyLengthType::Calculated) NotImplemented_nid_no_patch(__func__);
    *result = self->m_p;
    return result;
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::PerspectiveTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize&) {
    _ZN7WebCore20TransformationMatrix16applyPerspectiveEd(&transform, PerspectiveValue(self));
    return false;
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperationeqERKNS_18TransformOperationE(const WebCore::PerspectiveTransformOperation* self, const WebCore::TransformOperation& other) {
    if (other.m_type != self->m_base.m_type) return false;
    const LegacyLength& left = self->m_p;
    const LegacyLength& right = reinterpret_cast<const WebCore::PerspectiveTransformOperation&>(other).m_p;
    if (left.m_type != right.m_type || left.m_hasQuirk != right.m_hasQuirk) return false;
    if (left.m_type == LegacyLengthType::Undefined) return true;
    if (left.m_type == LegacyLengthType::Calculated) NotImplemented_nid_no_patch(__func__);
    return LengthValue(left) == LengthValue(right);
}

}
