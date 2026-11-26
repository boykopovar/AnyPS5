// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/transforms/TranslateTransformOperation.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/transforms/TranslateTransformOperation.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/Length.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/transforms/TransformOperation.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreLayoutExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreTransformExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::TextStream;

constexpr unsigned NumberRespectingIntegers = 1u << 1;

WebCore::TranslateTransformOperation* Checked(WebCore::TranslateTransformOperation* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::TranslateTransformOperation::") + operation + ": null this");
    return self;
}

const WebCore::TranslateTransformOperation* CheckedConst(const WebCore::TranslateTransformOperation* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::TranslateTransformOperation::") + operation + ": null this");
    return self;
}

float LengthValue(const WebCore::Length& length) {
    return length.m_isFloat ? length.m_floatValue : static_cast<float>(length.m_intValue);
}

bool IsZero(const WebCore::Length& length) {
    if (length.isCalculated() || length.isAuto()) return false;
    return length.m_isFloat ? !length.m_floatValue : !length.m_intValue;
}

bool AnyCalculated(const WebCore::Length& first, const WebCore::Length& second, const WebCore::Length& third) {
    return first.isCalculated() || second.isCalculated() || third.isCalculated();
}

bool IsIntrinsic(const WebCore::Length& length) {
    WebCore::LengthType type = length.type();
    return type == WebCore::LengthType::MinContent || type == WebCore::LengthType::MaxContent
        || type == WebCore::LengthType::FillAvailable || type == WebCore::LengthType::FitContent;
}

bool IsLegacyIntrinsic(const WebCore::Length& length) {
    WebCore::LengthType type = length.type();
    return type == WebCore::LengthType::Intrinsic || type == WebCore::LengthType::MinIntrinsic;
}

bool IsIntrinsicOrAuto(const WebCore::Length& length) {
    return length.isAuto() || IsIntrinsic(length) || IsLegacyIntrinsic(length);
}

bool IsTranslateType(WebCore::TransformOperation::Type type) {
    return type == WebCore::TransformOperation::Type::TranslateX
        || type == WebCore::TransformOperation::Type::TranslateY
        || type == WebCore::TransformOperation::Type::TranslateZ
        || type == WebCore::TransformOperation::Type::Translate
        || type == WebCore::TransformOperation::Type::Translate3D;
}

bool LengthEqual(const WebCore::Length& first, const WebCore::Length& second) {
    if (first.type() != second.type() || first.m_hasQuirk != second.m_hasQuirk) return false;
    if (first.type() == WebCore::LengthType::Undefined) return true;
    if (first.isCalculated()) NotImplemented_nid_no_patch(__func__);
    return LengthValue(first) == LengthValue(second);
}

WebCore::TransformOperationRef Create(const WebCore::Length& tx, const WebCore::Length& ty, const WebCore::Length& tz, WebCore::TransformOperation::Type type) {
    if (!IsTranslateType(type)) throw std::invalid_argument("WebCore::TranslateTransformOperation: non-translate operation type");
    if (AnyCalculated(tx, ty, tz)) NotImplemented_nid_no_patch(__func__);
    void* mem = _ZN3WTF10fastMallocEm(sizeof(WebCore::TranslateTransformOperation));
    auto* object = static_cast<WebCore::TranslateTransformOperation*>(mem);
    _ZN7WebCore27TranslateTransformOperationC1ERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(object, tx, ty, tz, type);
    return WebCore::TransformOperationRef{object};
}

WebCore::TransformOperation::Type APS5_VABI TranslatePrimitiveType(const WebCore::TranslateTransformOperation* self) {
    return _ZNK7WebCore27TranslateTransformOperation19isRepresentableIn2DEv(self)
        ? WebCore::TransformOperation::Type::Translate
        : WebCore::TransformOperation::Type::Translate3D;
}

