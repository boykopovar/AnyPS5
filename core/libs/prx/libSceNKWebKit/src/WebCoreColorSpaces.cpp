/*
 * Copyright (C) 2017, 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Derived from Source/WebCore/platform/graphics/ColorConversion.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/ColorSerialization.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/ColorTypes.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/graphics/ColorMatrix.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ColorSpaceExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace {

using NKWebKit::WtfAbi::String;

struct Components {
    std::array<float, 4> values{};
};

using Matrix3 = std::array<float, 9>;

constexpr Matrix3 XyzToLinearSrgb{
    3.2404542f, -1.5371385f, -0.4985314f,
    -0.9692660f, 1.8760108f, 0.0415560f,
    0.0556434f, -0.2040259f, 1.0572252f,
};

constexpr Matrix3 LinearSrgbToXyz{
    0.4124564f, 0.3575761f, 0.1804375f,
    0.2126729f, 0.7151522f, 0.0721750f,
    0.0193339f, 0.1191920f, 0.9503041f,
};

constexpr Matrix3 XyzToLinearDisplayP3{
    2.493496911941425f, -0.9313836179191239f, -0.4027107844507168f,
    -0.8294889695615747f, 1.7626640603183463f, 0.0236246858419436f,
    0.0358458302437845f, -0.0761723892680418f, 0.9568845240076872f,
};

constexpr Matrix3 LinearDisplayP3ToXyz{
    0.4865709486482162f, 0.2656676931690931f, 0.198217285234363f,
    0.2289745640697488f, 0.6917385218365064f, 0.079286914093745f,
    0.0f, 0.0451133818589026f, 1.043944368900976f,
};

Components Transform(const Matrix3& matrix, const Components& input) {
    Components result;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            float product = matrix[row * 3 + column] * input.values[column];
            result.values[row] += product;
        }
    }
    result.values[3] = input.values[3];
    return result;
}

float ClampUnit(float value) {
    if (value >= 1.0f) return 1.0f;
    if (value <= 0.0f) return 0.0f;
    return value;
}

float LinearToRgbComponent(float c) {
    if (c < 0.0031308f) return 12.92f * c;
    return ClampUnit(1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
}

float RgbToLinearComponent(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    return ClampUnit(std::pow((c + 0.055f) / 1.055f, 2.4f));
}

template<typename TColor> Components ToComponents(const TColor& color) {
    return Components{{color.red, color.green, color.blue, color.alpha}};
}

template<typename TColor> TColor FromComponents(const Components& components) {
    return TColor{components.values[0], components.values[1], components.values[2], components.values[3]};
}

template<typename TResult, typename TSource> TResult Linearize(const TSource& color) {
    return TResult{RgbToLinearComponent(color.red), RgbToLinearComponent(color.green), RgbToLinearComponent(color.blue), color.alpha};
}

template<typename TResult, typename TSource> TResult Delinearize(const TSource& color) {
    return TResult{LinearToRgbComponent(color.red), LinearToRgbComponent(color.green), LinearToRgbComponent(color.blue), color.alpha};
}

float CalcHue(float temp1, float temp2, float hueValue) {
    if (hueValue < 0.0f)
        hueValue += 6.0f;
    else if (hueValue >= 6.0f)
        hueValue -= 6.0f;
    if (hueValue < 1.0f) return temp1 + (temp2 - temp1) * hueValue;
    if (hueValue < 3.0f) return temp2;
    if (hueValue < 4.0f) return temp1 + (temp2 - temp1) * (4.0f - hueValue);
    return temp1;
}

String MakeString(const std::string& text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

char DecimalDigit(unsigned number) {
    return static_cast<char>('0' + number);
}

std::string FractionDigitsForFractionalAlphaValue(std::uint8_t alpha) {
    unsigned value = alpha;
    if (((value * 100 + 0x7F) / 0xFF * 0xFF + 50) / 100 != value)
        return {DecimalDigit(value * 10 / 0xFF % 10), DecimalDigit(value * 100 / 0xFF % 10), DecimalDigit((value * 1000 + 0x7F) / 0xFF % 10)};
    if (unsigned thirdDigit = (value * 100 + 0x7F) / 0xFF % 10)
        return {DecimalDigit(value * 10 / 0xFF), DecimalDigit(thirdDigit)};
    return {DecimalDigit((value * 10 + 0x7F) / 0xFF)};
}

std::string Hex2(std::uint8_t value, bool lowercase) {
    const char* digits = lowercase ? "0123456789abcdef" : "0123456789ABCDEF";
    return {digits[value >> 4], digits[value & 0xF]};
}

String SerializeCss(WebCore::SRGBA<std::uint8_t> color) {
    std::string prefix = std::to_string(color.red) + ", " + std::to_string(color.green) + ", " + std::to_string(color.blue);
    switch (color.alpha) {
    case 0:
        return MakeString("rgba(" + prefix + ", 0)");
    case 0xFF:
        return MakeString("rgb(" + prefix + ")");
    default:
        return MakeString("rgba(" + prefix + ", 0." + FractionDigitsForFractionalAlphaValue(color.alpha) + ")");
    }
}

bool AreEssentiallyEqual(float u, float v) {
    if (u == v) return true;
    constexpr float epsilon = std::numeric_limits<float>::epsilon();
    auto safeDivision = [](float dividend, float divisor) {
        if (divisor < 1 && dividend > divisor * std::numeric_limits<float>::max()) return std::numeric_limits<float>::max();
        if (divisor > 1 && dividend < divisor * std::numeric_limits<float>::min()) return 0.0f;
        return dividend / divisor;
    };
    const float delta = std::abs(u - v);
    return safeDivision(delta, std::abs(u)) <= epsilon && safeDivision(delta, std::abs(v)) <= epsilon;
}

template<typename TColor> String SerializeColorFunction(const TColor& color, const char* colorSpace) {
    using NKWebKit::WtfAbi::NumberToString;
    std::string text = std::string("color(") + colorSpace + ' ' + NumberToString(color.red) + ' ' + NumberToString(color.green) + ' ' + NumberToString(color.blue);
    if (!AreEssentiallyEqual(color.alpha, 1.0f)) text += " / " + NumberToString(color.alpha);
    text += ')';
    return MakeString(text);
}

}  // namespace

extern "C" {

WebCore::LinearSRGBA<float> APS5_VABI _ZN7WebCore13toLinearSRGBAERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color) {
    return Linearize<WebCore::LinearSRGBA<float>>(color);
}

WebCore::SRGBA<float> APS5_VABI _ZN7WebCore7toSRGBAERKNS_11LinearSRGBAIfEE(const WebCore::LinearSRGBA<float>& color) {
    return Delinearize<WebCore::SRGBA<float>>(color);
}

WebCore::LinearDisplayP3<float> APS5_VABI _ZN7WebCore17toLinearDisplayP3ERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color) {
    return Linearize<WebCore::LinearDisplayP3<float>>(color);
}

WebCore::DisplayP3<float> APS5_VABI _ZN7WebCore11toDisplayP3ERKNS_15LinearDisplayP3IfEE(const WebCore::LinearDisplayP3<float>& color) {
    return Delinearize<WebCore::DisplayP3<float>>(color);
}

WebCore::SRGBA<float> APS5_VABI _ZN7WebCore7toSRGBAERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color) {
    auto linearDisplayP3 = Linearize<WebCore::LinearDisplayP3<float>>(color);
    auto xyz = Transform(LinearDisplayP3ToXyz, ToComponents(linearDisplayP3));
    auto linearSrgb = FromComponents<WebCore::LinearSRGBA<float>>(Transform(XyzToLinearSrgb, xyz));
    return Delinearize<WebCore::SRGBA<float>>(linearSrgb);
}

WebCore::DisplayP3<float> APS5_VABI _ZN7WebCore11toDisplayP3ERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color) {
    auto linearSrgb = Linearize<WebCore::LinearSRGBA<float>>(color);
    auto xyz = Transform(LinearSrgbToXyz, ToComponents(linearSrgb));
    auto linearDisplayP3 = FromComponents<WebCore::LinearDisplayP3<float>>(Transform(XyzToLinearDisplayP3, xyz));
    return Delinearize<WebCore::DisplayP3<float>>(linearDisplayP3);
}

WebCore::HSLA<float> APS5_VABI _ZN7WebCore6toHSLAERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color) {
    const float r = color.red;
    const float g = color.green;
    const float b = color.blue;
    const auto [lowest, highest] = std::minmax({r, g, b});
    const float chroma = highest - lowest;

    float hue;
    if (!chroma)
        hue = 0;
    else if (highest == r)
        hue = (60.0f * ((g - b) / chroma)) + 360.0f;
    else if (highest == g)
        hue = (60.0f * ((b - r) / chroma)) + 120.0f;
    else
        hue = (60.0f * ((r - g) / chroma)) + 240.0f;

    if (hue >= 360.0f) hue -= 360.0f;
    hue /= 360.0f;

    const float lightness = 0.5f * (highest + lowest);
    float saturation;
    if (!chroma)
        saturation = 0;
    else if (lightness <= 0.5f)
        saturation = chroma / (highest + lowest);
    else
        saturation = chroma / (2.0f - (highest + lowest));

    return {hue, saturation, lightness, color.alpha};
}

WebCore::SRGBA<float> APS5_VABI _ZN7WebCore7toSRGBAERKNS_4HSLAIfEE(const WebCore::HSLA<float>& color) {
    const float saturation = color.saturation;
    const float lightness = color.lightness;
    if (!saturation) return {lightness, lightness, lightness, color.alpha};

    const float temp2 = lightness <= 0.5f ? lightness * (1.0f + saturation) : lightness + saturation - lightness * saturation;
    const float temp1 = 2.0f * lightness - temp2;
    const float hue = color.hue * 6.0f;
    return {CalcHue(temp1, temp2, hue + 2.0f), CalcHue(temp1, temp2, hue), CalcHue(temp1, temp2, hue - 2.0f), color.alpha};
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore19serializationForCSSENS_5SRGBAIhEE(WebCore::SRGBA<std::uint8_t> color) {
    return SerializeCss(color);
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20serializationForHTMLENS_5SRGBAIhEE(WebCore::SRGBA<std::uint8_t> color) {
    if (color.alpha == 0xFF) return MakeString('#' + Hex2(color.red, true) + Hex2(color.green, true) + Hex2(color.blue, true));
    return SerializeCss(color);
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore32serializationForRenderTreeAsTextENS_5SRGBAIhEE(WebCore::SRGBA<std::uint8_t> color) {
    std::string text = '#' + Hex2(color.red, false) + Hex2(color.green, false) + Hex2(color.blue, false);
    if (color.alpha < 0xFF) text += Hex2(color.alpha, false);
    return MakeString(text);
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore19serializationForCSSERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color) {
    return SerializeColorFunction(color, "srgb");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20serializationForHTMLERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color) {
    return SerializeColorFunction(color, "srgb");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore32serializationForRenderTreeAsTextERKNS_5SRGBAIfEE(const WebCore::SRGBA<float>& color) {
    return SerializeColorFunction(color, "srgb");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore19serializationForCSSERKNS_11LinearSRGBAIfEE(const WebCore::LinearSRGBA<float>& color) {
    return SerializeColorFunction(color, "linear-srgb");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20serializationForHTMLERKNS_11LinearSRGBAIfEE(const WebCore::LinearSRGBA<float>& color) {
    return SerializeColorFunction(color, "linear-srgb");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore32serializationForRenderTreeAsTextERKNS_11LinearSRGBAIfEE(const WebCore::LinearSRGBA<float>& color) {
    return SerializeColorFunction(color, "linear-srgb");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore19serializationForCSSERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color) {
    return SerializeColorFunction(color, "display-p3");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20serializationForHTMLERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color) {
    return SerializeColorFunction(color, "display-p3");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore32serializationForRenderTreeAsTextERKNS_9DisplayP3IfEE(const WebCore::DisplayP3<float>& color) {
    return SerializeColorFunction(color, "display-p3");
}

}
