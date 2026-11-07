// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/platform/graphics/filters/FilterOperation.cpp
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/platform/graphics/filters/FilterOperation.h
/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/platform/Length.h
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
*/

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/FilterOperationExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::FilterAbi::BlendingContext;
using NKWebKit::FilterAbi::FilterOperationPtr;
using NKWebKit::FilterAbi::FilterOperationRef;
using NKWebKit::FilterAbi::FilterOperationVtableStorage;
using NKWebKit::FilterAbi::IntOutsets;
using NKWebKit::FilterAbi::SRGBAf;
using NKWebKit::WtfAbi::TextStream;

using WebCore::BasicColorMatrixFilterOperation;
using WebCore::BasicComponentTransferFilterOperation;
using WebCore::BlurFilterOperation;
using WebCore::Color;
using WebCore::DefaultFilterOperation;
using WebCore::DropShadowFilterOperation;
using WebCore::FilterOperation;
using WebCore::FilterOperationType;
using WebCore::IntPoint;
using WebCore::InvertLightnessFilterOperation;
using WebCore::Length;
using WebCore::LengthType;

template<typename T>
T& Checked(T* pointer) {
    if (!pointer) throw std::invalid_argument("null WebCore filter operation");
    return *pointer;
}

void RejectCalculated(const Length& length) {
    if (length.m_type == LengthType::Calculated) throw std::invalid_argument("calculated WebCore::Length");
}

float LengthValue(const Length& length) {
    return length.m_isFloat ? length.m_floatValue : static_cast<float>(length.m_intValue);
}

void CopyLength(Length& target, const Length& source) {
    RejectCalculated(source);
    target.m_type = source.m_type;
    target.m_hasQuirk = source.m_hasQuirk;
    switch (source.m_type) {
    case LengthType::Auto:
    case LengthType::Content:
    case LengthType::Undefined:
        target.m_intValue = 0;
        target.m_isFloat = false;
        break;
    default:
        target.m_isFloat = source.m_isFloat;
        if (target.m_isFloat)
            target.m_floatValue = source.m_floatValue;
        else
            target.m_intValue = source.m_intValue;
        break;
    }
}

void MoveLength(Length& target, Length& source) {
    CopyLength(target, source);
    source.m_type = LengthType::Auto;
}

bool LengthsEqual(const Length& first, const Length& second) {
    if (first.m_type != second.m_type || first.m_hasQuirk != second.m_hasQuirk) return false;
    if (first.m_type == LengthType::Undefined) return true;
    RejectCalculated(first);
    return LengthValue(first) == LengthValue(second);
}

void InitializeOperation(FilterOperation& operation, FilterOperationType type, const FilterOperationVtableStorage& vtable) {
    operation.m_vptr = const_cast<NKWebKit::FilterAbi::FilterOperationVtable*>(&vtable.dispatch);
    operation.m_refCount = 1U;
    operation.m_type = type;
    operation.m_pad[0] = 0;
    operation.m_pad[1] = 0;
    operation.m_pad[2] = 0;
}

template<typename T>
T* AllocateOperation() {
    return static_cast<T*>(_ZN3WTF10fastMallocEm(sizeof(T)));
}

void APS5_VABI DefaultVtableD1_nid_no_patch(FilterOperation*) {
}

void APS5_VABI DefaultVtableD0_nid_no_patch(FilterOperation* self) {
    _ZN3WTF8fastFreeEPv(self);
}

FilterOperationRef APS5_VABI DefaultVtableClone_nid_no_patch(const FilterOperation* self) {
    return _ZNK7WebCore22DefaultFilterOperation5cloneEv(static_cast<const DefaultFilterOperation*>(self));
}

bool APS5_VABI DefaultVtableEquals_nid_no_patch(const FilterOperation* self, const FilterOperation* other) {
    return _ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(static_cast<const DefaultFilterOperation*>(self), other);
}

FilterOperationPtr APS5_VABI DefaultVtableBlend_nid_no_patch(FilterOperation*, const FilterOperation*, const BlendingContext*, bool) {
    return FilterOperationPtr();
}

bool APS5_VABI DefaultVtableTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    return false;
}

bool APS5_VABI DefaultVtableInverseTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    return false;
}

bool APS5_VABI DefaultVtableIsIdentity_nid_no_patch(const FilterOperation*) {
    return false;
}

IntOutsets APS5_VABI DefaultVtableOutsets_nid_no_patch(const FilterOperation*) {
    return IntOutsets{};
}

bool APS5_VABI DefaultVtableAffectsOpacity_nid_no_patch(const FilterOperation*) {
    return false;
}

bool APS5_VABI DefaultVtableMovesPixels_nid_no_patch(const FilterOperation*) {
    return false;
}

