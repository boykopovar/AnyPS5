/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/DoViCodecParameters.hpp"
#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace NKWebKit::WtfAbi;
using WebCore::DoViParameters;

struct View {
    StringView input;
    unsigned start;
    unsigned length;
    char16_t At(unsigned index) const {
        return input.is8Bit ? static_cast<const unsigned char*>(input.data)[start + index] : static_cast<const char16_t*>(input.data)[start + index];
    }
    bool Equals(std::string_view text) const {
        if (length != text.size())
            return false;
        for (unsigned i = 0; i < length; ++i) {
            auto c = At(i);
            if (c >= 0x80 || (c | 0x20) != static_cast<unsigned char>(text[i]))
                return false;
        }
        return true;
    }
    View Left(unsigned count) const { return {input, start, std::min(count, length)}; }
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

std::optional<DoViParameters::Codec> ParseDoViCodecType(View string) {
    using Codec = DoViParameters::Codec;
    if (string.Equals("dva1")) return Codec::AVC1;
    if (string.Equals("dvav")) return Codec::AVC3;
    if (string.Equals("dvh1")) return Codec::HVC1;
    if (string.Equals("dvhe")) return Codec::HEV1;
    return std::nullopt;
}

std::optional<uint16_t> ProfileIDForAlphabeticDoViProfile(View profile) {
    if (profile.Equals("dvav.se")) return 9;
    if (profile.Equals("dvhe.dtb")) return 7;
    if (profile.Equals("dvhe.dtr")) return 4;
    if (profile.Equals("dvhe.st")) return 8;
    if (profile.Equals("dvhe.stn")) return 5;
    return std::nullopt;
}

static bool isValidDoViProfileID(uint16_t profileID)
{
    switch (profileID) {
    case 4:
    case 5:
    case 7:
    case 8:
    case 9:
        return true;
    default:
        return false;
    }
}

static std::optional<uint16_t> maximumLevelIDForDoViProfileID(uint16_t profileID)
{
    switch (profileID) {
    case 4: return 9;
    case 5: return 13;
    case 7: return 9;
    case 8: return 13;
    case 9: return 5;
    default: return std::nullopt;
    }
}

static bool isValidProfileIDForCodec(uint16_t profileID, DoViParameters::Codec codec)
{
    if (profileID == 9)
        return codec == DoViParameters::Codec::AVC1 || codec == DoViParameters::Codec::AVC3;
    return codec == DoViParameters::Codec::HVC1 || codec == DoViParameters::Codec::HEV1;
}

std::optional<DoViParameters> parseDoViCodecParameters(View codecView)
{
    auto codecSplit = codecView.Split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return std::nullopt;

    DoViParameters parameters;

    auto codec = ParseDoViCodecType(*nextElement);
    if (!codec)
        return std::nullopt;
    parameters.codec = *codec;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto profileID = *nextElement;
    if (!profileID.length)
        return std::nullopt;

    auto firstCharacter = profileID.At(0);
    if (firstCharacter == '0') {
        auto bitstreamProfileID = ParseInteger(profileID);
        if (!bitstreamProfileID)
            return std::nullopt;
        parameters.bitstreamProfileID = *bitstreamProfileID;
    } else {
        auto bitstreamProfileID = ProfileIDForAlphabeticDoViProfile(codecView.Left(5 + profileID.length));
        if (!bitstreamProfileID)
            return std::nullopt;
        parameters.bitstreamProfileID = *bitstreamProfileID;
    }

    if (!isValidDoViProfileID(parameters.bitstreamProfileID))
        return std::nullopt;

    if (!isValidProfileIDForCodec(parameters.bitstreamProfileID, parameters.codec))
        return std::nullopt;

    if (++nextElement == codecSplit.end())
        return std::nullopt;

    auto bitstreamLevelID = ParseInteger(*nextElement);
    if (!bitstreamLevelID)
        return std::nullopt;
    parameters.bitstreamLevelID = *bitstreamLevelID;

    auto maximumLevelID = maximumLevelIDForDoViProfileID(parameters.bitstreamProfileID);
    if (!maximumLevelID || parameters.bitstreamLevelID > *maximumLevelID)
        return std::nullopt;

    return parameters;
}

}  // namespace

std::optional<WebCore::DoViParameters> APS5_VABI _ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(const NKWebKit::WtfAbi::String& input) {
    if (!input.impl)
        return std::nullopt;
    if (input.impl->length && !input.impl->data)
        throw std::invalid_argument("DoVi codec string has null storage");
    return parseDoViCodecParameters({{input.impl->data, input.impl->length, !!(input.impl->hashAndFlags & 4)}, 0, input.impl->length});
}
