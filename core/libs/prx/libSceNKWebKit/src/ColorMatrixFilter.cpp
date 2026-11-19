// Derived from WebKit/WebKit/Source/WebCore/platform/graphics/filters/FilterOperation.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
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

// Derived from WebKit/WebKit/Source/WebCore/platform/graphics/ColorMatrix.h@73a35b43be7ae44c084885bd3fa4b403e59723c8
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ColorMatrixFilterExports.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace {

using Matrix = std::array<float, 9>;

constexpr float PiFloat = 3.14159265358979323846f;

Matrix GrayscaleColorMatrix(float amount) {
    const float oneMinusAmount = std::clamp(1.0f - amount, 0.0f, 1.0f);
    return {
        0.2126f + 0.7874f * oneMinusAmount, 0.7152f - 0.7152f * oneMinusAmount, 0.0722f - 0.0722f * oneMinusAmount,
        0.2126f - 0.2126f * oneMinusAmount, 0.7152f + 0.2848f * oneMinusAmount, 0.0722f - 0.0722f * oneMinusAmount,
        0.2126f - 0.2126f * oneMinusAmount, 0.7152f - 0.7152f * oneMinusAmount, 0.0722f + 0.9278f * oneMinusAmount,
    };
}

Matrix SepiaColorMatrix(float amount) {
    const float oneMinusAmount = std::clamp(1.0f - amount, 0.0f, 1.0f);
    return {
        0.393f + 0.607f * oneMinusAmount, 0.769f - 0.769f * oneMinusAmount, 0.189f - 0.189f * oneMinusAmount,
        0.349f - 0.349f * oneMinusAmount, 0.686f + 0.314f * oneMinusAmount, 0.168f - 0.168f * oneMinusAmount,
        0.272f - 0.272f * oneMinusAmount, 0.534f - 0.534f * oneMinusAmount, 0.131f + 0.869f * oneMinusAmount,
    };
}

Matrix SaturationColorMatrix(float amount) {
    return {
        0.213f + 0.787f * amount, 0.715f - 0.715f * amount, 0.072f - 0.072f * amount,
        0.213f - 0.213f * amount, 0.715f + 0.285f * amount, 0.072f - 0.072f * amount,
        0.213f - 0.213f * amount, 0.715f - 0.715f * amount, 0.072f + 0.928f * amount,
    };
}

Matrix HueRotateColorMatrix(float angleInDegrees) {
    const float radians = angleInDegrees * PiFloat / 180.0f;
    const float cosHue = std::cos(radians);
    const float sinHue = std::sin(radians);
    return {
        0.213f + cosHue * 0.787f - sinHue * 0.213f, 0.715f - cosHue * 0.715f - sinHue * 0.715f, 0.072f - cosHue * 0.072f + sinHue * 0.928f,
        0.213f - cosHue * 0.213f + sinHue * 0.143f, 0.715f + cosHue * 0.285f + sinHue * 0.140f, 0.072f - cosHue * 0.072f - sinHue * 0.283f,
        0.213f - cosHue * 0.213f - sinHue * 0.787f, 0.715f - cosHue * 0.715f + sinHue * 0.715f, 0.072f + cosHue * 0.928f + sinHue * 0.072f,
    };
}

void Transform(const Matrix& matrix, WebCore::SRGBA<float>& color) {
    const std::array<float, 3> input{color.red, color.green, color.blue};
    std::array<float, 3> result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column)
            result[row] += matrix[row * 3 + column] * input[column];
    }
    color.red = result[0];
    color.green = result[1];
    color.blue = result[2];
}

}  // namespace

extern "C" {

bool APS5_VABI _ZNK7WebCore31BasicColorMatrixFilterOperation14transformColorERNS_5SRGBAIfEE(const NKWebKit::ColorMatrixFilterOperationAbi* self, WebCore::SRGBA<float>* colorPointer) {
    if (!self) throw std::invalid_argument("BasicColorMatrixFilterOperation::transformColor: null this");
    if (!colorPointer) throw std::invalid_argument("BasicColorMatrixFilterOperation::transformColor: null color");
    const float amount = static_cast<float>(self->amount);
    switch (self->type) {
    case NKWebKit::LegacyFilterOperationType::Grayscale:
        Transform(GrayscaleColorMatrix(amount), *colorPointer);
        return true;
    case NKWebKit::LegacyFilterOperationType::Sepia:
        Transform(SepiaColorMatrix(amount), *colorPointer);
        return true;
    case NKWebKit::LegacyFilterOperationType::HueRotate:
        Transform(HueRotateColorMatrix(amount), *colorPointer);
        return true;
    case NKWebKit::LegacyFilterOperationType::Saturate:
        Transform(SaturationColorMatrix(amount), *colorPointer);
        return true;
    default:
        return false;
    }
}

}
