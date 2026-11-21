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

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/html/parser/HTMLParserIdioms.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/network/HTTPParsers.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/text/LineEnding.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from WebKit/WebKit/Source/WebCore/platform/network/HTTPParsers.cpp@ccda9bfb069d312ebdfbeb37a2151e0536968a1d
// Derived from WebKit/WebKit/Source/WTF/wtf/text/StringToIntegerConversion.h@ccda9bfb069d312ebdfbeb37a2151e0536968a1d

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreParsersExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
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

using WebCore::XFrameOptionsDisposition;

bool IsFrameOptionsWhitespace(char16_t character) {
    return character == ' ' || (character >= '\t' && character <= '\r')
        || character == 0x1680 || (character >= 0x2000 && character <= 0x200a)
        || character == 0x2028 || character == 0x205f || character == 0x3000;
}

template<typename Char> bool MatchesFrameOption(std::basic_string_view<Char> value, std::string_view token) {
    if (value.size() != token.size()) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char16_t character = value[i];
        const char16_t lower = character >= 'A' && character <= 'Z' ? character + ('a' - 'A') : character;
        if (lower != static_cast<unsigned char>(token[i])) return false;
    }
    return true;
}

template<typename Char> XFrameOptionsDisposition ParseFrameOption(std::basic_string_view<Char> value) {
    while (!value.empty() && IsFrameOptionsWhitespace(value.front())) value.remove_prefix(1);
    while (!value.empty() && IsFrameOptionsWhitespace(value.back())) value.remove_suffix(1);
    if (MatchesFrameOption(value, "deny")) return XFrameOptionsDisposition::Deny;
    if (MatchesFrameOption(value, "sameorigin")) return XFrameOptionsDisposition::SameOrigin;
    if (MatchesFrameOption(value, "allowall")) return XFrameOptionsDisposition::AllowAll;
    return XFrameOptionsDisposition::Invalid;
}

template<typename Char> XFrameOptionsDisposition ParseFrameOptions(const void* data, unsigned length) {
    std::basic_string_view<Char> remaining(static_cast<const Char*>(data), length);
    auto result = XFrameOptionsDisposition::None;
    while (!remaining.empty()) {
        const auto separator = remaining.find(',');
        const auto token = remaining.substr(0, separator);
        if (!token.empty()) {
            const auto parsed = ParseFrameOption(token);
            if (result != XFrameOptionsDisposition::None && result != parsed) return XFrameOptionsDisposition::Conflict;
            result = parsed;
        }
        if (separator == remaining.npos) break;
        remaining.remove_prefix(separator + 1);
    }
    return result;
}

ByteVector& Checked(ByteVector* vector) {
    if (!vector) throw std::invalid_argument("null WTF::Vector");
    return *vector;
}

