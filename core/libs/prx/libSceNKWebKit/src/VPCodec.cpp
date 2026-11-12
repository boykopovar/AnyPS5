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

Copyright (C) 2014-2019 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/VPCodecExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace {
using namespace NKWebKit::WtfAbi;
using namespace WebCore;

struct View {
    StringView input;
    unsigned start;
    unsigned length;
    char16_t At(unsigned index) const {
        return input.is8Bit ? static_cast<const unsigned char*>(input.data)[start + index] : static_cast<const char16_t*>(input.data)[start + index];
    }
    String ToString() const {
        if (input.is8Bit)
            return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(input.data) + start, length);
        return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(input.data) + start, length);
    }
    String ToStringWithoutCopying() const { return ToString(); }
    std::vector<View> Split(char16_t separator) const {
        std::vector<View> parts;
        unsigned position = 0;
        while (position < length) {
            unsigned end = position;
            while (end < length && At(end) != separator)
                ++end;
            if (end > position)
                parts.push_back({input, start + position, end - position});
            position = end < length ? end + 1 : end;
        }
        return parts;
    }
};

bool operator==(const String& string, const char* text) {
    if (!string.impl)
        return !*text;
    unsigned length = 0;
    while (text[length])
        ++length;
    if (length != string.impl->length)
        return false;
    for (unsigned i = 0; i < length; ++i) {
        auto c = string.impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(string.impl->data)[i] : static_cast<const char16_t*>(string.impl->data)[i];
        if (c != text[i])
            return false;
    }
    return true;
}

