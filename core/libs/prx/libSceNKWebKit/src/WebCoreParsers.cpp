/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/html/parser/HTMLParserIdioms.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/HTTPParsers.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/text/LineEnding.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreParsersExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace NKWebKit::WtfAbi {

ByteVector::ByteVector(ByteVector&& other) noexcept
    : buffer(std::exchange(other.buffer, nullptr)), capacity(std::exchange(other.capacity, 0)), size(std::exchange(other.size, 0)) {}

ByteVector::~ByteVector() {
    if (buffer) _ZN3WTF8fastFreeEPv(buffer);
}

}  // namespace NKWebKit::WtfAbi

namespace {

using NKWebKit::WtfAbi::ByteVector;
using NKWebKit::WtfAbi::StringView;
using WebCore::HTMLIntegerParsingError;

template<typename CharacterType> bool IsASCIIWhitespace(CharacterType character) {
    return character == ' ' || character == '\n' || character == '\t' || character == '\r' || character == '\f';
}

template<typename CharacterType> bool IsASCIIDigit(CharacterType character) {
    return character >= '0' && character <= '9';
}

bool IsHTTPSpace(char16_t character) {
    return character <= ' ' && (character == ' ' || character == '\n' || character == '\t' || character == '\r');
}

struct IntegerResult {
    bool hasValue;
    int value;
    HTMLIntegerParsingError error;
};

IntegerResult Failure(HTMLIntegerParsingError error) {
    return IntegerResult{false, 0, error};
}

template<typename CharacterType> IntegerResult ParseInteger(const CharacterType* position, const CharacterType* end) {
    while (position < end && IsASCIIWhitespace(*position)) ++position;

    if (position == end) return Failure(HTMLIntegerParsingError::Other);

    bool isNegative = false;
    if (*position == '-') {
        isNegative = true;
        ++position;
    } else if (*position == '+') {
        ++position;
    }

    if (position == end || !IsASCIIDigit(*position)) return Failure(HTMLIntegerParsingError::Other);

    constexpr int intMax = std::numeric_limits<int>::max();
    constexpr int base = 10;
    constexpr int maxMultiplier = intMax / base;

    unsigned result = 0;
    do {
        int digitValue = *position - '0';

        if (result > static_cast<unsigned>(maxMultiplier) || (result == static_cast<unsigned>(maxMultiplier) && digitValue > (intMax % base) + static_cast<int>(isNegative)))
            return Failure(isNegative ? HTMLIntegerParsingError::NegativeOverflow : HTMLIntegerParsingError::PositiveOverflow);

        result = base * result + digitValue;
        ++position;
    } while (position < end && IsASCIIDigit(*position));

    unsigned signedResult = isNegative ? 0u - result : result;
    return IntegerResult{true, static_cast<int>(signedResult), HTMLIntegerParsingError::Other};
}

IntegerResult Parse(StringView input) {
    if (!input.length) return Failure(HTMLIntegerParsingError::Other);
    if (!input.data) throw std::invalid_argument("null WTF::StringView data");
    if (input.is8Bit) {
        auto* start = static_cast<const unsigned char*>(input.data);
        return ParseInteger(start, start + input.length);
    }
    auto* start = static_cast<const char16_t*>(input.data);
    return ParseInteger(start, start + input.length);
}

bool Equals(StringView view, const char* literal) {
    std::size_t length = std::strlen(literal);
    if (view.length != length) return false;
    if (!length) return true;
    if (view.is8Bit) return std::memcmp(view.data, literal, length) == 0;
    auto* characters = static_cast<const char16_t*>(view.data);
    for (std::size_t i = 0; i < length; ++i)
        if (characters[i] != static_cast<unsigned char>(literal[i])) return false;
    return true;
}

StringView Trim(StringView view) {
    if (!view.length) return view;
    if (!view.data) throw std::invalid_argument("null WTF::StringView data");
    unsigned start = 0;
    unsigned end = view.length;
    auto at = [&](unsigned index) -> char16_t {
        return view.is8Bit ? static_cast<const unsigned char*>(view.data)[index] : static_cast<const char16_t*>(view.data)[index];
    };
    while (start < end && IsHTTPSpace(at(start))) ++start;
    while (end > start && IsHTTPSpace(at(end - 1))) --end;
    const char* base = static_cast<const char*>(view.data);
    return StringView{base + static_cast<std::size_t>(start) * (view.is8Bit ? 1 : 2), end - start, view.is8Bit};
}

ByteVector& Checked(ByteVector* vector) {
    if (!vector) throw std::invalid_argument("null WTF::Vector");
    return *vector;
}

}  // namespace

