// Derived from Source/WebCore/loader/HTTPHeaderField.cpp@78802c1a045dfa8b124c52833f908f3e7f4edd8e
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/loader/HTTPHeaderField.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/HTTPHeaderFieldExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::HTTPHeaderField;
using NKWebKit::WebCoreAbi::OptionalHTTPHeaderField;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

HTTPHeaderField& Checked(HTTPHeaderField* self) {
    if (!self) throw std::invalid_argument("null WebCore::HTTPHeaderField");
    return *self;
}

const HTTPHeaderField& Checked(const HTTPHeaderField* self) {
    if (!self) throw std::invalid_argument("null WebCore::HTTPHeaderField");
    return *self;
}

String& Checked(String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

void Construct(HTTPHeaderField* self, String* name, String* value) {
    auto& field = Checked(self);
    auto& nameString = Checked(name);
    auto& valueString = Checked(value);
    field.name.impl = std::exchange(nameString.impl, nullptr);
    field.value.impl = std::exchange(valueString.impl, nullptr);
}

void ConstructCopy(HTTPHeaderField* self, const HTTPHeaderField* other) {
    auto& field = Checked(self);
    const auto& source = Checked(other);
    String name(source.name);
    String value(source.value);
    field.name.impl = std::exchange(name.impl, nullptr);
    field.value.impl = std::exchange(value.impl, nullptr);
}

struct Characters {
    const StringImpl* impl;
    unsigned start;
    unsigned length;

    char16_t operator[](unsigned index) const {
        if (impl->hashAndFlags & 4) return static_cast<const unsigned char*>(impl->data)[start + index];
        return static_cast<const char16_t*>(impl->data)[start + index];
    }
};

bool IsASCIIAlphanumeric(char16_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool IsTokenCharacter(char16_t c) {
    return IsASCIIAlphanumeric(c)
        || c == '!' || c == '#' || c == '$'
        || c == '%' || c == '&' || c == '\''
        || c == '*' || c == '+' || c == '-'
        || c == '.' || c == '^' || c == '_'
        || c == '`' || c == '|' || c == '~';
}

bool IsDelimiter(char16_t c) {
    return c == '(' || c == ')' || c == ','
        || c == '/' || c == ':' || c == ';'
        || c == '<' || c == '=' || c == '>'
        || c == '?' || c == '@' || c == '['
        || c == '\\' || c == ']' || c == '{'
        || c == '}' || c == '"';
}

bool IsWhitespace(char16_t c) {
    return c == ' ' || c == '\t';
}

bool IsInRange(char16_t c, char16_t min, char16_t max) {
    return c >= min && c <= max;
}

bool IsOBSText(char16_t c) {
    return IsInRange(c, 0x80, 0xFF);
}

bool IsQuotedTextCharacter(char16_t c) {
    return IsWhitespace(c) || c == 0x21 || IsInRange(c, 0x23, 0x5B) || IsInRange(c, 0x5D, 0x7E) || IsOBSText(c);
}

bool IsQuotedPairSecondOctet(char16_t c) {
    return IsWhitespace(c) || IsTokenCharacter(c) || IsDelimiter(c) || IsOBSText(c);
}

bool IsCommentText(char16_t c) {
    return IsWhitespace(c) || IsInRange(c, 0x21, 0x27) || IsInRange(c, 0x2A, 0x5B) || IsInRange(c, 0x5D, 0x7E) || IsOBSText(c);
}

bool IsValidName(const Characters& name) {
    if (!name.length) return false;
    for (unsigned i = 0; i < name.length; ++i) {
        if (!IsTokenCharacter(name[i])) return false;
    }
    return true;
}

bool IsValidValue(const Characters& value) {
    enum class State { OptionalWhitespace, Token, QuotedString, Comment };
    State state = State::OptionalWhitespace;
    std::size_t commentDepth = 0;
    bool hadNonWhitespace = false;
    for (unsigned i = 0; i < value.length; ++i) {
        char16_t c = value[i];
        switch (state) {
        case State::OptionalWhitespace:
            if (IsWhitespace(c)) continue;
            hadNonWhitespace = true;
            if (IsTokenCharacter(c)) {
                state = State::Token;
                continue;
            }
            if (c == '"') {
                state = State::QuotedString;
                continue;
            }
            if (c == '(') {
                ++commentDepth;
                state = State::Comment;
                continue;
            }
            return false;
        case State::Token:
            if (!IsTokenCharacter(c)) state = State::OptionalWhitespace;
            continue;
        case State::QuotedString:
            if (c == '"') {
                state = State::OptionalWhitespace;
                continue;
            }
            if (c == '\\') {
                if (++i == value.length || !IsQuotedPairSecondOctet(value[i])) return false;
                continue;
            }
            if (!IsQuotedTextCharacter(c)) return false;
            continue;
        case State::Comment:
            if (c == '(') {
                ++commentDepth;
                continue;
            }
            if (c == ')') {
                if (!--commentDepth) state = State::OptionalWhitespace;
                continue;
            }
            if (c == '\\') {
                if (++i == value.length || !IsQuotedPairSecondOctet(value[i])) return false;
                continue;
            }
            if (!IsCommentText(c)) return false;
            continue;
        }
    }
    return (state == State::OptionalWhitespace || state == State::Token) && hadNonWhitespace;
}

Characters StripWhitespace(const String& string) {
    Characters characters{string.impl, 0, string.impl ? string.impl->length : 0};
    while (characters.length && IsWhitespace(characters[0])) {
        ++characters.start;
        --characters.length;
    }
    while (characters.length && IsWhitespace(characters[characters.length - 1])) --characters.length;
    return characters;
}

String TakeOrCopy(String& original, const Characters& stripped) {
    if (stripped.length == original.impl->length) return String(std::move(original));
    if (original.impl->hashAndFlags & 4)
        return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(original.impl->data) + stripped.start, stripped.length);
    return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(original.impl->data) + stripped.start, stripped.length);
}

