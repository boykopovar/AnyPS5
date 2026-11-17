/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/platform/network/ParsedContentRange.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/ParsedContentRangeExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::ParsedContentRange;
using NKWebKit::WtfAbi::String;

constexpr std::size_t notFound = static_cast<std::size_t>(-1);

ParsedContentRange& Checked(ParsedContentRange* self) {
    if (!self) throw std::invalid_argument("null WebCore::ParsedContentRange");
    return *self;
}

const ParsedContentRange& Checked(const ParsedContentRange* self) {
    if (!self) throw std::invalid_argument("null WebCore::ParsedContentRange");
    return *self;
}

const String& Checked(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

class Text {
public:
    explicit Text(const String& string) {
        const auto* impl = string.impl;
        if (!impl) return;
        m_length = impl->length;
        m_is8Bit = impl->hashAndFlags & 4;
        m_data = impl->data;
        if (m_length && !m_data) throw std::invalid_argument("null WTF::StringImpl characters");
    }

    std::size_t size() const { return m_length; }

    char16_t at(std::size_t index) const {
        if (m_is8Bit) return static_cast<const unsigned char*>(m_data)[index];
        return static_cast<const char16_t*>(m_data)[index];
    }

    std::size_t find(char16_t value, std::size_t start) const {
        for (std::size_t i = start; i < m_length; ++i) {
            if (at(i) == value) return i;
        }
        return notFound;
    }

private:
    const void* m_data{nullptr};
    std::size_t m_length{0};
    bool m_is8Bit{false};
};

bool AreContentRangeValuesValid(std::int64_t firstBytePosition, std::int64_t lastBytePosition, std::int64_t instanceLength) {
    if (firstBytePosition < 0) return false;
    if (lastBytePosition < firstBytePosition) return false;
    if (instanceLength == ParsedContentRange::unknownLength) return true;
    return lastBytePosition < instanceLength;
}

bool ParseDigits(const Text& text, std::size_t begin, std::size_t end, std::int64_t& value) {
    if (begin >= end) return false;
    std::int64_t result = 0;
    for (std::size_t i = begin; i < end; ++i) {
        const char16_t unit = text.at(i);
        if (unit < u'0' || unit > u'9') return false;
        const int digit = unit - u'0';
        if (result > (ParsedContentRange::unknownLength - digit) / 10) return false;
        result = result * 10 + digit;
    }
    value = result;
    return true;
}

bool ParseContentRange(const Text& text, ParsedContentRange& range) {
    static constexpr char16_t prefix[] = u"bytes ";
    constexpr std::size_t prefixLength = 6;

    if (text.size() < prefixLength) return false;
    for (std::size_t i = 0; i < prefixLength; ++i) {
        if (text.at(i) != prefix[i]) return false;
    }

    const std::size_t byteSeparator = text.find(u'-', prefixLength);
    if (byteSeparator == notFound) return false;

    const std::size_t instanceLengthSeparator = text.find(u'/', byteSeparator + 1);
    if (instanceLengthSeparator == notFound) return false;

    if (!ParseDigits(text, prefixLength, byteSeparator, range.firstBytePosition)) return false;
    if (!ParseDigits(text, byteSeparator + 1, instanceLengthSeparator, range.lastBytePosition)) return false;

    const std::size_t instanceBegin = instanceLengthSeparator + 1;
    if (text.size() - instanceBegin == 1 && text.at(instanceBegin) == u'*') {
        range.instanceLength = ParsedContentRange::unknownLength;
    } else if (!ParseDigits(text, instanceBegin, text.size(), range.instanceLength)) {
        return false;
    }

    return AreContentRangeValuesValid(range.firstBytePosition, range.lastBytePosition, range.instanceLength);
}

void Construct(ParsedContentRange* self, const String* headerValue) {
    ParsedContentRange& range = Checked(self);
    const Text text(Checked(headerValue));
    range.firstBytePosition = 0;
    range.lastBytePosition = 0;
    range.instanceLength = ParsedContentRange::invalidLength;
    if (!ParseContentRange(text, range)) range.instanceLength = ParsedContentRange::invalidLength;
}

void Construct(ParsedContentRange* self, std::int64_t firstBytePosition, std::int64_t lastBytePosition, std::int64_t instanceLength) {
    ParsedContentRange& range = Checked(self);
    range.firstBytePosition = firstBytePosition;
    range.lastBytePosition = lastBytePosition;
    range.instanceLength = instanceLength;
    if (!AreContentRangeValuesValid(firstBytePosition, lastBytePosition, instanceLength)) range.instanceLength = ParsedContentRange::invalidLength;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore18ParsedContentRangeC1ERKN3WTF6StringE(ParsedContentRange* self, const String* headerValue) {
    Construct(self, headerValue);
}

void APS5_VABI _ZN7WebCore18ParsedContentRangeC1Elll(ParsedContentRange* self, std::int64_t firstBytePosition, std::int64_t lastBytePosition, std::int64_t instanceLength) {
    Construct(self, firstBytePosition, lastBytePosition, instanceLength);
}

void APS5_VABI _ZN7WebCore18ParsedContentRangeC2ERKN3WTF6StringE(ParsedContentRange* self, const String* headerValue) {
    Construct(self, headerValue);
}

void APS5_VABI _ZN7WebCore18ParsedContentRangeC2Elll(ParsedContentRange* self, std::int64_t firstBytePosition, std::int64_t lastBytePosition, std::int64_t instanceLength) {
    Construct(self, firstBytePosition, lastBytePosition, instanceLength);
}

String APS5_VABI _ZNK7WebCore18ParsedContentRange11headerValueEv(const ParsedContentRange* self) {
    const ParsedContentRange& range = Checked(self);
    if (range.instanceLength == ParsedContentRange::invalidLength) return String();
    std::string text = "bytes " + std::to_string(range.firstBytePosition) + '-' + std::to_string(range.lastBytePosition) + '/';
    if (range.instanceLength == ParsedContentRange::unknownLength) text += '*';
    else text += std::to_string(range.instanceLength);
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

}
