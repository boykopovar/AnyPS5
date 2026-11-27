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
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace {

using WebCore::LegacyLength;
using WebCore::LegacyLengthType;

static_assert(std::is_trivially_copyable_v<LegacyLength>);
static_assert(std::is_trivially_copyable_v<WebCore::LegacyTransformOperation>);
static_assert(std::is_trivially_copyable_v<WebCore::PerspectiveTransformOperation>);
static_assert(std::is_trivially_copyable_v<WebCore::TransformationMatrix>);

template <typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
void RequireWritableGuestValue(T* pointer, const char* function) {
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, sizeof(T));
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, sizeof(T));
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(T)));
    return false;
}

float LengthValue(const LegacyLength& length) {
    return length.m_isFloat ? length.m_floatValue : static_cast<float>(length.m_intValue);
}

float PerspectiveValue(const LegacyLength& length) {
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
    WebCore::LegacyTransformOperation** (APS5_VABI* clone)(WebCore::LegacyTransformOperation**, const WebCore::PerspectiveTransformOperation*);
    bool (APS5_VABI* equals)(const WebCore::PerspectiveTransformOperation*, const WebCore::LegacyTransformOperation&);
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

WebCore::PerspectiveTransformOperation MakeOperation(const LegacyLength& length) {
    WebCore::PerspectiveTransformOperation constructed{};
    std::memset(&constructed, 0, sizeof(constructed));
    constructed.m_base.m_vptr = const_cast<OperationVtable*>(&PerspectiveVtable.dispatch);
    constructed.m_base.m_refCount = 1;
    constructed.m_base.m_type = PerspectiveOperationType;
    constructed.m_p = length;
    return constructed;
}

void Construct(WebCore::PerspectiveTransformOperation* self, const LegacyLength* p, const char* function) {
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation: null this");
    if (!p) throw std::invalid_argument("WebCore::PerspectiveTransformOperation: null length");
    const LegacyLength length = ReadGuestValue(p, function);
    if (length.m_type == LegacyLengthType::Calculated) throw std::invalid_argument("calculated WebCore length");
    const auto previous = ReadGuestValue(self, function);
    RequireWritableGuestValue(self, function);
    const auto constructed = MakeOperation(length);
    if (!TryPublishGuestValue(self, constructed, previous))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

WebCore::PerspectiveTransformOperation* AllocateLength(const LegacyLength& length) {
    auto* operation = static_cast<WebCore::PerspectiveTransformOperation*>(_ZN3WTF10fastMallocEm(sizeof(WebCore::PerspectiveTransformOperation)));
    *operation = MakeOperation(length);
    return operation;
}

void Destroy(WebCore::PerspectiveTransformOperation* self, const char* function) {
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation: null this");
    const auto local = ReadGuestValue(self, function);
    if (local.m_p.m_type == LegacyLengthType::Calculated) throw std::invalid_argument("calculated WebCore length");
}

}

extern "C" {

WebCore::PerspectiveTransformOperation** APS5_VABI _ZN7WebCore29PerspectiveTransformOperation6createERKNS_6LengthE(WebCore::PerspectiveTransformOperation** result, const LegacyLength* p) {
    if (!result) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::create: null result");
    if (!p) throw std::invalid_argument("WebCore::PerspectiveTransformOperation: null length");
    const LegacyLength length = ReadGuestValue(p, __func__);
    if (length.m_type == LegacyLengthType::Calculated) throw std::invalid_argument("calculated WebCore length");
    const auto previous = ReadGuestValue(result, __func__);
    RequireWritableGuestValue(result, __func__);
    auto* operation = AllocateLength(length);
    if (!TryPublishGuestValue(result, operation, previous)) {
        _ZN3WTF8fastFreeEPv(operation);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return result;
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationC1ERKNS_6LengthE(WebCore::PerspectiveTransformOperation* self, const LegacyLength* p) {
    Construct(self, p, __func__);
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationC2ERKNS_6LengthE(WebCore::PerspectiveTransformOperation* self, const LegacyLength* p) {
    Construct(self, p, __func__);
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationD1Ev(WebCore::PerspectiveTransformOperation* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationD2Ev(WebCore::PerspectiveTransformOperation* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN7WebCore29PerspectiveTransformOperationD0Ev(WebCore::PerspectiveTransformOperation* self) {
    Destroy(self, __func__);
    RequireWritableGuestValue(self, __func__);
    _ZN3WTF8fastFreeEPv(self);
}

WebCore::LegacyTransformOperation** APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation5cloneEv(WebCore::LegacyTransformOperation** result, const WebCore::PerspectiveTransformOperation* self) {
    if (!result) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::clone: null result");
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::clone: null this");
    const auto local = ReadGuestValue(self, __func__);
    if (local.m_p.m_type == LegacyLengthType::Calculated) throw std::invalid_argument("calculated WebCore length");
    const auto previous = ReadGuestValue(result, __func__);
    RequireWritableGuestValue(result, __func__);
    auto* copy = AllocateLength(local.m_p);
    auto* base = &copy->m_base;
    if (!TryPublishGuestValue(result, base, previous)) {
        _ZN3WTF8fastFreeEPv(copy);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return result;
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation10isIdentityEv(const WebCore::PerspectiveTransformOperation* self) {
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::isIdentity: null this");
    return !PerspectiveValue(ReadGuestValue(self, __func__).m_p);
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation27isAffectedByTransformOriginEv(const WebCore::PerspectiveTransformOperation* self) {
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::isAffectedByTransformOrigin: null this");
    return PerspectiveValue(ReadGuestValue(self, __func__).m_p) != 0.0f;
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation19isRepresentableIn2DEv(const WebCore::PerspectiveTransformOperation*) {
    return false;
}

LegacyLength* APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation11perspectiveEv(LegacyLength* result, const WebCore::PerspectiveTransformOperation* self) {
    if (!result) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::perspective: null result");
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::perspective: null this");
    const auto local = ReadGuestValue(self, __func__);
    if (local.m_p.m_type == LegacyLengthType::Calculated) NotImplemented_nid_no_patch(__func__);
    const auto previous = ReadGuestValue(result, __func__);
    RequireWritableGuestValue(result, __func__);
    if (!TryPublishGuestValue(result, local.m_p, previous))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return result;
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::PerspectiveTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize&) {
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::apply: null this");
    const auto local = ReadGuestValue(self, __func__);
    const float perspective = PerspectiveValue(local.m_p);
    auto* target = &transform;
    const auto previous = ReadGuestValue(target, __func__);
    RequireWritableGuestValue(target, __func__);
    auto updated = previous;
    _ZN7WebCore20TransformationMatrix16applyPerspectiveEd(&updated, perspective);
    if (!TryPublishGuestValue(target, updated, previous))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return false;
}

bool APS5_VABI _ZNK7WebCore29PerspectiveTransformOperationeqERKNS_18TransformOperationE(const WebCore::PerspectiveTransformOperation* self, const WebCore::LegacyTransformOperation& other) {
    if (!self) throw std::invalid_argument("WebCore::PerspectiveTransformOperation::operator==: null this");
    const auto leftOp = ReadGuestValue(self, __func__);
    const auto otherBase = ReadGuestValue(&other, __func__);
    if (otherBase.m_type != leftOp.m_base.m_type) return false;
    const auto rightOp = ReadGuestValue(reinterpret_cast<const WebCore::PerspectiveTransformOperation*>(&other), __func__);
    const LegacyLength& left = leftOp.m_p;
    const LegacyLength& right = rightOp.m_p;
    if (left.m_type != right.m_type || left.m_hasQuirk != right.m_hasQuirk) return false;
    if (left.m_type == LegacyLengthType::Undefined) return true;
    if (left.m_type == LegacyLengthType::Calculated) NotImplemented_nid_no_patch(__func__);
    return LengthValue(left) == LengthValue(right);
}

}