std::optional<WebCore::TransformOperation::Type> SharedPrimitiveTypeOfTypes(
    WebCore::TransformOperation::Type type, WebCore::TransformOperation::Type other) {
    if (type == other) return type;
    static constexpr WebCore::TransformOperation::Type sharedPrimitives[3][2] = {
        {WebCore::TransformOperation::Type::Rotate, WebCore::TransformOperation::Type::Rotate3D},
        {WebCore::TransformOperation::Type::Scale, WebCore::TransformOperation::Type::Scale3D},
        {WebCore::TransformOperation::Type::Translate, WebCore::TransformOperation::Type::Translate3D},
    };
    for (const auto& pair : sharedPrimitives) {
        if ((type == pair[0] || type == pair[1]) && (other == pair[0] || other == pair[1])) return pair[1];
    }
    return std::nullopt;
}

std::optional<WebCore::TransformOperation::Type> SharedPrimitiveType(
    const WebCore::TranslateTransformOperation& self, const WebCore::TransformOperation* other) {
    if (!other) return self.m_type;
    if (other->m_type == self.m_type) return self.m_type;
    if (!IsTranslateType(other->m_type)) return std::nullopt;
    const auto& otherTranslate = static_cast<const WebCore::TranslateTransformOperation&>(*other);
    return SharedPrimitiveTypeOfTypes(TranslatePrimitiveType(&self), TranslatePrimitiveType(&otherTranslate));
}

WebCore::Length BlendLength(const WebCore::Length& from, const WebCore::Length& to, double progress);

WebCore::Length BlendMixedTypes(const WebCore::Length& from, const WebCore::Length& to, double progress) {
    if (!to.isCalculated() && !from.isPercent() && (progress == 1.0 || IsZero(from))) {
        WebCore::Length zero{0, to.type()};
        return BlendLength(zero, to, progress);
    }
    if (!from.isCalculated() && !to.isPercent() && (progress == 0.0 || IsZero(to))) {
        WebCore::Length zero{0, from.type()};
        return BlendLength(from, zero, progress);
    }
    if (IsIntrinsicOrAuto(from) || IsIntrinsicOrAuto(to)
        || from.type() == WebCore::LengthType::Relative || to.type() == WebCore::LengthType::Relative) {
        return WebCore::Length{0, WebCore::LengthType::Fixed};
    }
    NotImplemented_nid_no_patch(__func__);
}

WebCore::Length BlendLength(const WebCore::Length& from, const WebCore::Length& to, double progress) {
    if ((from.isAuto() || to.isAuto())
        || from.type() == WebCore::LengthType::Undefined || to.type() == WebCore::LengthType::Undefined) {
        return progress < 0.5 ? from : to;
    }
    if (from.isCalculated() || to.isCalculated() || from.type() != to.type()) {
        return BlendMixedTypes(from, to, progress);
    }
    if (progress == 0.0) return from;
    if (progress == 1.0) return to;

    WebCore::LengthType resultType = to.type();
    if (IsZero(to)) resultType = from.type();

    if (resultType == WebCore::LengthType::Percent) {
        float fromPercent = IsZero(from) ? 0.0f : LengthValue(from);
        float toPercent = IsZero(to) ? 0.0f : LengthValue(to);
        return WebCore::Length{static_cast<float>(fromPercent + (toPercent - fromPercent) * progress), WebCore::LengthType::Percent};
    }

    float fromValue = IsZero(from) ? 0.0f : LengthValue(from);
    float toValue = IsZero(to) ? 0.0f : LengthValue(to);
    return WebCore::Length{static_cast<float>(fromValue + (toValue - fromValue) * progress), resultType};
}

TextStream& Text(TextStream& stream, const char* text) {
    return _ZN3WTF10TextStreamlsEPKc(&stream, text);
}

TextStream& RespectingIntegers(TextStream& stream, double value) {
    struct Restore {
        TextStream& stream;
        unsigned flags;
        ~Restore() { stream.formattingFlags = flags; }
    } restore{stream, stream.formattingFlags};
    stream.formattingFlags |= NumberRespectingIntegers;
    return _ZN3WTF10TextStreamlsEd(&stream, value);
}

