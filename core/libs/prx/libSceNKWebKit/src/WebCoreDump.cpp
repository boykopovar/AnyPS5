// Derived from Source/WebCore/editing/VisiblePosition.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WebCore/dom/Position.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WebCore/editing/VisibleSelection.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WebCore/page/scrolling/ScrollingCoordinatorTypes.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
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
 * Copyright (C) 2004, 2005, 2006, 2015 Apple Inc.  All rights reserved.
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
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreLayoutUnit.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::WtfAbi::TextStream;

constexpr unsigned SVGStyleRect = 1u << 0;
constexpr unsigned NumberRespectingIntegers = 1u << 1;
constexpr unsigned LayoutUnitsAsIntegers = 1u << 2;
constexpr int FixedPointDenominator = 64;
constexpr int LayoutUnitFractionalBits = 6;

[[noreturn]] void ThrowBadValue() {
    throw std::invalid_argument("invalid WebCore enumeration value");
}

TextStream& Text(TextStream& stream, const char* text) {
    return _ZN3WTF10TextStreamlsEPKc(&stream, text);
}

TextStream& Int(TextStream& stream, int value) {
    return _ZN3WTF10TextStreamlsEi(&stream, value);
}

TextStream& Float(TextStream& stream, float value) {
    return _ZN3WTF10TextStreamlsEf(&stream, value);
}

TextStream& Double(TextStream& stream, double value) {
    return _ZN3WTF10TextStreamlsEd(&stream, value);
}

bool HasFlag(const TextStream& stream, unsigned flag) {
    return (stream.formattingFlags & flag) != 0;
}

TextStream& RespectingIntegers(TextStream& stream, double value) {
    struct Restore {
        TextStream& stream;
        unsigned flags;
        ~Restore() { stream.formattingFlags = flags; }
    } restore{stream, stream.formattingFlags};
    stream.formattingFlags |= NumberRespectingIntegers;
    return Double(stream, value);
}

float ToFloat(WebCore::LayoutUnit value) {
    return static_cast<float>(value.m_value) / FixedPointDenominator;
}

int Round(WebCore::LayoutUnit value) {
    return WebCore::SaturatedSum(value.m_value, FixedPointDenominator / 2) >> LayoutUnitFractionalBits;
}

int SnapSize(WebCore::LayoutUnit size, WebCore::LayoutUnit location) {
    WebCore::LayoutUnit fraction{location.m_value % FixedPointDenominator};
    return Round(WebCore::LayoutUnit{WebCore::SaturatedSum(fraction.m_value, size.m_value)}) - Round(fraction);
}

const char* const kCompositeOperatorNames[] = {
    "clear", "copy", "source-over", "source-in", "source-out", "source-atop", "destination-over",
    "destination-in", "destination-out", "destination-atop", "xor", "darker", "lighter", "difference"
};

const char* const kBlendModeNames[] = {
    "normal", "multiply", "screen", "darken", "lighten", "overlay", "color-dodge", "color-burn", "hard-light",
    "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity", "plus-darker", "plus-lighter"
};

void DumpValue(TextStream& stream, int value) {
    Int(stream, value);
}

void DumpValue(TextStream& stream, bool value) {
    _ZN3WTF10TextStreamlsEb(&stream, value);
}

void DumpValue(TextStream& stream, const void* value) {
    _ZN3WTF10TextStreamlsEPKv(&stream, value);
}

void DumpValue(TextStream& stream, WebCore::ScrollElasticity value) {
    switch (value) {
    case WebCore::ScrollElasticity::Automatic:
        Int(stream, 0);
        break;
    case WebCore::ScrollElasticity::None:
        Int(stream, 1);
        break;
    case WebCore::ScrollElasticity::Allowed:
        Int(stream, 2);
        break;
    }
}

void DumpValue(TextStream& stream, WebCore::ScrollbarMode value) {
    switch (value) {
    case WebCore::ScrollbarMode::Auto:
        Int(stream, 0);
        break;
    case WebCore::ScrollbarMode::AlwaysOff:
        Int(stream, 1);
        break;
    case WebCore::ScrollbarMode::AlwaysOn:
        Int(stream, 2);
        break;
    }
}