bool APS5_VABI DefaultVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch(const FilterOperation*) {
    return false;
}

void APS5_VABI BasicColorMatrixVtableD1_nid_no_patch(FilterOperation*) {
}

void APS5_VABI BasicColorMatrixVtableD0_nid_no_patch(FilterOperation* self) {
    _ZN3WTF8fastFreeEPv(self);
}

FilterOperationRef APS5_VABI BasicColorMatrixVtableClone_nid_no_patch(const FilterOperation* self) {
    return _ZNK7WebCore31BasicColorMatrixFilterOperation5cloneEv(static_cast<const BasicColorMatrixFilterOperation*>(self));
}

bool APS5_VABI BasicColorMatrixVtableEquals_nid_no_patch(const FilterOperation* self, const FilterOperation* other) {
    return _ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(static_cast<const BasicColorMatrixFilterOperation*>(self), other);
}

FilterOperationPtr APS5_VABI BasicColorMatrixVtableBlend_nid_no_patch(FilterOperation*, const FilterOperation*, const BlendingContext*, bool) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI BasicColorMatrixVtableTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI BasicColorMatrixVtableInverseTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    return false;
}

bool APS5_VABI BasicColorMatrixVtableIsIdentity_nid_no_patch(const FilterOperation* self) {
    auto& operation = static_cast<const BasicColorMatrixFilterOperation&>(Checked(self));
    return operation.m_type == FilterOperationType::Saturate ? operation.m_amount == 1 : !operation.m_amount;
}

IntOutsets APS5_VABI BasicColorMatrixVtableOutsets_nid_no_patch(const FilterOperation*) {
    return IntOutsets{};
}

bool APS5_VABI BasicColorMatrixVtableAffectsOpacity_nid_no_patch(const FilterOperation*) {
    return false;
}

bool APS5_VABI BasicColorMatrixVtableMovesPixels_nid_no_patch(const FilterOperation*) {
    return false;
}

bool APS5_VABI BasicColorMatrixVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch(const FilterOperation*) {
    return false;
}

void APS5_VABI BasicComponentTransferVtableD1_nid_no_patch(FilterOperation*) {
}

void APS5_VABI BasicComponentTransferVtableD0_nid_no_patch(FilterOperation* self) {
    _ZN3WTF8fastFreeEPv(self);
}

FilterOperationRef APS5_VABI BasicComponentTransferVtableClone_nid_no_patch(const FilterOperation* self) {
    return _ZNK7WebCore37BasicComponentTransferFilterOperation5cloneEv(static_cast<const BasicComponentTransferFilterOperation*>(self));
}

bool APS5_VABI BasicComponentTransferVtableEquals_nid_no_patch(const FilterOperation* self, const FilterOperation* other) {
    return _ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(static_cast<const BasicComponentTransferFilterOperation*>(self), other);
}

FilterOperationPtr APS5_VABI BasicComponentTransferVtableBlend_nid_no_patch(FilterOperation*, const FilterOperation*, const BlendingContext*, bool) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI BasicComponentTransferVtableTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI BasicComponentTransferVtableInverseTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    return false;
}

bool APS5_VABI BasicComponentTransferVtableIsIdentity_nid_no_patch(const FilterOperation* self) {
    auto& operation = static_cast<const BasicComponentTransferFilterOperation&>(Checked(self));
    return operation.m_type == FilterOperationType::Invert ? !operation.m_amount : operation.m_amount == 1;
}

IntOutsets APS5_VABI BasicComponentTransferVtableOutsets_nid_no_patch(const FilterOperation*) {
    return IntOutsets{};
}

bool APS5_VABI BasicComponentTransferVtableAffectsOpacity_nid_no_patch(const FilterOperation* self) {
    return _ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(static_cast<const BasicComponentTransferFilterOperation*>(self));
}

bool APS5_VABI BasicComponentTransferVtableMovesPixels_nid_no_patch(const FilterOperation*) {
    return false;
}

bool APS5_VABI BasicComponentTransferVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch(const FilterOperation*) {
    return false;
}

void APS5_VABI InvertLightnessVtableD1_nid_no_patch(FilterOperation*) {
}

void APS5_VABI InvertLightnessVtableD0_nid_no_patch(FilterOperation* self) {
    _ZN3WTF8fastFreeEPv(self);
}

FilterOperationRef APS5_VABI InvertLightnessVtableClone_nid_no_patch(const FilterOperation* self) {
    return _ZNK7WebCore30InvertLightnessFilterOperation5cloneEv(static_cast<const InvertLightnessFilterOperation*>(self));
}