void DumpType(TextStream& stream, WebCore::TransformOperation::Type type) {
    const char* name = nullptr;
    switch (type) {
    case WebCore::TransformOperation::Type::ScaleX: name = "scaleX"; break;
    case WebCore::TransformOperation::Type::ScaleY: name = "scaleY"; break;
    case WebCore::TransformOperation::Type::Scale: name = "scale"; break;
    case WebCore::TransformOperation::Type::TranslateX: name = "translateX"; break;
    case WebCore::TransformOperation::Type::TranslateY: name = "translateY"; break;
    case WebCore::TransformOperation::Type::Translate: name = "translate"; break;
    case WebCore::TransformOperation::Type::Rotate: name = "rotate"; break;
    case WebCore::TransformOperation::Type::SkewX: name = "skewX"; break;
    case WebCore::TransformOperation::Type::SkewY: name = "skewY"; break;
    case WebCore::TransformOperation::Type::Skew: name = "skew"; break;
    case WebCore::TransformOperation::Type::Matrix: name = "matrix"; break;
    case WebCore::TransformOperation::Type::ScaleZ: name = "scaleX"; break;
    case WebCore::TransformOperation::Type::Scale3D: name = "scale3d"; break;
    case WebCore::TransformOperation::Type::TranslateZ: name = "translateZ"; break;
    case WebCore::TransformOperation::Type::Translate3D: name = "translate3d"; break;
    case WebCore::TransformOperation::Type::RotateX: name = "rotateX"; break;
    case WebCore::TransformOperation::Type::RotateY: name = "rotateY"; break;
    case WebCore::TransformOperation::Type::RotateZ: name = "rotateZ"; break;
    case WebCore::TransformOperation::Type::Rotate3D: name = "rotate3d"; break;
    case WebCore::TransformOperation::Type::Matrix3D: name = "matrix3d"; break;
    case WebCore::TransformOperation::Type::Perspective: name = "perspective"; break;
    case WebCore::TransformOperation::Type::Identity: name = "identity"; break;
    case WebCore::TransformOperation::Type::None: name = "none"; break;
    }
    if (!name) APS5_INVALID_ARG_EX;
    Text(stream, name);
}

void DumpLength(TextStream& stream, const WebCore::Length& length) {
    switch (length.type()) {
    case WebCore::LengthType::Auto:
        Text(stream, "auto");
        break;
    case WebCore::LengthType::Content:
        Text(stream, "content");
        break;
    case WebCore::LengthType::Undefined:
        Text(stream, "undefined");
        break;
    case WebCore::LengthType::Fixed:
        RespectingIntegers(stream, LengthValue(length));
        Text(stream, "px");
        break;
    case WebCore::LengthType::Relative:
        Text(stream, "relative");
        Text(stream, " ");
        RespectingIntegers(stream, LengthValue(length));
        break;
    case WebCore::LengthType::Intrinsic:
        Text(stream, "intrinsic");
        Text(stream, " ");
        RespectingIntegers(stream, LengthValue(length));
        break;
    case WebCore::LengthType::MinIntrinsic:
        Text(stream, "min-intrinsic");
        Text(stream, " ");
        RespectingIntegers(stream, LengthValue(length));
        break;
    case WebCore::LengthType::MinContent:
        Text(stream, "min-content");
        Text(stream, " ");
        RespectingIntegers(stream, LengthValue(length));
        break;
    case WebCore::LengthType::MaxContent:
        Text(stream, "max-content");
        Text(stream, " ");
        RespectingIntegers(stream, LengthValue(length));
        break;
    case WebCore::LengthType::FillAvailable:
        Text(stream, "fill-available");
        Text(stream, " ");
        RespectingIntegers(stream, LengthValue(length));
        break;
    case WebCore::LengthType::FitContent:
        Text(stream, "fit-content");
        Text(stream, " ");
        RespectingIntegers(stream, LengthValue(length));
        break;
    case WebCore::LengthType::Percent:
        RespectingIntegers(stream, LengthValue(length));
        Text(stream, "%");
        break;
    case WebCore::LengthType::Calculated:
        NotImplemented_nid_no_patch(__func__);
    }
    if (length.m_hasQuirk) Text(stream, " has-quirk");
}