const NKWebKit::WtfAbi::String& Checked(const NKWebKit::WtfAbi::String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

long long& Checked(long long* value) {
    if (!value) throw std::invalid_argument("null long long");
    return *value;
}

constexpr std::size_t notFound = std::numeric_limits<std::size_t>::max();

bool IsSpaceOrNewline(char16_t character) {
    if (character <= 0xFF) return character == ' ' || (character >= 0x9 && character <= 0xD);
    return character == 0x1680 || (character >= 0x2000 && character <= 0x200A) || character == 0x2028 || character == 0x205F || character == 0x3000;
}

class CodeUnits {
public:
    explicit CodeUnits(const NKWebKit::WtfAbi::String& string) {
        const auto* impl = string.impl;
        if (!impl) return;
        m_length = impl->length;
        m_is8Bit = impl->hashAndFlags & 4;
        m_data = impl->data;
        if (m_length && !m_data) throw std::invalid_argument("null WTF::StringImpl characters");
    }

    std::size_t size() const { return m_length; }

    char16_t at(std::size_t index) const {
        if (m_is8Bit) return static_cast<const unsigned char*>(m_data)[m_begin + index];
        return static_cast<const char16_t*>(m_data)[m_begin + index];
    }

    std::size_t find(char16_t value) const {
        for (std::size_t i = 0; i < m_length; ++i) {
            if (at(i) == value) return i;
        }
        return notFound;
    }

    CodeUnits slice(std::size_t begin, std::size_t end) const {
        CodeUnits result = *this;
        result.m_begin = m_begin + begin;
        result.m_length = end - begin;
        return result;
    }

    CodeUnits stripped() const {
        std::size_t begin = 0;
        std::size_t end = m_length;
        while (begin < end && IsSpaceOrNewline(at(begin))) ++begin;
        while (end > begin && IsSpaceOrNewline(at(end - 1))) --end;
        return slice(begin, end);
    }

private:
    const void* m_data{nullptr};
    std::size_t m_begin{0};
    std::size_t m_length{0};
    bool m_is8Bit{false};
};

bool ParseInt64Strict(const CodeUnits& text, long long& result) {
    constexpr std::uint64_t base = 10;
    constexpr std::uint64_t integralMax = static_cast<std::uint64_t>(std::numeric_limits<long long>::max());
    constexpr std::uint64_t maxMultiplier = integralMax / base;

    const std::size_t length = text.size();
    std::size_t position = 0;
    while (position < length && IsSpaceOrNewline(text.at(position))) ++position;

    bool isNegative = false;
    if (position < length && text.at(position) == '-') {
        isNegative = true;
        ++position;
    } else if (position < length && text.at(position) == '+') {
        ++position;
    }

    if (position == length || !IsASCIIDigit(text.at(position))) return false;

    std::uint64_t value = 0;
    while (position < length && IsASCIIDigit(text.at(position))) {
        const std::uint64_t digit = static_cast<std::uint64_t>(text.at(position) - '0');
        if (value > maxMultiplier || (value == maxMultiplier && digit > integralMax % base + static_cast<std::uint64_t>(isNegative))) return false;
        value = base * value + digit;
        ++position;
    }

    while (position < length && IsSpaceOrNewline(text.at(position))) ++position;
    if (position != length) return false;

    result = static_cast<long long>(isNegative ? 0 - value : value);
    return true;
}

bool ParseRange(const CodeUnits& range, long long& rangeOffset, long long& rangeEnd, long long& rangeSuffixLength) {
    rangeOffset = rangeEnd = rangeSuffixLength = -1;

    static constexpr char prefix[] = "bytes=";
    constexpr std::size_t prefixLength = sizeof(prefix) - 1;
    if (range.size() < prefixLength) return false;
    for (std::size_t i = 0; i < prefixLength; ++i) {
        if ((range.at(i) | 0x20) != prefix[i]) return false;
    }

    const CodeUnits byteRange = range.slice(prefixLength, range.size());
    const std::size_t index = byteRange.find('-');
    if (index == notFound) return false;

    if (!index) {
        long long suffixLength;
        if (ParseInt64Strict(byteRange.slice(1, byteRange.size()).stripped(), suffixLength)) rangeSuffixLength = suffixLength;
        return true;
    }

    long long firstBytePos;
    if (!ParseInt64Strict(byteRange.slice(0, index).stripped(), firstBytePos)) return false;

    const CodeUnits lastBytePosText = byteRange.slice(index + 1, byteRange.size()).stripped();
    long long lastBytePos = -1;
    if (lastBytePosText.size() && !ParseInt64Strict(lastBytePosText, lastBytePos)) return false;

    if (firstBytePos < 0 || !(lastBytePos == -1 || lastBytePos >= firstBytePos)) return false;

    rangeOffset = firstBytePos;
    rangeEnd = lastBytePos;
    return true;
}

}  // namespace

extern "C" {

WebCore::XFrameOptionsDisposition APS5_VABI _ZN7WebCore24parseXFrameOptionsHeaderERKN3WTF6StringE(const NKWebKit::WtfAbi::String& header) {
    const auto* impl = header.impl;
    if (!impl || !impl->length) return XFrameOptionsDisposition::None;
    if (!impl->data) throw std::invalid_argument("null WTF::String data");
    constexpr unsigned Is8Bit = 4;
    if (impl->hashAndFlags & Is8Bit) return ParseFrameOptions<char>(impl->data, impl->length);
    return ParseFrameOptions<char16_t>(impl->data, impl->length);
}
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

bool APS5_VABI _ZN7WebCore10parseRangeERKN3WTF6StringERxS4_S4_(const NKWebKit::WtfAbi::String* range, long long* rangeOffset, long long* rangeEnd, long long* rangeSuffixLength) {
    const CodeUnits text(Checked(range));
    return ParseRange(text, Checked(rangeOffset), Checked(rangeEnd), Checked(rangeSuffixLength));
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