bool APS5_VABI InvertLightnessVtableEquals_nid_no_patch(const FilterOperation* self, const FilterOperation* other) {
    return _ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE(static_cast<const InvertLightnessFilterOperation*>(self), other);
}

FilterOperationPtr APS5_VABI InvertLightnessVtableBlend_nid_no_patch(FilterOperation* self, const FilterOperation* from, const BlendingContext*, bool) {
    auto& operation = Checked(self);
    if (from && from->m_type != operation.m_type) return FilterOperationPtr(self);
    return FilterOperationPtr(_ZN7WebCore30InvertLightnessFilterOperation6createEv());
}

bool APS5_VABI InvertLightnessVtableTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI InvertLightnessVtableInverseTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI InvertLightnessVtableIsIdentity_nid_no_patch(const FilterOperation*) {
    return false;
}

IntOutsets APS5_VABI InvertLightnessVtableOutsets_nid_no_patch(const FilterOperation*) {
    return IntOutsets{};
}

bool APS5_VABI InvertLightnessVtableAffectsOpacity_nid_no_patch(const FilterOperation*) {
    return false;
}

bool APS5_VABI InvertLightnessVtableMovesPixels_nid_no_patch(const FilterOperation*) {
    return false;
}

bool APS5_VABI InvertLightnessVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch(const FilterOperation*) {
    return false;
}

void APS5_VABI BlurVtableD1_nid_no_patch(FilterOperation* self) {
    _ZN7WebCore19BlurFilterOperationD1Ev(static_cast<BlurFilterOperation*>(self));
}

void APS5_VABI BlurVtableD0_nid_no_patch(FilterOperation* self) {
    _ZN7WebCore19BlurFilterOperationD0Ev(static_cast<BlurFilterOperation*>(self));
}

FilterOperationRef APS5_VABI BlurVtableClone_nid_no_patch(const FilterOperation* self) {
    return _ZNK7WebCore19BlurFilterOperation5cloneEv(static_cast<const BlurFilterOperation*>(self));
}

bool APS5_VABI BlurVtableEquals_nid_no_patch(const FilterOperation* self, const FilterOperation* other) {
    return _ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(static_cast<const BlurFilterOperation*>(self), other);
}

FilterOperationPtr APS5_VABI BlurVtableBlend_nid_no_patch(FilterOperation*, const FilterOperation*, const BlendingContext*, bool) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI BlurVtableTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    return false;
}

bool APS5_VABI BlurVtableInverseTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    return false;
}

bool APS5_VABI BlurVtableIsIdentity_nid_no_patch(const FilterOperation*) {
    NotImplemented_nid_no_patch(__func__);
}

IntOutsets APS5_VABI BlurVtableOutsets_nid_no_patch(const FilterOperation*) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI BlurVtableAffectsOpacity_nid_no_patch(const FilterOperation*) {
    return true;
}

bool APS5_VABI BlurVtableMovesPixels_nid_no_patch(const FilterOperation*) {
    return true;
}

bool APS5_VABI BlurVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch(const FilterOperation*) {
    return false;
}

void APS5_VABI DropShadowVtableD1_nid_no_patch(FilterOperation* self) {
    _ZN7WebCore25DropShadowFilterOperationD1Ev(static_cast<DropShadowFilterOperation*>(self));
}

void APS5_VABI DropShadowVtableD0_nid_no_patch(FilterOperation* self) {
    _ZN7WebCore25DropShadowFilterOperationD0Ev(static_cast<DropShadowFilterOperation*>(self));
}

FilterOperationRef APS5_VABI DropShadowVtableClone_nid_no_patch(const FilterOperation* self) {
    return _ZNK7WebCore25DropShadowFilterOperation5cloneEv(static_cast<const DropShadowFilterOperation*>(self));
}

bool APS5_VABI DropShadowVtableEquals_nid_no_patch(const FilterOperation* self, const FilterOperation* other) {
    return _ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(static_cast<const DropShadowFilterOperation*>(self), other);
}

FilterOperationPtr APS5_VABI DropShadowVtableBlend_nid_no_patch(FilterOperation*, const FilterOperation*, const BlendingContext*, bool) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI DropShadowVtableTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    return false;
}

bool APS5_VABI DropShadowVtableInverseTransformColor_nid_no_patch(const FilterOperation*, SRGBAf*) {
    return false;
}

bool APS5_VABI DropShadowVtableIsIdentity_nid_no_patch(const FilterOperation* self) {
    auto& operation = static_cast<const DropShadowFilterOperation&>(Checked(self));
    return operation.m_stdDeviation < 0 || (!operation.m_stdDeviation && !operation.m_location.m_x && !operation.m_location.m_y);
}

IntOutsets APS5_VABI DropShadowVtableOutsets_nid_no_patch(const FilterOperation*) {
    NotImplemented_nid_no_patch(__func__);
}