OptionalHTTPHeaderField Create(String* unparsedName, String* unparsedValue) {
    auto& nameString = Checked(unparsedName);
    auto& valueString = Checked(unparsedValue);
    auto strippedName = StripWhitespace(nameString);
    auto strippedValue = StripWhitespace(valueString);
    OptionalHTTPHeaderField result;
    if (!IsValidName(strippedName) || !IsValidValue(strippedValue)) return result;
    String name = TakeOrCopy(nameString, strippedName);
    String value = TakeOrCopy(valueString, strippedValue);
    result.field.name.impl = std::exchange(name.impl, nullptr);
    result.field.value.impl = std::exchange(value.impl, nullptr);
    result.hasValue = true;
    return result;
}

void Destroy(HTTPHeaderField* self) {
    auto& field = Checked(self);
    String name(std::exchange(field.name.impl, nullptr));
    String value(std::exchange(field.value.impl, nullptr));
}

}

extern "C" {

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC1EON3WTF6StringES3_(HTTPHeaderField* self, String* name, String* value) {
    Construct(self, name, value);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC1ERKS0_(HTTPHeaderField* self, const HTTPHeaderField* other) {
    ConstructCopy(self, other);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC2EON3WTF6StringES3_(HTTPHeaderField* self, String* name, String* value) {
    Construct(self, name, value);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldC2ERKS0_(HTTPHeaderField* self, const HTTPHeaderField* other) {
    ConstructCopy(self, other);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldD1Ev(HTTPHeaderField* self) {
    Destroy(self);
}

void APS5_VABI _ZN7WebCore15HTTPHeaderFieldD2Ev(HTTPHeaderField* self) {
    Destroy(self);
}

HTTPHeaderField& APS5_VABI _ZN7WebCore15HTTPHeaderFieldaSERKS0_(HTTPHeaderField* self, const HTTPHeaderField* other) {
    auto& field = Checked(self);
    const auto& source = Checked(other);
    String name(source.name);
    String value(source.value);
    String oldName(std::exchange(field.name.impl, std::exchange(name.impl, nullptr)));
    String oldValue(std::exchange(field.value.impl, std::exchange(value.impl, nullptr)));
    return field;
}

OptionalHTTPHeaderField APS5_VABI _ZN7WebCore15HTTPHeaderField6createEON3WTF6StringES3_(String* name, String* value) {
    return Create(name, value);
}

const String& APS5_VABI _ZNK7WebCore15HTTPHeaderField4nameEv(const HTTPHeaderField* self) {
    return Checked(self).name;
}

const String& APS5_VABI _ZNK7WebCore15HTTPHeaderField5valueEv(const HTTPHeaderField* self) {
    return Checked(self).value;
}

}
