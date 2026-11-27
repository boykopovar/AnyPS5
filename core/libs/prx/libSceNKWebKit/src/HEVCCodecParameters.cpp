// Derived from Source/WebCore/platform/graphics/HEVCUtilities.cpp@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WTF/wtf/text/StringToIntegerConversion.h@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WTF/wtf/text/StringView.h@c55364d53f971f63a709976f168d932d558b8909
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

#include "prx/libSceNKWebKit/include/HEVCCodecParameters.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace NKWebKit::WtfAbi;
using WebCore::HEVCOptionalParameterSet;
using WebCore::HEVCParameterSet;

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
    View Substring(unsigned position) const {
        if (position >= length)
            return {input, start + length, 0};
        return {input, start + position, length - position};
    }
    String ToString() const {
        if (input.is8Bit)
            return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(input.data) + start, length);
        return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(input.data) + start, length);
    }
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

bool IsCharacterAllowedInBase(char16_t c, unsigned base) {
    if (c > 0x7F)
        return false;
    if (c >= '0' && c <= '9')
        return static_cast<unsigned>(c - '0') < base;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return (c >= 'a' && c < 'a' + base - 10) || (c >= 'A' && c < 'A' + base - 10);
    return false;
}

template<typename T> std::optional<T> ToIntegralType(View view, unsigned base = 10) {
    constexpr T integralMax = std::numeric_limits<T>::max();
    const T maxMultiplier = static_cast<T>(integralMax / base);
    unsigned position = 0;
    while (position < view.length && IsSpaceOrNewline(view.At(position)))
        ++position;
    if (position < view.length && view.At(position) == '+')
        ++position;
    if (position == view.length || !IsCharacterAllowedInBase(view.At(position), base))
        return std::nullopt;
    T value = 0;
    while (position < view.length && IsCharacterAllowedInBase(view.At(position), base)) {
        const char16_t c = view.At(position);
        T digitValue;
        if (c >= '0' && c <= '9')
            digitValue = static_cast<T>(c - '0');
        else if (c >= 'a')
            digitValue = static_cast<T>(c - 'a' + 10);
        else
            digitValue = static_cast<T>(c - 'A' + 10);
        if (value > maxMultiplier || (value == maxMultiplier && digitValue > integralMax % base))
            return std::nullopt;
        value = static_cast<T>(base * value + digitValue);
        ++position;
    }
    while (position < view.length && IsSpaceOrNewline(view.At(position)))
        ++position;
    if (position != view.length)
        return std::nullopt;
    return value;
}

HEVCOptionalParameterSet parseHEVCCodecParameters(View codecView)
{
    auto codecSplit = codecView.Split('.');
    auto nextElement = codecSplit.begin();
    if (nextElement == codecSplit.end())
        return {};

    const View codecName = *nextElement;
    if (!codecName.Equals("hvc1") && !codecName.Equals("hev1"))
        return {};

    if (++nextElement == codecSplit.end())
        return {};

    auto profileSpace = *nextElement;
    if (!profileSpace.length)
        return {};

    unsigned short generalProfileSpace = 0;
    auto firstCharacter = profileSpace.At(0);
    bool hasProfileSpace = firstCharacter >= 'A' && firstCharacter <= 'C';
    if (hasProfileSpace) {
        generalProfileSpace = static_cast<unsigned short>(1 + (firstCharacter - 'A'));
        profileSpace = profileSpace.Substring(1);
    }

    auto generalProfileIDC = ToIntegralType<uint8_t>(profileSpace);
    if (!generalProfileIDC)
        return {};

    if (++nextElement == codecSplit.end())
        return {};

    auto generalProfileCompatibilityFlags = ToIntegralType<uint32_t>(*nextElement, 16);
    if (!generalProfileCompatibilityFlags)
        return {};

    if (++nextElement == codecSplit.end())
        return {};

    auto generalTier = *nextElement;
    firstCharacter = generalTier.At(0);
    if (firstCharacter != 'L' && firstCharacter != 'H')
        return {};

    auto generalLevelIDC = ToIntegralType<uint8_t>(generalTier.Substring(1));
    if (!generalLevelIDC)
        return {};

    unsigned short constraintFlags[6] {};
    for (auto& flag : constraintFlags) {
        if (++nextElement == codecSplit.end())
            break;

        auto parsedFlag = ToIntegralType<uint8_t>(*nextElement, 16);
        if (!parsedFlag)
            return {};
        flag = *parsedFlag;
    }

    Vector<unsigned short> flags(6);
    for (unsigned index = 0; index < 6; ++index)
        flags.buffer[index] = constraintFlags[index];
    flags.size = 6;
    HEVCParameterSet parameters {
        codecName.ToString(),
        generalProfileSpace,
        *generalProfileIDC,
        *generalProfileCompatibilityFlags,
        firstCharacter == 'H',
        *generalLevelIDC,
        std::move(flags),
    };
    return HEVCOptionalParameterSet(std::move(parameters));
}

}

WebCore::HEVCOptionalParameterSet APS5_VABI _ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(const NKWebKit::WtfAbi::String& string) {
    if (!string.impl)
        return {};
    if (string.impl->length && !string.impl->data)
        throw std::invalid_argument("null HEVC string storage");
    return parseHEVCCodecParameters({{string.impl->data, string.impl->length, !!(string.impl->hashAndFlags & 4)}, 0, string.impl->length});
}