bool APS5_VABI DropShadowVtableAffectsOpacity_nid_no_patch(const FilterOperation*) {
    return true;
}

bool APS5_VABI DropShadowVtableMovesPixels_nid_no_patch(const FilterOperation*) {
    return true;
}

bool APS5_VABI DropShadowVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch(const FilterOperation*) {
    return false;
}

const FilterOperationVtableStorage DefaultVtableInstance = {
    0,
    nullptr,
    {
        &DefaultVtableD1_nid_no_patch,
        &DefaultVtableD0_nid_no_patch,
        &DefaultVtableClone_nid_no_patch,
        &DefaultVtableEquals_nid_no_patch,
        &DefaultVtableBlend_nid_no_patch,
        &DefaultVtableTransformColor_nid_no_patch,
        &DefaultVtableInverseTransformColor_nid_no_patch,
        &DefaultVtableIsIdentity_nid_no_patch,
        &DefaultVtableOutsets_nid_no_patch,
        &DefaultVtableAffectsOpacity_nid_no_patch,
        &DefaultVtableMovesPixels_nid_no_patch,
        &DefaultVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch,
    },
};

const FilterOperationVtableStorage BasicColorMatrixVtableInstance = {
    0,
    nullptr,
    {
        &BasicColorMatrixVtableD1_nid_no_patch,
        &BasicColorMatrixVtableD0_nid_no_patch,
        &BasicColorMatrixVtableClone_nid_no_patch,
        &BasicColorMatrixVtableEquals_nid_no_patch,
        &BasicColorMatrixVtableBlend_nid_no_patch,
        &BasicColorMatrixVtableTransformColor_nid_no_patch,
        &BasicColorMatrixVtableInverseTransformColor_nid_no_patch,
        &BasicColorMatrixVtableIsIdentity_nid_no_patch,
        &BasicColorMatrixVtableOutsets_nid_no_patch,
        &BasicColorMatrixVtableAffectsOpacity_nid_no_patch,
        &BasicColorMatrixVtableMovesPixels_nid_no_patch,
        &BasicColorMatrixVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch,
    },
};

const FilterOperationVtableStorage BasicComponentTransferVtableInstance = {
    0,
    nullptr,
    {
        &BasicComponentTransferVtableD1_nid_no_patch,
        &BasicComponentTransferVtableD0_nid_no_patch,
        &BasicComponentTransferVtableClone_nid_no_patch,
        &BasicComponentTransferVtableEquals_nid_no_patch,
        &BasicComponentTransferVtableBlend_nid_no_patch,
        &BasicComponentTransferVtableTransformColor_nid_no_patch,
        &BasicComponentTransferVtableInverseTransformColor_nid_no_patch,
        &BasicComponentTransferVtableIsIdentity_nid_no_patch,
        &BasicComponentTransferVtableOutsets_nid_no_patch,
        &BasicComponentTransferVtableAffectsOpacity_nid_no_patch,
        &BasicComponentTransferVtableMovesPixels_nid_no_patch,
        &BasicComponentTransferVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch,
    },
};

const FilterOperationVtableStorage InvertLightnessVtableInstance = {
    0,
    nullptr,
    {
        &InvertLightnessVtableD1_nid_no_patch,
        &InvertLightnessVtableD0_nid_no_patch,
        &InvertLightnessVtableClone_nid_no_patch,
        &InvertLightnessVtableEquals_nid_no_patch,
        &InvertLightnessVtableBlend_nid_no_patch,
        &InvertLightnessVtableTransformColor_nid_no_patch,
        &InvertLightnessVtableInverseTransformColor_nid_no_patch,
        &InvertLightnessVtableIsIdentity_nid_no_patch,
        &InvertLightnessVtableOutsets_nid_no_patch,
        &InvertLightnessVtableAffectsOpacity_nid_no_patch,
        &InvertLightnessVtableMovesPixels_nid_no_patch,
        &InvertLightnessVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch,
    },
};

const FilterOperationVtableStorage BlurVtableInstance = {
    0,
    nullptr,
    {
        &BlurVtableD1_nid_no_patch,
        &BlurVtableD0_nid_no_patch,
        &BlurVtableClone_nid_no_patch,
        &BlurVtableEquals_nid_no_patch,
        &BlurVtableBlend_nid_no_patch,
        &BlurVtableTransformColor_nid_no_patch,
        &BlurVtableInverseTransformColor_nid_no_patch,
        &BlurVtableIsIdentity_nid_no_patch,
        &BlurVtableOutsets_nid_no_patch,
        &BlurVtableAffectsOpacity_nid_no_patch,
        &BlurVtableMovesPixels_nid_no_patch,
        &BlurVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch,
    },
};