void DumpValue(TextStream& stream, WebCore::NativeScrollbarVisibility value) {
    switch (value) {
    case WebCore::NativeScrollbarVisibility::Visible:
        Int(stream, 0);
        break;
    case WebCore::NativeScrollbarVisibility::HiddenByStyle:
        Int(stream, 1);
        break;
    case WebCore::NativeScrollbarVisibility::ReplacedByCustomScrollbar:
        Int(stream, 2);
        break;
    }
}

void DumpValue(TextStream& stream, const WebCore::Position& value);

template<typename T> void DumpProperty(TextStream& stream, const char* name, const T& value) {
    _ZN3WTF10TextStream10startGroupEv(&stream);
    Text(stream, name);
    Text(stream, " ");
    DumpValue(stream, value);
    _ZN3WTF10TextStream8endGroupEv(&stream);
}

void DumpValue(TextStream& stream, const WebCore::Position& value) {
    _ZN3WTF10TextStream10startGroupEv(&stream);
    Text(stream, "Position ");
    _ZN3WTF10TextStreamlsEPKv(&stream, &value);
    DumpProperty(stream, "anchor node", value.m_anchorNode);
    DumpProperty(stream, "offset", static_cast<int>(value.m_offset));
    _ZN3WTF10TextStream10startGroupEv(&stream);
    Text(stream, "anchor type ");
    switch (value.m_anchorTypeBits & 0x7u) {
    case 0:
        Text(stream, "offset in anchor");
        break;
    case 1:
        Text(stream, "before anchor");
        break;
    case 2:
        Text(stream, "after anchor");
        break;
    case 3:
        Text(stream, "before children");
        break;
    case 4:
        Text(stream, "after children");
        break;
    }
    _ZN3WTF10TextStream8endGroupEv(&stream);
    _ZN3WTF10TextStream8endGroupEv(&stream);
}