struct TranslateVtable {
    void (APS5_VABI* completeDestructor)(WebCore::TranslateTransformOperation*);
    void (APS5_VABI* deletingDestructor)(WebCore::TranslateTransformOperation*);
    WebCore::TransformOperationRef (APS5_VABI* clone)(const WebCore::TranslateTransformOperation*);
    WebCore::TransformOperationRef (APS5_VABI* selfOrCopyWithResolvedCalculatedValues)(WebCore::TranslateTransformOperation*, const WebCore::FloatSize&);
    bool (APS5_VABI* equal)(const WebCore::TranslateTransformOperation*, const WebCore::TransformOperation&);
    bool (APS5_VABI* isIdentity)(const WebCore::TranslateTransformOperation*);
    bool (APS5_VABI* apply)(const WebCore::TranslateTransformOperation*, WebCore::TransformationMatrix&, const WebCore::FloatSize&);
    WebCore::TransformOperationRef (APS5_VABI* blend)(WebCore::TranslateTransformOperation*, const WebCore::TransformOperation*, double, bool);
    WebCore::TransformOperation::Type (APS5_VABI* primitiveType)(const WebCore::TranslateTransformOperation*);
    bool (APS5_VABI* isAffectedByTransformOrigin)(const WebCore::TranslateTransformOperation*);
    bool (APS5_VABI* isRepresentableIn2D)(const WebCore::TranslateTransformOperation*);
    void (APS5_VABI* dump)(const WebCore::TranslateTransformOperation*, TextStream&);
};

struct TranslateVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    TranslateVtable dispatch;
};

WebCore::TransformOperationRef APS5_VABI SelfOrCopyWithResolvedCalculatedValues(WebCore::TranslateTransformOperation* self, const WebCore::FloatSize& borderBoxSize) {
    Checked(self, "selfOrCopyWithResolvedCalculatedValues");
    if (!self->m_x.isCalculated() && !self->m_y.isCalculated() && !self->m_z.isCalculated()) {
        ++self->m_refCount;
        return WebCore::TransformOperationRef{self};
    }
    WebCore::Length x{_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(self->m_x, borderBoxSize.m_width), WebCore::LengthType::Fixed};
    WebCore::Length y{_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(self->m_y, borderBoxSize.m_height), WebCore::LengthType::Fixed};
    return Create(x, y, self->m_z, self->m_type);
}

bool APS5_VABI IsAffectedByTransformOrigin(const WebCore::TranslateTransformOperation*) {
    return false;
}

const TranslateVtableStorage TranslateVtableInstance = {
    0,
    nullptr,
    {
        &_ZN7WebCore27TranslateTransformOperationD1Ev,
        &_ZN7WebCore27TranslateTransformOperationD0Ev,
        &_ZNK7WebCore27TranslateTransformOperation5cloneEv,
        &SelfOrCopyWithResolvedCalculatedValues,
        &_ZNK7WebCore27TranslateTransformOperationeqERKNS_18TransformOperationE,
        &_ZNK7WebCore27TranslateTransformOperation10isIdentityEv,
        &_ZNK7WebCore27TranslateTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE,
        &_ZN7WebCore27TranslateTransformOperation5blendEPKNS_18TransformOperationEdb,
        &TranslatePrimitiveType,
        &IsAffectedByTransformOrigin,
        &_ZNK7WebCore27TranslateTransformOperation19isRepresentableIn2DEv,
        &_ZNK7WebCore27TranslateTransformOperation4dumpERN3WTF10TextStreamE,
    },
};

}  // namespace

void WebCore::TransformOperation::deref() const {
    using DeletingDestructor = void (APS5_VABI*)(const TransformOperation*);
    if (!--m_refCount) {
        const void* slot = static_cast<const void* const*>(m_vptr)[1];
        reinterpret_cast<DeletingDestructor>(const_cast<void*>(slot))(this);
    }
}

