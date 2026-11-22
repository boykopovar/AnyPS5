// Derived from Source/WebCore/platform/graphics/HEVCUtilities.cpp@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WTF/wtf/text/StringView.h@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WTF/wtf/text/StringToIntegerConversion.h@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace NKWebKit::WtfAbi;
using WebCore::DoViOptionalParameterSet;
using WebCore::DoViParameterSet;

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
            if (At(i) != static_cast<unsigned char>(text[i]))
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

bool IsSpaceOrNewline(char16_t c) {
    if (c <= 0xFF)
        return c == ' ' || (c >= '\t' && c <= '\r');
    return c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x205F || c == 0x3000;
}

std::optional<uint8_t> ToIntegralType(View view) {
    unsigned position = 0;
    while (position < view.length && IsSpaceOrNewline(view.At(position)))
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
    while (position < view.length && IsSpaceOrNewline(view.At(position)))
        ++position;
    if (position != view.length)
        return std::nullopt;
    return static_cast<uint8_t>(value);
}

std::string_view codecStringForDoViCodecType(View codec)
{
    if (codec.Equals("dvhe")) return "hev1";
    if (codec.Equals("dvh1")) return "hvc1";
    if (codec.Equals("dvav")) return "avc3";
    if (codec.Equals("dva1")) return "avc1";
    return {};
}

std::optional<unsigned short> profileIDForAlphabeticDoViProfile(View profile)
{
    if (profile.Equals("dvhe.dtr")) return 4;
    if (profile.Equals("dvhe.stn")) return 5;
    if (profile.Equals("dvhe.dtb")) return 7;
    if (profile.Equals("dvhe.st")) return 8;
    if (profile.Equals("dvav.se")) return 9;
    return std::nullopt;
}

static bool isValidDoViProfileID(unsigned short profileID)
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

static std::optional<unsigned short> maximumLevelIDForDoViProfileID(unsigned short profileID)
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

static bool isValidProfileIDForCodecName(unsigned short profileID, std::string_view codecName)
{
    if (profileID == 9)
        return codecName == "avc1" || codecName == "avc3";
    return codecName == "hvc1" || codecName == "hev1";
}

DoViOptionalParameterSet parseDoViCodecParameters(View codecView)
{
    auto codecSplit = codecView.Split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return {};

    const std::string_view codecName = codecStringForDoViCodecType(*nextElement);
    if (codecName.empty())
        return {};

    if (++nextElement == codecSplit.end())
        return {};

    auto profileID = *nextElement;
    if (!profileID.length)
        return {};

    unsigned short bitstreamProfileID = 0;
    auto firstCharacter = profileID.At(0);
    if (firstCharacter == '0') {
        auto numericProfileID = ToIntegralType(profileID);
        if (!numericProfileID)
            return {};
        bitstreamProfileID = *numericProfileID;
    } else {
        auto alphabeticProfileID = profileIDForAlphabeticDoViProfile(codecView.Left(5 + profileID.length));
        if (!alphabeticProfileID)
            return {};
        bitstreamProfileID = *alphabeticProfileID;
    }

    if (!isValidDoViProfileID(bitstreamProfileID))
        return {};

    if (!isValidProfileIDForCodecName(bitstreamProfileID, codecName))
        return {};

    if (++nextElement == codecSplit.end())
        return {};

    auto levelID = *nextElement;
    if (!levelID.length)
        return {};

    auto bitstreamLevelID = ToIntegralType(levelID);
    if (!bitstreamLevelID)
        return {};

    auto maximumLevelID = maximumLevelIDForDoViProfileID(bitstreamProfileID);
    if (!maximumLevelID || *bitstreamLevelID > *maximumLevelID)
        return {};

    DoViParameterSet parameters {
        _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(codecName.data()), static_cast<unsigned>(codecName.size())),
        bitstreamProfileID,
        *bitstreamLevelID,
    };
    return DoViOptionalParameterSet(std::move(parameters));
}

}  // namespace

WebCore::DoViOptionalParameterSet APS5_VABI _ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(const NKWebKit::WtfAbi::String& input) {
    if (!input.impl)
        return {};
    if (input.impl->length && !input.impl->data)
        throw std::invalid_argument("DoVi codec string has null storage");
    return parseDoViCodecParameters({{input.impl->data, input.impl->length, !!(input.impl->hashAndFlags & 4)}, 0, input.impl->length});
}