void DumpAffinity(TextStream& stream, std::uint8_t affinity) {
    switch (affinity) {
    case 0:
        Text(stream, "upstream");
        break;
    case 1:
        Text(stream, "downstream");
        break;
    }
}

}  // namespace

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(TextStream& stream, const WebCore::IntPoint& value) {
    Text(stream, "(");
    Int(stream, value.m_x);
    Text(stream, ",");
    Int(stream, value.m_y);
    return Text(stream, ")");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntSizeE(TextStream& stream, const WebCore::IntSize& value) {
    Text(stream, "width=");
    Int(stream, value.m_width);
    Text(stream, " height=");
    return Int(stream, value.m_height);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(TextStream& stream, const WebCore::IntRect& value) {
    if (HasFlag(stream, SVGStyleRect)) {
        Text(stream, "at (");
        Int(stream, value.m_location.m_x);
        Text(stream, ",");
        Int(stream, value.m_location.m_y);
        Text(stream, ") size ");
        Int(stream, value.m_size.m_width);
        Text(stream, "x");
        return Int(stream, value.m_size.m_height);
    }
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_8IntPointE(stream, value.m_location);
    Text(stream, " ");
    return _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntSizeE(stream, value.m_size);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE(TextStream& stream, const WebCore::FloatPoint& value) {
    Text(stream, "(");
    RespectingIntegers(stream, value.m_x);
    Text(stream, ",");
    RespectingIntegers(stream, value.m_y);
    return Text(stream, ")");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(TextStream& stream, const WebCore::FloatSize& value) {
    Text(stream, "width=");
    RespectingIntegers(stream, value.m_width);
    Text(stream, " height=");
    return RespectingIntegers(stream, value.m_height);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(TextStream& stream, const WebCore::FloatRect& value) {
    if (HasFlag(stream, SVGStyleRect)) {
        Text(stream, "at (");
        RespectingIntegers(stream, value.m_location.m_x);
        Text(stream, ",");
        RespectingIntegers(stream, value.m_location.m_y);
        Text(stream, ") size ");
        RespectingIntegers(stream, value.m_size.m_width);
        Text(stream, "x");
        return RespectingIntegers(stream, value.m_size.m_height);
    }
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE(stream, value.m_location);
    Text(stream, " ");
    return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(stream, value.m_size);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_12FloatPoint3DE(TextStream& stream, const WebCore::FloatPoint3D& value) {
    Float(stream, value.m_x);
    Text(stream, " ");
    Float(stream, value.m_y);
    Text(stream, " ");
    return Float(stream, value.m_z);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutUnitE(TextStream& stream, const WebCore::LayoutUnit& value) {
    return RespectingIntegers(stream, ToFloat(value));
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutSizeE(TextStream& stream, const WebCore::LayoutSize& value) {
    Text(stream, "width=");
    Float(stream, ToFloat(value.m_width));
    Text(stream, " height=");
    return Float(stream, ToFloat(value.m_height));
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_11LayoutPointE(TextStream& stream, const WebCore::LayoutPoint& value) {
    Text(stream, "(");
    if (HasFlag(stream, LayoutUnitsAsIntegers)) {
        Int(stream, value.m_x.m_value / FixedPointDenominator);
        Text(stream, ",");
        Int(stream, value.m_y.m_value / FixedPointDenominator);
    } else {
        Float(stream, ToFloat(value.m_x));
        Text(stream, ",");
        Float(stream, ToFloat(value.m_y));
    }
    return Text(stream, ")");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_10LayoutRectE(TextStream& stream, const WebCore::LayoutRect& value) {
    if (HasFlag(stream, LayoutUnitsAsIntegers)) {
        WebCore::IntRect snapped{
            WebCore::IntPoint{Round(value.m_location.m_x), Round(value.m_location.m_y)},
            WebCore::IntSize{SnapSize(value.m_size.m_width, value.m_location.m_x), SnapSize(value.m_size.m_height, value.m_location.m_y)}};
        return _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(stream, snapped);
    }
    WebCore::FloatRect rect{
        WebCore::FloatPoint{ToFloat(value.m_location.m_x), ToFloat(value.m_location.m_y)},
        WebCore::FloatSize{ToFloat(value.m_size.m_width), ToFloat(value.m_size.m_height)}};
    return _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(stream, rect);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_17CompositeOperatorE(TextStream& stream, WebCore::CompositeOperator value) {
    auto index = static_cast<unsigned>(value);
    if (index >= sizeof(kCompositeOperatorNames) / sizeof(kCompositeOperatorNames[0])) ThrowBadValue();
    return Text(stream, kCompositeOperatorNames[index]);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_9BlendModeE(TextStream& stream, WebCore::BlendMode value) {
    auto index = static_cast<unsigned>(value) - static_cast<unsigned>(WebCore::BlendMode::Normal);
    if (index >= sizeof(kBlendModeNames) / sizeof(kBlendModeNames[0])) ThrowBadValue();
    return Text(stream, kBlendModeNames[index]);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_8WindRuleE(TextStream& stream, WebCore::WindRule value) {
    switch (value) {
    case WebCore::WindRule::NonZero:
        Text(stream, "NON-ZERO");
        break;
    case WebCore::WindRule::EvenOdd:
        Text(stream, "EVEN-ODD");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_7LineCapE(TextStream& stream, WebCore::LineCap value) {
    switch (value) {
    case WebCore::LineCap::Butt:
        Text(stream, "BUTT");
        break;
    case WebCore::LineCap::Round:
        Text(stream, "ROUND");
        break;
    case WebCore::LineCap::Square:
        Text(stream, "SQUARE");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_8LineJoinE(TextStream& stream, WebCore::LineJoin value) {
    switch (value) {
    case WebCore::LineJoin::Miter:
        Text(stream, "MITER");
        break;
    case WebCore::LineJoin::Round:
        Text(stream, "ROUND");
        break;
    case WebCore::LineJoin::Bevel:
        Text(stream, "BEVEL");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_10ColorSpaceE(TextStream& stream, WebCore::ColorSpace value) {
    using WebCore::ColorSpace;
    switch (value) {
    case ColorSpace::A98RGB: return Text(stream, "A98-RGB");
    case ColorSpace::DisplayP3: return Text(stream, "DisplayP3");
    case ColorSpace::ExtendedA98RGB: return Text(stream, "Extended A98-RGB");
    case ColorSpace::ExtendedDisplayP3: return Text(stream, "Extended DisplayP3");
    case ColorSpace::ExtendedLinearSRGB: return Text(stream, "Extended Linear sRGB");
    case ColorSpace::ExtendedProPhotoRGB: return Text(stream, "Extended ProPhotoRGB");
    case ColorSpace::ExtendedRec2020: return Text(stream, "Extended Rec2020");
    case ColorSpace::ExtendedSRGB: return Text(stream, "Extended sRGB");
    case ColorSpace::HSL: return Text(stream, "HSL");
    case ColorSpace::HWB: return Text(stream, "HWB");
    case ColorSpace::LCH: return Text(stream, "LCH");
    case ColorSpace::Lab: return Text(stream, "Lab");
    case ColorSpace::LinearSRGB: return Text(stream, "Linear sRGB");
    case ColorSpace::OKLCH: return Text(stream, "OKLCH");
    case ColorSpace::OKLab: return Text(stream, "OKLab");
    case ColorSpace::ProPhotoRGB: return Text(stream, "ProPhotoRGB");
    case ColorSpace::Rec2020: return Text(stream, "Rec2020");
    case ColorSpace::SRGB: return Text(stream, "sRGB");
    case ColorSpace::XYZ_D50: return Text(stream, "XYZ-D50");
    case ColorSpace::XYZ_D65: return Text(stream, "XYZ-D50");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_22AlphaPremultiplicationE(TextStream& stream, WebCore::AlphaPremultiplication value) {
    switch (value) {
    case WebCore::AlphaPremultiplication::Premultiplied:
        Text(stream, "premultiplied");
        break;
    case WebCore::AlphaPremultiplication::Unpremultiplied:
        Text(stream, "unpremultiplied");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_10ScrollTypeE(TextStream& stream, WebCore::ScrollType value) {
    switch (value) {
    case WebCore::ScrollType::User:
        Text(stream, "user");
        break;
    case WebCore::ScrollType::Programmatic:
        Text(stream, "programmatic");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_14ScrollClampingE(TextStream& stream, WebCore::ScrollClamping value) {
    switch (value) {
    case WebCore::ScrollClamping::Unclamped:
        Text(stream, "unclamped");
        break;
    case WebCore::ScrollClamping::Clamped:
        Text(stream, "clamped");
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_30ScrollBehaviorForFixedElementsE(TextStream& stream, WebCore::ScrollBehaviorForFixedElements value) {
    switch (value) {
    case WebCore::ScrollBehaviorForFixedElements::StickToDocumentBounds:
        Int(stream, 0);
        break;
    case WebCore::ScrollBehaviorForFixedElements::StickToViewportBounds:
        Int(stream, 1);
        break;
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(TextStream& stream, WebCore::ScrollAlignment::Behavior value) {
    using Behavior = WebCore::ScrollAlignment::Behavior;
    switch (value) {
    case Behavior::NoScroll: return Text(stream, "NoScroll");
    case Behavior::AlignCenter: return Text(stream, "AlignCenter");
    case Behavior::AlignTop: return Text(stream, "AlignTop");
    case Behavior::AlignBottom: return Text(stream, "AlignBottom");
    case Behavior::AlignLeft: return Text(stream, "AlignLeft");
    case Behavior::AlignRight: return Text(stream, "AlignRight");
    case Behavior::AlignToClosestEdge: return Text(stream, "AlignToClosestEdge");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15ScrollAlignmentE(TextStream& stream, const WebCore::ScrollAlignment& value) {
    Text(stream, "ScrollAlignment: visible: ");
    _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(stream, value.m_rectVisible);
    Text(stream, " hidden: ");
    _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(stream, value.m_rectHidden);
    Text(stream, " partial: ");
    return _ZN7WebCorelsERN3WTF10TextStreamENS_15ScrollAlignment8BehaviorE(stream, value.m_rectPartial);
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15AffineTransformE(TextStream& stream, const WebCore::AffineTransform& value) {
    const auto& m = value.m_transform;
    if (m[0] == 1 && m[1] == 0 && m[2] == 0 && m[3] == 1 && m[4] == 0 && m[5] == 0) return Text(stream, "identity");
    Text(stream, "{m=((");
    Double(stream, m[0]);
    Text(stream, ",");
    Double(stream, m[1]);
    Text(stream, ")(");
    Double(stream, m[2]);
    Text(stream, ",");
    Double(stream, m[3]);
    Text(stream, ")) t=(");
    Double(stream, m[4]);
    Text(stream, ",");
    Double(stream, m[5]);
    return Text(stream, ")}");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_20TransformationMatrixE(TextStream& stream, const WebCore::TransformationMatrix& value) {
    struct IndentScope {
        TextStream& stream;
        explicit IndentScope(TextStream& s) : stream(s) { ++stream.indent; }
        ~IndentScope() { --stream.indent; }
    } scope{stream};
    Text(stream, "\n");
    for (int row = 0; row < 4; ++row) {
        _ZN3WTF10TextStream11writeIndentEv(&stream);
        Text(stream, "[");
        for (int column = 0; column < 4; ++column) {
            if (column) Text(stream, " ");
            Double(stream, value.m_matrix[row][column]);
        }
        Text(stream, row < 3 ? "]\n" : "]");
    }
    return stream;
}


NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_17ScrollingNodeTypeE(TextStream& stream, WebCore::ScrollingNodeType value) {
    using WebCore::ScrollingNodeType;
    switch (value) {
    case ScrollingNodeType::MainFrame: return Text(stream, "main-frame-scrolling");
    case ScrollingNodeType::Subframe: return Text(stream, "subframe-scrolling");
    case ScrollingNodeType::FrameHosting: return Text(stream, "frame-hosting");
    case ScrollingNodeType::Overflow: return Text(stream, "overflow-scrolling");
    case ScrollingNodeType::OverflowProxy: return Text(stream, "overflow-scroll-proxy");
    case ScrollingNodeType::Fixed: return Text(stream, "fixed");
    case ScrollingNodeType::Sticky: return Text(stream, "sticky");
    case ScrollingNodeType::Positioned: return Text(stream, "positioned");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_28ScrollingLayerPositionActionE(TextStream& stream, WebCore::ScrollingLayerPositionAction value) {
    using WebCore::ScrollingLayerPositionAction;
    switch (value) {
    case ScrollingLayerPositionAction::Set: return Text(stream, "set");
    case ScrollingLayerPositionAction::SetApproximate: return Text(stream, "set approximate");
    case ScrollingLayerPositionAction::Sync: return Text(stream, "sync");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_21ViewportRectStabilityE(TextStream& stream, WebCore::ViewportRectStability value) {
    using WebCore::ViewportRectStability;
    switch (value) {
    case ViewportRectStability::Stable: return Text(stream, "stable");
    case ViewportRectStability::Unstable: return Text(stream, "unstable");
    case ViewportRectStability::ChangingObscuredInsetsInteractively: return Text(stream, "changing obscured insets interactively");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_26GraphicsLayerPaintingPhaseE(TextStream& stream, WebCore::GraphicsLayerPaintingPhase value) {
    using WebCore::GraphicsLayerPaintingPhase;
    switch (value) {
    case GraphicsLayerPaintingPhase::Background: return Text(stream, "background");
    case GraphicsLayerPaintingPhase::Foreground: return Text(stream, "foreground");
    case GraphicsLayerPaintingPhase::Mask: return Text(stream, "mask");
    case GraphicsLayerPaintingPhase::ClipPath: return Text(stream, "clip-path");
    case GraphicsLayerPaintingPhase::OverflowContents: return Text(stream, "overflow-contents");
    case GraphicsLayerPaintingPhase::CompositedScroll: return Text(stream, "composited-scroll");
    case GraphicsLayerPaintingPhase::ChildClippingMask: return Text(stream, "child-clipping-mask");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_25ScrollPositioningBehaviorE(TextStream& stream, WebCore::ScrollPositioningBehavior value) {
    using WebCore::ScrollPositioningBehavior;
    switch (value) {
    case ScrollPositioningBehavior::None: return Text(stream, "none");
    case ScrollPositioningBehavior::Stationary: return Text(stream, "stationary");
    case ScrollPositioningBehavior::Moves: return Text(stream, "moves");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_13GraphicsLayer16CustomAppearanceE(TextStream& stream, const WebCore::GraphicsLayer::CustomAppearance& value) {
    using WebCore::GraphicsLayer;
    switch (value) {
    case GraphicsLayer::CustomAppearance::None: return Text(stream, "none");
    case GraphicsLayer::CustomAppearance::ScrollingOverhang: return Text(stream, "scrolling-overhang");
    case GraphicsLayer::CustomAppearance::ScrollingShadow: return Text(stream, "scrolling-shadow");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS0_6VectorImLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(TextStream& stream, const NKWebKit::WtfAbi::Vector<std::uint64_t>& vector) {
    std::vector<std::uint64_t> items(vector.size);
    for (unsigned index = 0; index < vector.size;) {
        unsigned chunk = vector.size - index;
        if (chunk > 64) chunk = 64;
        if (!GuestCopyIn_nid_no_patch(items.data() + index, vector.buffer + index, chunk * sizeof(items[0]))) APS5_INVALID_ARG_EX;
        index += chunk;
    }
    Text(stream, "[");
    unsigned count = 0;
    for (unsigned index = 0; index < vector.size; ++index) {
        if (count) Text(stream, ", ");
        _ZN3WTF10TextStreamlsEm(&stream, items[index]);
        ++count;
        if (count == stream.containerSizeLimit) break;
    }
    if (count != vector.size) Text(stream, ", ...");
    return Text(stream, "]");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE(TextStream& stream, WebCore::ScrollableAreaParameters value) {
    DumpProperty(stream, "horizontal scroll elasticity", value.horizontalScrollElasticity);
    DumpProperty(stream, "vertical scroll elasticity", value.verticalScrollElasticity);
    DumpProperty(stream, "horizontal scrollbar mode", value.horizontalScrollbarMode);
    DumpProperty(stream, "vertical scrollbar mode", value.verticalScrollbarMode);
    if (value.allowsHorizontalScrolling) DumpProperty(stream, "allows horizontal scrolling", value.allowsHorizontalScrolling);
    if (value.allowsVerticalScrolling) DumpProperty(stream, "allows vertical scrolling", value.allowsVerticalScrolling);
    if (value.horizontalNativeScrollbarVisibility == WebCore::NativeScrollbarVisibility::HiddenByStyle)
        DumpProperty(stream, "horizontal scrollbar hidden by style", value.horizontalNativeScrollbarVisibility);
    if (value.verticalNativeScrollbarVisibility == WebCore::NativeScrollbarVisibility::HiddenByStyle)
        DumpProperty(stream, "vertical scrollbar hidden by style", value.verticalNativeScrollbarVisibility);
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_15VisiblePositionE(TextStream& stream, const WebCore::VisiblePosition& visiblePosition) {
    _ZN3WTF10TextStream10startGroupEv(&stream);
    Text(stream, "VisiblePosition ");
    _ZN3WTF10TextStreamlsEPKv(&stream, &visiblePosition);
    WebCore::Position position = visiblePosition.m_deepPosition;
    DumpProperty(stream, "position", position);
    _ZN3WTF10TextStream10startGroupEv(&stream);
    Text(stream, "affinity ");
    DumpAffinity(stream, visiblePosition.m_affinity);
    _ZN3WTF10TextStream8endGroupEv(&stream);
    _ZN3WTF10TextStream8endGroupEv(&stream);
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_16VisibleSelectionE(TextStream& stream, const WebCore::VisibleSelection& selection) {
    _ZN3WTF10TextStream10startGroupEv(&stream);
    Text(stream, "VisibleSelection ");
    _ZN3WTF10TextStreamlsEPKv(&stream, &selection);
    DumpProperty(stream, "base", selection.m_base);
    DumpProperty(stream, "extent", selection.m_extent);
    DumpProperty(stream, "start", selection.m_start);
    DumpProperty(stream, "end", selection.m_end);
    _ZN3WTF10TextStream8endGroupEv(&stream);
    return stream;
}
}