extern "C" {

WebCore::TransformOperationRef APS5_VABI _ZN7WebCore27TranslateTransformOperation6createERKNS_6LengthES3_NS_18TransformOperation13OperationTypeE(const WebCore::Length& tx, const WebCore::Length& ty, WebCore::TransformOperation::Type type) {
    WebCore::Length zero{0, WebCore::LengthType::Fixed};
    return _ZN7WebCore27TranslateTransformOperation6createERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(tx, ty, zero, type);
}

WebCore::TransformOperationRef APS5_VABI _ZN7WebCore27TranslateTransformOperation6createERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(const WebCore::Length& tx, const WebCore::Length& ty, const WebCore::Length& tz, WebCore::TransformOperation::Type type) {
    return Create(tx, ty, tz, type);
}

void APS5_VABI _ZN7WebCore27TranslateTransformOperationC1ERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(WebCore::TranslateTransformOperation* self, const WebCore::Length& tx, const WebCore::Length& ty, const WebCore::Length& tz, WebCore::TransformOperation::Type type) {
    Checked(self, "TranslateTransformOperation");
    if (!IsTranslateType(type)) throw std::invalid_argument("WebCore::TranslateTransformOperation: non-translate operation type");
    if (AnyCalculated(tx, ty, tz)) NotImplemented_nid_no_patch(__func__);
    self->m_vptr = &TranslateVtableInstance.dispatch;
    self->m_refCount = 1;
    self->m_type = type;
    self->m_x = tx;
    self->m_y = ty;
    self->m_z = tz;
}

void APS5_VABI _ZN7WebCore27TranslateTransformOperationC2ERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(WebCore::TranslateTransformOperation* self, const WebCore::Length& tx, const WebCore::Length& ty, const WebCore::Length& tz, WebCore::TransformOperation::Type type) {
    _ZN7WebCore27TranslateTransformOperationC1ERKNS_6LengthES3_S3_NS_18TransformOperation13OperationTypeE(self, tx, ty, tz, type);
}

void APS5_VABI _ZN7WebCore27TranslateTransformOperationD2Ev(WebCore::TranslateTransformOperation* self) {
    Checked(self, "~TranslateTransformOperation");
    if (AnyCalculated(self->m_x, self->m_y, self->m_z)) NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN7WebCore27TranslateTransformOperationD1Ev(WebCore::TranslateTransformOperation* self) {
    _ZN7WebCore27TranslateTransformOperationD2Ev(self);
}

void APS5_VABI _ZN7WebCore27TranslateTransformOperationD0Ev(WebCore::TranslateTransformOperation* self) {
    _ZN7WebCore27TranslateTransformOperationD1Ev(self);
    _ZN3WTF8fastFreeEPv(self);
}

bool APS5_VABI _ZNK7WebCore27TranslateTransformOperation10isIdentityEv(const WebCore::TranslateTransformOperation* self) {
    CheckedConst(self, "isIdentity");
    return !_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(self->m_x, 1.0f)
        && !_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(self->m_y, 1.0f)
        && !_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(self->m_z, 1.0f);
}

bool APS5_VABI _ZNK7WebCore27TranslateTransformOperation19isRepresentableIn2DEv(const WebCore::TranslateTransformOperation* self) {
    CheckedConst(self, "isRepresentableIn2D");
    return IsZero(self->m_z);
}

float APS5_VABI _ZNK7WebCore27TranslateTransformOperation1xERKNS_9FloatSizeE(const WebCore::TranslateTransformOperation* self, const WebCore::FloatSize& borderBoxSize) {
    CheckedConst(self, "x");
    return _ZN7WebCore19floatValueForLengthERKNS_6LengthEf(self->m_x, borderBoxSize.m_width);
}

WebCore::Length APS5_VABI _ZNK7WebCore27TranslateTransformOperation1xEv(const WebCore::TranslateTransformOperation* self) {
    CheckedConst(self, "x");
    if (self->m_x.isCalculated()) NotImplemented_nid_no_patch(__func__);
    return self->m_x;
}

float APS5_VABI _ZNK7WebCore27TranslateTransformOperation1yERKNS_9FloatSizeE(const WebCore::TranslateTransformOperation* self, const WebCore::FloatSize& borderBoxSize) {
    CheckedConst(self, "y");
    return _ZN7WebCore19floatValueForLengthERKNS_6LengthEf(self->m_y, borderBoxSize.m_height);
}

WebCore::Length APS5_VABI _ZNK7WebCore27TranslateTransformOperation1yEv(const WebCore::TranslateTransformOperation* self) {
    CheckedConst(self, "y");
    if (self->m_y.isCalculated()) NotImplemented_nid_no_patch(__func__);
    return self->m_y;
}

float APS5_VABI _ZNK7WebCore27TranslateTransformOperation1zERKNS_9FloatSizeE(const WebCore::TranslateTransformOperation* self, const WebCore::FloatSize& borderBoxSize) {
    CheckedConst(self, "z");
    (void)borderBoxSize;
    return _ZN7WebCore19floatValueForLengthERKNS_6LengthEf(self->m_z, 1.0f);
}

WebCore::Length APS5_VABI _ZNK7WebCore27TranslateTransformOperation1zEv(const WebCore::TranslateTransformOperation* self) {
    CheckedConst(self, "z");
    if (self->m_z.isCalculated()) NotImplemented_nid_no_patch(__func__);
    return self->m_z;
}

void APS5_VABI _ZNK7WebCore27TranslateTransformOperation4dumpERN3WTF10TextStreamE(const WebCore::TranslateTransformOperation* self, TextStream& stream) {
    CheckedConst(self, "dump");
    DumpType(stream, self->m_type);
    Text(stream, "(");
    DumpLength(stream, self->m_x);
    Text(stream, ", ");
    DumpLength(stream, self->m_y);
    Text(stream, ", ");
    DumpLength(stream, self->m_z);
    Text(stream, ")");
}

bool APS5_VABI _ZNK7WebCore27TranslateTransformOperation5applyERNS_20TransformationMatrixERKNS_9FloatSizeE(const WebCore::TranslateTransformOperation* self, WebCore::TransformationMatrix& transform, const WebCore::FloatSize& borderBoxSize) {
    CheckedConst(self, "apply");
    _ZN7WebCore20TransformationMatrix11translate3dEddd(&transform,
        _ZNK7WebCore27TranslateTransformOperation1xERKNS_9FloatSizeE(self, borderBoxSize),
        _ZNK7WebCore27TranslateTransformOperation1yERKNS_9FloatSizeE(self, borderBoxSize),
        _ZNK7WebCore27TranslateTransformOperation1zERKNS_9FloatSizeE(self, borderBoxSize));
    return self->m_x.isPercent() || self->m_y.isPercent();
}

WebCore::TransformOperationRef APS5_VABI _ZNK7WebCore27TranslateTransformOperation5cloneEv(const WebCore::TranslateTransformOperation* self) {
    CheckedConst(self, "clone");
    if (AnyCalculated(self->m_x, self->m_y, self->m_z)) NotImplemented_nid_no_patch(__func__);
    return Create(self->m_x, self->m_y, self->m_z, self->m_type);
}

bool APS5_VABI _ZNK7WebCore27TranslateTransformOperationeqERKNS_18TransformOperationE(const WebCore::TranslateTransformOperation* self, const WebCore::TransformOperation& other) {
    CheckedConst(self, "operator==");
    if (!self->isSameType(other)) return false;
    const auto& otherTranslate = static_cast<const WebCore::TranslateTransformOperation&>(other);
    return LengthEqual(self->m_x, otherTranslate.m_x)
        && LengthEqual(self->m_y, otherTranslate.m_y)
        && LengthEqual(self->m_z, otherTranslate.m_z);
}

WebCore::TransformOperationRef APS5_VABI _ZN7WebCore27TranslateTransformOperation5blendEPKNS_18TransformOperationEdb(WebCore::TranslateTransformOperation* self, const WebCore::TransformOperation* from, double progress, bool blendToIdentity) {
    Checked(self, "blend");
    WebCore::Length zeroLength{0, WebCore::LengthType::Fixed};
    if (blendToIdentity) {
        return Create(BlendLength(self->m_x, zeroLength, progress), BlendLength(self->m_y, zeroLength, progress), BlendLength(self->m_z, zeroLength, progress), self->m_type);
    }
    auto outputType = SharedPrimitiveType(*self, from);
    if (!outputType) {
        ++self->m_refCount;
        return WebCore::TransformOperationRef{self};
    }
    const WebCore::TranslateTransformOperation* fromOp = nullptr;
    if (from && IsTranslateType(from->m_type)) {
        fromOp = static_cast<const WebCore::TranslateTransformOperation*>(from);
    }
    WebCore::Length fromX = fromOp ? fromOp->m_x : zeroLength;
    WebCore::Length fromY = fromOp ? fromOp->m_y : zeroLength;
    WebCore::Length fromZ = fromOp ? fromOp->m_z : zeroLength;
    return Create(BlendLength(fromX, self->m_x, progress), BlendLength(fromY, self->m_y, progress), BlendLength(fromZ, self->m_z, progress), *outputType);
}

}