bool IsWhitespace(char16_t c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

std::optional<uint8_t> ParseInteger(View view) {
    unsigned position = 0;
    while (position < view.length && IsWhitespace(view.At(position)))
        ++position;
    if (position < view.length && view.At(position) == '+')
        ++position;
    if (position == view.length || view.At(position) < '0' || view.At(position) > '9')
        return std::nullopt;
    unsigned value = 0;
    do {
        value = value * 10 + view.At(position++) - '0';
        if (value > 255)
            return std::nullopt;
    } while (position < view.length && view.At(position) >= '0' && view.At(position) <= '9');
    while (position < view.length && IsWhitespace(view.At(position)))
        ++position;
    if (position != view.length)
        return std::nullopt;
    return static_cast<uint8_t>(value);
}

static bool IsValidVPProfile(uint8_t profile)
{
    return profile <= 3;
}

static bool IsValidVPLevel(uint8_t level)
{
    constexpr uint8_t validLevels[] = {
        VPConfigurationLevel::Level_1,
        VPConfigurationLevel::Level_1_1,
        VPConfigurationLevel::Level_2,
        VPConfigurationLevel::Level_2_1,
        VPConfigurationLevel::Level_3,
        VPConfigurationLevel::Level_3_1,
        VPConfigurationLevel::Level_4,
        VPConfigurationLevel::Level_4_1,
        VPConfigurationLevel::Level_5,
        VPConfigurationLevel::Level_5_1,
        VPConfigurationLevel::Level_5_2,
        VPConfigurationLevel::Level_6,
        VPConfigurationLevel::Level_6_1,
        VPConfigurationLevel::Level_6_2,
    };

    return std::binary_search(std::begin(validLevels), std::end(validLevels), level);
}

static bool IsValidBitDepth(uint8_t bitDepth)
{
    return bitDepth == 8
        || bitDepth == 10
        || bitDepth == 12;
}

static bool IsValidRange(uint8_t range)
{
    return range == VPConfigurationRange::VideoRange
        || range == VPConfigurationRange::FullRange;
}

static bool IsValidChromaSubsampling(uint8_t subsampling)
{
    return subsampling >= VPConfigurationChromaSubsampling::Subsampling_420_Vertical
        && subsampling <= VPConfigurationChromaSubsampling::Subsampling_444;
}

static bool IsValidVPColorPrimaries(uint8_t colorPrimaries)
{
    constexpr uint8_t validColorPrimaries[] = {
        VPConfigurationColorPrimaries::BT_709_6,
        VPConfigurationColorPrimaries::Unspecified,
        VPConfigurationColorPrimaries::BT_470_6_M,
        VPConfigurationColorPrimaries::BT_470_7_BG,
        VPConfigurationColorPrimaries::BT_601_7,
        VPConfigurationColorPrimaries::SMPTE_ST_240,
        VPConfigurationColorPrimaries::Film,
        VPConfigurationColorPrimaries::BT_2020_Nonconstant_Luminance,
        VPConfigurationColorPrimaries::SMPTE_ST_428_1,
        VPConfigurationColorPrimaries::SMPTE_RP_431_2,
        VPConfigurationColorPrimaries::SMPTE_EG_432_1,
        VPConfigurationColorPrimaries::EBU_Tech_3213_E,
    };

    return std::binary_search(std::begin(validColorPrimaries), std::end(validColorPrimaries), colorPrimaries);
}

static bool IsValidVPTransferCharacteristics(uint8_t transferCharacteristics)
{
    constexpr uint8_t validTransferCharacteristics[] = {
        VPConfigurationTransferCharacteristics::BT_709_6,
        VPConfigurationTransferCharacteristics::Unspecified,
        VPConfigurationTransferCharacteristics::BT_470_6_M,
        VPConfigurationTransferCharacteristics::BT_470_7_BG,
        VPConfigurationTransferCharacteristics::BT_601_7,
        VPConfigurationTransferCharacteristics::SMPTE_ST_240,
        VPConfigurationTransferCharacteristics::Linear,
        VPConfigurationTransferCharacteristics::Logrithmic,
        VPConfigurationTransferCharacteristics::Logrithmic_Sqrt,
        VPConfigurationTransferCharacteristics::IEC_61966_2_4,
        VPConfigurationTransferCharacteristics::BT_1361_0,
        VPConfigurationTransferCharacteristics::IEC_61966_2_1,
        VPConfigurationTransferCharacteristics::BT_2020_10bit,
        VPConfigurationTransferCharacteristics::BT_2020_12bit,
        VPConfigurationTransferCharacteristics::SMPTE_ST_2084,
        VPConfigurationTransferCharacteristics::SMPTE_ST_428_1,
        VPConfigurationTransferCharacteristics::BT_2100_HLG,
    };
    return std::binary_search(std::begin(validTransferCharacteristics), std::end(validTransferCharacteristics), transferCharacteristics);
}

static bool IsValidVPMatrixCoefficients(uint8_t matrixCoefficients)
{
    constexpr uint8_t validMatrixCoefficients[] = {
        VPConfigurationMatrixCoefficients::Identity,
        VPConfigurationMatrixCoefficients::BT_709_6,
        VPConfigurationMatrixCoefficients::Unspecified,
        VPConfigurationMatrixCoefficients::FCC,
        VPConfigurationMatrixCoefficients::BT_470_7_BG,
        VPConfigurationMatrixCoefficients::BT_601_7,
        VPConfigurationMatrixCoefficients::SMPTE_ST_240,
        VPConfigurationMatrixCoefficients::YCgCo,
        VPConfigurationMatrixCoefficients::BT_2020_Nonconstant_Luminance,
        VPConfigurationMatrixCoefficients::BT_2020_Constant_Luminance,
        VPConfigurationMatrixCoefficients::SMPTE_ST_2085,
        VPConfigurationMatrixCoefficients::Chromacity_Constant_Luminance,
        VPConfigurationMatrixCoefficients::Chromacity_Nonconstant_Luminance,
        VPConfigurationMatrixCoefficients::BT_2100_ICC,
    };
    return std::binary_search(std::begin(validMatrixCoefficients), std::end(validMatrixCoefficients), matrixCoefficients);
}

std::optional<VPCodecConfigurationRecord> ParseVPCodecParameters(View codecView)
{
    auto codecSplit = codecView.Split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return std::nullopt;

    VPCodecConfigurationRecord configuration;

    configuration.codecName = (*nextElement).ToString();
    ++nextElement;

    if (configuration.codecName == "vp8" || configuration.codecName == "vp9") {
        if (nextElement == codecSplit.end())
            return configuration;
        
        auto codecString = codecView.ToStringWithoutCopying();
        if (codecString == "vp8.0" || codecString == "vp9.0")
            return configuration;
    }



    if (configuration.codecName != "vp08" && configuration.codecName != "vp09")
        return std::nullopt;

    if (nextElement == codecSplit.end())
        return std::nullopt;

    auto profile = ParseInteger(*nextElement);
    if (!profile || !IsValidVPProfile(*profile))
        return std::nullopt;
    configuration.profile = *profile;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto level = ParseInteger(*nextElement);
    if (!level || !IsValidVPLevel(*level))
        return std::nullopt;
    configuration.level = *level;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto bitDepth = ParseInteger(*nextElement);
    if (!bitDepth || !IsValidBitDepth(*bitDepth))
        return std::nullopt;
    configuration.bitDepth = *bitDepth;


    if (++nextElement == codecSplit.end())
        return configuration;

    auto chromaSubsampling = ParseInteger(*nextElement);
    if (!chromaSubsampling || !IsValidChromaSubsampling(*chromaSubsampling))
        return std::nullopt;
    configuration.chromaSubsampling = *chromaSubsampling;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto colorPrimaries = ParseInteger(*nextElement);
    if (!colorPrimaries || !IsValidVPColorPrimaries(*colorPrimaries))
        return std::nullopt;
    configuration.colorPrimaries = *colorPrimaries;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto transferCharacteristics = ParseInteger(*nextElement);
    if (!transferCharacteristics || !IsValidVPTransferCharacteristics(*transferCharacteristics))
        return std::nullopt;
    configuration.transferCharacteristics = *transferCharacteristics;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto matrixCoefficients = ParseInteger(*nextElement);
    if (!matrixCoefficients || !IsValidVPMatrixCoefficients(*matrixCoefficients))
        return std::nullopt;
    configuration.matrixCoefficients = *matrixCoefficients;

    if (!configuration.matrixCoefficients && configuration.chromaSubsampling != 3)
        return std::nullopt;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto videoFullRangeFlag = ParseInteger(*nextElement);
    if (!videoFullRangeFlag || !IsValidRange(*videoFullRangeFlag))
        return std::nullopt;
    configuration.videoFullRangeFlag = *videoFullRangeFlag;

    if (++nextElement != codecSplit.end())
        return std::nullopt;

    return configuration;
}

}  // namespace

std::optional<WebCore::VPCodecConfigurationRecord> APS5_VABI _ZN7WebCore22parseVPCodecParametersEN3WTF10StringViewE(NKWebKit::WtfAbi::StringView input) {
    if (!input.data && input.length)
        throw std::invalid_argument("null VP codec StringView storage");
    return ParseVPCodecParameters({input, 0, input.length});
}