const FilterOperationVtableStorage DropShadowVtableInstance = {
    0,
    nullptr,
    {
        &DropShadowVtableD1_nid_no_patch,
        &DropShadowVtableD0_nid_no_patch,
        &DropShadowVtableClone_nid_no_patch,
        &DropShadowVtableEquals_nid_no_patch,
        &DropShadowVtableBlend_nid_no_patch,
        &DropShadowVtableTransformColor_nid_no_patch,
        &DropShadowVtableInverseTransformColor_nid_no_patch,
        &DropShadowVtableIsIdentity_nid_no_patch,
        &DropShadowVtableOutsets_nid_no_patch,
        &DropShadowVtableAffectsOpacity_nid_no_patch,
        &DropShadowVtableMovesPixels_nid_no_patch,
        &DropShadowVtableShouldBeRestrictedBySecurityOrigin_nid_no_patch,
    },
};

DefaultFilterOperation* CreateDefault(FilterOperationType representedType) {
    auto* operation = AllocateOperation<DefaultFilterOperation>();
    InitializeOperation(*operation, FilterOperationType::Default, DefaultVtableInstance);
    operation->m_representedType = representedType;
    return operation;
}

BasicColorMatrixFilterOperation* CreateBasicColorMatrix(double amount, FilterOperationType type) {
    auto* operation = AllocateOperation<BasicColorMatrixFilterOperation>();
    InitializeOperation(*operation, type, BasicColorMatrixVtableInstance);
    operation->m_amount = amount;
    return operation;
}

BasicComponentTransferFilterOperation* CreateBasicComponentTransfer(double amount, FilterOperationType type) {
    auto* operation = AllocateOperation<BasicComponentTransferFilterOperation>();
    InitializeOperation(*operation, type, BasicComponentTransferVtableInstance);
    operation->m_amount = amount;
    return operation;
}

InvertLightnessFilterOperation* CreateInvertLightness() {
    auto* operation = AllocateOperation<InvertLightnessFilterOperation>();
    InitializeOperation(*operation, FilterOperationType::AppleInvertLightness, InvertLightnessVtableInstance);
    return operation;
}

BlurFilterOperation* CreateBlur(Length& stdDeviation) {
    RejectCalculated(stdDeviation);
    auto* operation = AllocateOperation<BlurFilterOperation>();
    InitializeOperation(*operation, FilterOperationType::Blur, BlurVtableInstance);
    MoveLength(operation->m_stdDeviation, stdDeviation);
    return operation;
}

BlurFilterOperation* CloneBlur(const Length& stdDeviation) {
    RejectCalculated(stdDeviation);
    auto* operation = AllocateOperation<BlurFilterOperation>();
    InitializeOperation(*operation, FilterOperationType::Blur, BlurVtableInstance);
    CopyLength(operation->m_stdDeviation, stdDeviation);
    return operation;
}

DropShadowFilterOperation* CreateDropShadow(const IntPoint& location, int stdDeviation, const Color& color) {
    auto* operation = AllocateOperation<DropShadowFilterOperation>();
    InitializeOperation(*operation, FilterOperationType::DropShadow, DropShadowVtableInstance);
    operation->m_location = location;
    operation->m_stdDeviation = stdDeviation;
    try {
        _ZN7WebCore5ColorC1ERKS0_(&operation->m_color, color);
    } catch (...) {
        _ZN3WTF8fastFreeEPv(operation);
        throw;
    }
    return operation;
}

}  // namespace