extern "C" {

WebCore::ExpectedInt APS5_VABI _ZN7WebCore16parseHTMLIntegerEN3WTF10StringViewE(StringView input) {
    auto result = Parse(input);
    WebCore::ExpectedInt expected{};
    expected.hasValue = result.hasValue;
    if (result.hasValue) expected.value = result.value;
    else expected.error = result.error;
    return expected;
}

WebCore::CrossOriginResourcePolicy APS5_VABI _ZN7WebCore36parseCrossOriginResourcePolicyHeaderEN3WTF10StringViewE(StringView header) {
    auto trimmed = Trim(header);
    if (!trimmed.length) return WebCore::CrossOriginResourcePolicy::None;
    if (Equals(trimmed, "same-origin")) return WebCore::CrossOriginResourcePolicy::SameOrigin;
    if (Equals(trimmed, "same-site")) return WebCore::CrossOriginResourcePolicy::SameSite;
    if (Equals(trimmed, "cross-origin")) return WebCore::CrossOriginResourcePolicy::CrossOrigin;
    return WebCore::CrossOriginResourcePolicy::Invalid;
}

ByteVector APS5_VABI _ZN3WTF24normalizeLineEndingsToLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(ByteVector&& vector) {
    auto& source = Checked(&vector);
    unsigned char* q = source.buffer;
    for (unsigned char *p = source.buffer, *end = p + source.size; p != end;) {
        unsigned char character = *p++;
        if (character == '\r') {
            if (p != end && *p == '\n') ++p;
            *q++ = '\n';
        } else {
            *q++ = character;
        }
    }
    if (source.buffer) source.size = static_cast<unsigned>(q - source.buffer);
    return ByteVector(std::move(source));
}

ByteVector APS5_VABI _ZN3WTF26normalizeLineEndingsToCRLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(ByteVector&& source) {
    auto& input = Checked(&source);
    std::size_t resultLength = 0;
    for (const unsigned char *p = input.buffer, *end = p + input.size; p != end;) {
        unsigned char character = *p++;
        if (character == '\r') {
            if (p != end && *p == '\n') ++p;
            resultLength += 2;
        } else if (character == '\n') {
            resultLength += 2;
        } else {
            resultLength += 1;
        }
    }

    if (resultLength == input.size) return ByteVector(std::move(input));

    if (resultLength > std::numeric_limits<unsigned>::max()) throw std::length_error("WTF::Vector size overflow");
    ByteVector result;
    result.buffer = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(resultLength));
    if (!result.buffer) throw std::bad_alloc();
    result.capacity = static_cast<unsigned>(resultLength);
    result.size = static_cast<unsigned>(resultLength);
    std::memset(result.buffer, 0, resultLength);
    unsigned char* q = result.buffer;
    for (const unsigned char *p = input.buffer, *end = p + input.size; p != end;) {
        unsigned char character = *p++;
        if (character == '\r') {
            if (p != end && *p == '\n') ++p;
            *q++ = '\r';
            *q++ = '\n';
        } else if (character == '\n') {
            *q++ = '\r';
            *q++ = '\n';
        } else {
            *q++ = character;
        }
    }
    return result;
}

ByteVector APS5_VABI _ZN3WTF28normalizeLineEndingsToNativeEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(ByteVector&& from) {
    return _ZN3WTF24normalizeLineEndingsToLFEONS_6VectorIhLm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(std::move(from));
}

}
