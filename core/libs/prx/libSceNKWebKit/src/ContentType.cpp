/*
 * Copyright (C) 2006-2021 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 *
 * Derived from Source/WebCore/platform/ContentType.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/ContentTypeExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::ContentType;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

constexpr std::size_t notFound = std::numeric_limits<std::size_t>::max();

const ContentType& Checked(const ContentType* self) {
    if (!self) throw std::invalid_argument("null WebCore::ContentType");
    return *self;
}

ContentType& Checked(ContentType* self) {
    if (!self) throw std::invalid_argument("null WebCore::ContentType");
    return *self;
}

String& Checked(String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

const String& Checked(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

bool Is8Bit(const StringImpl& impl) {
    return impl.hashAndFlags & 4;
}

char16_t Unit(const StringImpl& impl, std::size_t index) {
    return Is8Bit(impl) ? static_cast<const unsigned char*>(impl.data)[index] : static_cast<const char16_t*>(impl.data)[index];
}

bool IsASCIIWhitespace(char16_t value) {
    return value == ' ' || value == '\t' || value == '\n' || value == '\f' || value == '\r';
}

char16_t ASCIILower(char16_t value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char16_t>(value | 0x20) : value;
}

std::size_t FindCharacter(const String& string, char16_t character, unsigned start) {
    if (!string.impl) return notFound;
    for (std::size_t i = start; i < string.impl->length; ++i) {
        if (Unit(*string.impl, i) == character) return i;
    }
    return notFound;
}

std::size_t FindIgnoringASCIICase(const String& string, const String& match, unsigned start) {
    if (!string.impl || !match.impl) return notFound;
    const unsigned length = string.impl->length;
    const unsigned matchLength = match.impl->length;
    if (!matchLength) return std::min(start, length);
    if (start > length || matchLength > length - start) return notFound;
    for (std::size_t i = start; i + matchLength <= length; ++i) {
        std::size_t j = 0;
        while (j < matchLength && ASCIILower(Unit(*string.impl, i + j)) == ASCIILower(Unit(*match.impl, j))) ++j;
        if (j == matchLength) return i;
    }
    return notFound;
}

String TrimmedSubstring(const String& string, unsigned start, unsigned length) {
    const StringImpl& impl = *string.impl;
    const unsigned size = impl.length;
    if (start > size) start = size;
    if (length > size - start) length = size - start;
    unsigned end = start + length;
    while (start < end && IsASCIIWhitespace(Unit(impl, start))) ++start;
    while (end > start && IsASCIIWhitespace(Unit(impl, end - 1))) --end;
    return _ZN3WTF10StringImpl9substringEjj(&impl, start, end - start);
}

}

extern "C" {

const String& APS5_VABI _ZN7WebCore11ContentType15codecsParameterEv() {
    static const String* const codecs = new String(_ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("codecs"), 6));
    return *codecs;
}

void APS5_VABI _ZN7WebCore11ContentTypeC1EON3WTF6StringE(ContentType* self, String* type) {
    auto& contentType = Checked(self);
    auto& source = Checked(type);
    contentType.type.impl = std::exchange(source.impl, nullptr);
}

void APS5_VABI _ZN7WebCore11ContentTypeC1ERKN3WTF6StringE(ContentType* self, const String* type) {
    auto& contentType = Checked(self);
    String copy(Checked(type));
    contentType.type.impl = std::exchange(copy.impl, nullptr);
}

void APS5_VABI _ZN7WebCore11ContentTypeC2EON3WTF6StringE(ContentType* self, String* type) {
    _ZN7WebCore11ContentTypeC1EON3WTF6StringE(self, type);
}

void APS5_VABI _ZN7WebCore11ContentTypeC2ERKN3WTF6StringE(ContentType* self, const String* type) {
    _ZN7WebCore11ContentTypeC1ERKN3WTF6StringE(self, type);
}

String APS5_VABI _ZNK7WebCore11ContentType13containerTypeEv(const ContentType* self) {
    const String& type = Checked(self).type;
    if (!type.impl) return String();
    const std::size_t semicolon = FindCharacter(type, ';', 0);
    const unsigned length = semicolon == notFound ? type.impl->length : static_cast<unsigned>(semicolon);
    return TrimmedSubstring(type, 0, length);
}

String APS5_VABI _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE(const ContentType* self, const String* parameterName) {
    const String& type = Checked(self).type;
    const String& name = Checked(parameterName);
    const std::size_t semicolonPosition = FindCharacter(type, ';', 0);
    if (semicolonPosition == notFound) return String();

    const std::size_t nameStart = FindIgnoringASCIICase(type, name, static_cast<unsigned>(semicolonPosition + 1));
    if (nameStart == notFound) return String();

    const unsigned nameLength = name.impl ? name.impl->length : 0;
    const std::size_t equalSignPosition = FindCharacter(type, '=', static_cast<unsigned>(nameStart + nameLength));
    if (equalSignPosition == notFound) return String();

    const std::size_t quotePosition = FindCharacter(type, '"', static_cast<unsigned>(equalSignPosition + 1));
    const std::size_t secondQuotePosition = FindCharacter(type, '"', static_cast<unsigned>(quotePosition + 1));
    std::size_t start;
    std::size_t end;
    if (quotePosition != notFound && secondQuotePosition != notFound) {
        start = quotePosition + 1;
        end = secondQuotePosition;
    } else {
        start = equalSignPosition + 1;
        end = FindCharacter(type, ';', static_cast<unsigned>(start));
    }
    return TrimmedSubstring(type, static_cast<unsigned>(start), static_cast<unsigned>(end - start));
}

}