extern "C" {

FilterOperationRef APS5_VABI _ZN7WebCore19BlurFilterOperation6createENS_6LengthE(Length* stdDeviation) {
    return FilterOperationRef(CreateBlur(Checked(stdDeviation)));
}

void APS5_VABI _ZN7WebCore19BlurFilterOperationC1ENS_6LengthE(BlurFilterOperation* self, Length* stdDeviation) {
    auto& operation = Checked(self);
    auto& value = Checked(stdDeviation);
    RejectCalculated(value);
    InitializeOperation(operation, FilterOperationType::Blur, BlurVtableInstance);
    MoveLength(operation.m_stdDeviation, value);
}

void APS5_VABI _ZN7WebCore19BlurFilterOperationC2ENS_6LengthE(BlurFilterOperation* self, Length* stdDeviation) {
    auto& operation = Checked(self);
    auto& value = Checked(stdDeviation);
    RejectCalculated(value);
    InitializeOperation(operation, FilterOperationType::Blur, BlurVtableInstance);
    MoveLength(operation.m_stdDeviation, value);
}

void APS5_VABI _ZN7WebCore19BlurFilterOperationD0Ev(BlurFilterOperation* self) {
    _ZN7WebCore19BlurFilterOperationD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

void APS5_VABI _ZN7WebCore19BlurFilterOperationD1Ev(BlurFilterOperation* self) {
    Checked(self);
}

void APS5_VABI _ZN7WebCore19BlurFilterOperationD2Ev(BlurFilterOperation* self) {
    Checked(self);
}

FilterOperationRef APS5_VABI _ZN7WebCore25DropShadowFilterOperation6createERKNS_8IntPointEiRKNS_5ColorE(const IntPoint* location, int stdDeviation, const Color* color) {
    return FilterOperationRef(CreateDropShadow(Checked(location), stdDeviation, Checked(color)));
}

void APS5_VABI _ZN7WebCore25DropShadowFilterOperationC1ERKNS_8IntPointEiRKNS_5ColorE(DropShadowFilterOperation* self, const IntPoint* location, int stdDeviation, const Color* color) {
    auto& operation = Checked(self);
    auto& position = Checked(location);
    _ZN7WebCore5ColorC1ERKS0_(&operation.m_color, Checked(color));
    InitializeOperation(operation, FilterOperationType::DropShadow, DropShadowVtableInstance);
    operation.m_location = position;
    operation.m_stdDeviation = stdDeviation;
}

void APS5_VABI _ZN7WebCore25DropShadowFilterOperationC2ERKNS_8IntPointEiRKNS_5ColorE(DropShadowFilterOperation* self, const IntPoint* location, int stdDeviation, const Color* color) {
    auto& operation = Checked(self);
    auto& position = Checked(location);
    _ZN7WebCore5ColorC1ERKS0_(&operation.m_color, Checked(color));
    InitializeOperation(operation, FilterOperationType::DropShadow, DropShadowVtableInstance);
    operation.m_location = position;
    operation.m_stdDeviation = stdDeviation;
}

void APS5_VABI _ZN7WebCore25DropShadowFilterOperationD0Ev(DropShadowFilterOperation* self) {
    _ZN7WebCore25DropShadowFilterOperationD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

void APS5_VABI _ZN7WebCore25DropShadowFilterOperationD1Ev(DropShadowFilterOperation* self) {
    Checked(self);
}

void APS5_VABI _ZN7WebCore25DropShadowFilterOperationD2Ev(DropShadowFilterOperation* self) {
    Checked(self);
}

FilterOperationRef APS5_VABI _ZN7WebCore30InvertLightnessFilterOperation6createEv() {
    return FilterOperationRef(CreateInvertLightness());
}

void APS5_VABI _ZN7WebCore30InvertLightnessFilterOperationC1Ev(InvertLightnessFilterOperation* self) {
    InitializeOperation(Checked(self), FilterOperationType::AppleInvertLightness, InvertLightnessVtableInstance);
}

void APS5_VABI _ZN7WebCore30InvertLightnessFilterOperationC2Ev(InvertLightnessFilterOperation* self) {
    InitializeOperation(Checked(self), FilterOperationType::AppleInvertLightness, InvertLightnessVtableInstance);
}

TextStream* APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15FilterOperationE(TextStream* stream, const FilterOperation* operation) {
    auto& ts = Checked(stream);
    auto& filter = Checked(operation);
    switch (filter.m_type) {
    case FilterOperationType::Reference:
        _ZN3WTF10TextStreamlsEPKc(&ts, "reference");
        break;
    case FilterOperationType::Grayscale:
        _ZN3WTF10TextStreamlsEPKc(&ts, "grayscale(");
        _ZN3WTF10TextStreamlsEd(&ts, static_cast<const BasicColorMatrixFilterOperation&>(filter).m_amount);
        _ZN3WTF10TextStreamlsEPKc(&ts, ")");
        break;
    case FilterOperationType::Sepia:
        _ZN3WTF10TextStreamlsEPKc(&ts, "sepia(");
        _ZN3WTF10TextStreamlsEd(&ts, static_cast<const BasicColorMatrixFilterOperation&>(filter).m_amount);
        _ZN3WTF10TextStreamlsEPKc(&ts, ")");
        break;
    case FilterOperationType::Saturate:
        _ZN3WTF10TextStreamlsEPKc(&ts, "saturate(");
        _ZN3WTF10TextStreamlsEd(&ts, static_cast<const BasicColorMatrixFilterOperation&>(filter).m_amount);
        _ZN3WTF10TextStreamlsEPKc(&ts, ")");
        break;
    case FilterOperationType::HueRotate:
        _ZN3WTF10TextStreamlsEPKc(&ts, "hue-rotate(");
        _ZN3WTF10TextStreamlsEd(&ts, static_cast<const BasicColorMatrixFilterOperation&>(filter).m_amount);
        _ZN3WTF10TextStreamlsEPKc(&ts, ")");
        break;
    case FilterOperationType::Invert:
        _ZN3WTF10TextStreamlsEPKc(&ts, "invert(");
        _ZN3WTF10TextStreamlsEd(&ts, static_cast<const BasicComponentTransferFilterOperation&>(filter).m_amount);
        _ZN3WTF10TextStreamlsEPKc(&ts, ")");
        break;
    case FilterOperationType::AppleInvertLightness:
        _ZN3WTF10TextStreamlsEPKc(&ts, "apple-invert-lightness()");
        break;
    case FilterOperationType::Opacity:
        _ZN3WTF10TextStreamlsEPKc(&ts, "opacity(");
        _ZN3WTF10TextStreamlsEd(&ts, static_cast<const BasicComponentTransferFilterOperation&>(filter).m_amount);
        _ZN3WTF10TextStreamlsEPKc(&ts, ")");
        break;
    case FilterOperationType::Brightness:
        _ZN3WTF10TextStreamlsEPKc(&ts, "brightness(");
        _ZN3WTF10TextStreamlsEd(&ts, static_cast<const BasicComponentTransferFilterOperation&>(filter).m_amount);
        _ZN3WTF10TextStreamlsEPKc(&ts, ")");
        break;
    case FilterOperationType::Contrast:
        _ZN3WTF10TextStreamlsEPKc(&ts, "contrast(");
        _ZN3WTF10TextStreamlsEd(&ts, static_cast<const BasicComponentTransferFilterOperation&>(filter).m_amount);
        _ZN3WTF10TextStreamlsEPKc(&ts, ")");
        break;
    case FilterOperationType::Blur:
        _ZN3WTF10TextStreamlsEPKc(&ts, "blur(");
        _ZN3WTF10TextStreamlsEf(&ts, LengthValue(static_cast<const BlurFilterOperation&>(filter).m_stdDeviation));
        _ZN3WTF10TextStreamlsEPKc(&ts, ")");
        break;
    case FilterOperationType::DropShadow:
        NotImplemented_nid_no_patch(__func__);
    case FilterOperationType::Passthrough:
        _ZN3WTF10TextStreamlsEPKc(&ts, "passthrough");
        break;
    case FilterOperationType::Default:
        _ZN3WTF10TextStreamlsEPKc(&ts, "default type=");
        _ZN3WTF10TextStreamlsEi(&ts, static_cast<int>(static_cast<const DefaultFilterOperation&>(filter).m_representedType));
        break;
    case FilterOperationType::None:
        _ZN3WTF10TextStreamlsEPKc(&ts, "none");
        break;
    }
    return &ts;
}

const Length* APS5_VABI _ZNK7WebCore19BlurFilterOperation12stdDeviationEv(const BlurFilterOperation* self) {
    return &Checked(self).m_stdDeviation;
}

FilterOperationRef APS5_VABI _ZNK7WebCore19BlurFilterOperation5cloneEv(const BlurFilterOperation* self) {
    return FilterOperationRef(CloneBlur(Checked(self).m_stdDeviation));
}

bool APS5_VABI _ZNK7WebCore19BlurFilterOperationeqERKNS_15FilterOperationE(const BlurFilterOperation* self, const FilterOperation* other) {
    auto& operation = Checked(self);
    auto& compared = Checked(other);
    if (operation.m_type != compared.m_type) return false;
    return LengthsEqual(operation.m_stdDeviation, static_cast<const BlurFilterOperation&>(compared).m_stdDeviation);
}

FilterOperationType APS5_VABI _ZNK7WebCore22DefaultFilterOperation15representedTypeEv(const DefaultFilterOperation* self) {
    return Checked(self).m_representedType;
}

FilterOperationRef APS5_VABI _ZNK7WebCore22DefaultFilterOperation5cloneEv(const DefaultFilterOperation* self) {
    return FilterOperationRef(CreateDefault(Checked(self).m_representedType));
}

bool APS5_VABI _ZNK7WebCore22DefaultFilterOperationeqERKNS_15FilterOperationE(const DefaultFilterOperation* self, const FilterOperation* other) {
    auto& operation = Checked(self);
    auto& compared = Checked(other);
    if (operation.m_type != compared.m_type) return false;
    return operation.m_representedType == static_cast<const DefaultFilterOperation&>(compared).m_representedType;
}

int APS5_VABI _ZNK7WebCore25DropShadowFilterOperation12stdDeviationEv(const DropShadowFilterOperation* self) {
    return Checked(self).m_stdDeviation;
}

int APS5_VABI _ZNK7WebCore25DropShadowFilterOperation1xEv(const DropShadowFilterOperation* self) {
    return Checked(self).m_location.m_x;
}

int APS5_VABI _ZNK7WebCore25DropShadowFilterOperation1yEv(const DropShadowFilterOperation* self) {
    return Checked(self).m_location.m_y;
}

FilterOperationRef APS5_VABI _ZNK7WebCore25DropShadowFilterOperation5cloneEv(const DropShadowFilterOperation* self) {
    auto& operation = Checked(self);
    return FilterOperationRef(CreateDropShadow(operation.m_location, operation.m_stdDeviation, operation.m_color));
}

const Color* APS5_VABI _ZNK7WebCore25DropShadowFilterOperation5colorEv(const DropShadowFilterOperation* self) {
    return &Checked(self).m_color;
}

IntPoint APS5_VABI _ZNK7WebCore25DropShadowFilterOperation8locationEv(const DropShadowFilterOperation* self) {
    return Checked(self).m_location;
}

bool APS5_VABI _ZNK7WebCore25DropShadowFilterOperationeqERKNS_15FilterOperationE(const DropShadowFilterOperation* self, const FilterOperation* other) {
    auto& operation = Checked(self);
    auto& compared = Checked(other);
    if (operation.m_type != compared.m_type) return false;
    auto& otherOperation = static_cast<const DropShadowFilterOperation&>(compared);
    return operation.m_location.m_x == otherOperation.m_location.m_x
        && operation.m_location.m_y == otherOperation.m_location.m_y
        && operation.m_stdDeviation == otherOperation.m_stdDeviation
        && operation.m_color.m_colorAndFlags == otherOperation.m_color.m_colorAndFlags;
}

FilterOperationRef APS5_VABI _ZNK7WebCore30InvertLightnessFilterOperation5cloneEv(const InvertLightnessFilterOperation* self) {
    Checked(self);
    return FilterOperationRef(CreateInvertLightness());
}

bool APS5_VABI _ZNK7WebCore30InvertLightnessFilterOperationeqERKNS_15FilterOperationE(const InvertLightnessFilterOperation* self, const FilterOperation* other) {
    auto& operation = Checked(self);
    auto& compared = Checked(other);
    return operation.m_type == compared.m_type;
}

double APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation17passthroughAmountEv(const BasicColorMatrixFilterOperation* self) {
    switch (Checked(self).m_type) {
    case FilterOperationType::Grayscale:
    case FilterOperationType::Sepia:
    case FilterOperationType::HueRotate:
        return 0;
    case FilterOperationType::Saturate:
        return 1;
    default:
        return 0;
    }
}

FilterOperationRef APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation5cloneEv(const BasicColorMatrixFilterOperation* self) {
    auto& operation = Checked(self);
    return FilterOperationRef(CreateBasicColorMatrix(operation.m_amount, operation.m_type));
}

double APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation6amountEv(const BasicColorMatrixFilterOperation* self) {
    return Checked(self).m_amount;
}

bool APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperationeqERKNS_15FilterOperationE(const BasicColorMatrixFilterOperation* self, const FilterOperation* other) {
    auto& operation = Checked(self);
    auto& compared = Checked(other);
    if (operation.m_type != compared.m_type) return false;
    return operation.m_amount == static_cast<const BasicColorMatrixFilterOperation&>(compared).m_amount;
}

bool APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation14affectsOpacityEv(const BasicComponentTransferFilterOperation* self) {
    return Checked(self).m_type == FilterOperationType::Opacity;
}

double APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation17passthroughAmountEv(const BasicComponentTransferFilterOperation* self) {
    switch (Checked(self).m_type) {
    case FilterOperationType::Opacity:
        return 1;
    case FilterOperationType::Invert:
        return 0;
    case FilterOperationType::Contrast:
        return 1;
    case FilterOperationType::Brightness:
        return 1;
    default:
        return 0;
    }
}

FilterOperationRef APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation5cloneEv(const BasicComponentTransferFilterOperation* self) {
    auto& operation = Checked(self);
    return FilterOperationRef(CreateBasicComponentTransfer(operation.m_amount, operation.m_type));
}

double APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperation6amountEv(const BasicComponentTransferFilterOperation* self) {
    return Checked(self).m_amount;
}

bool APS5_VABI _ZNK7WebCore37BasicComponentTransferFilterOperationeqERKNS_15FilterOperationE(const BasicComponentTransferFilterOperation* self, const FilterOperation* other) {
    auto& operation = Checked(self);
    auto& compared = Checked(other);
    if (operation.m_type != compared.m_type) return false;
    return operation.m_amount == static_cast<const BasicComponentTransferFilterOperation&>(compared).m_amount;
}

}
