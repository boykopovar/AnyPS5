/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Derived from Source/WebCore/platform/network/ParsedContentType.cpp@c55364d53f971f63a709976f168d932d558b8909
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
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 *
 * Derived from Source/WebCore/platform/network/HTTPParsers.cpp@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WebCore/platform/network/HTTPParsers.h@c55364d53f971f63a709976f168d932d558b8909
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
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/loader/HTTPHeaderField.cpp@c55364d53f971f63a709976f168d932d558b8909
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

/*
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Levin <levin@chromium.org>
 *
 * Derived from Source/WTF/wtf/HashTable.h@c55364d53f971f63a709976f168d932d558b8909
 * Derived from Source/WTF/wtf/text/StringImpl.h@c55364d53f971f63a709976f168d932d558b8909
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "prx/libSceNKWebKit/include/ParsedContentTypeExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using WebCore::Mode;
using WebCore::OptionalParsedContentType;
using WebCore::ParsedContentType;
using WebCore::ParsedContentTypeNames;
using WebCore::ParsedContentTypeParameter;

constexpr unsigned Is8BitFlag = 1u << 2;
constexpr unsigned HashFlagCount = 8;
constexpr std::size_t MetadataSize = 4 * sizeof(unsigned);
constexpr int TableSizeOffset = -1;
constexpr int TableSizeMaskOffset = -2;
constexpr int KeyCountOffset = -3;
constexpr int DeletedCountOffset = -4;
constexpr unsigned MinimumTableSize = 8;
constexpr unsigned MinLoad = 6;
constexpr unsigned MaxSmallTableCapacity = 1024;
constexpr unsigned MinimumVectorCapacity = 16;

struct Text {
    std::u16string units;
    bool is8Bit = true;
};

StringImpl* Retain(StringImpl* impl) {
    if (impl) impl->refCount += 2;
    return impl;
}

Text TextOf(const StringImpl* impl) {
    Text text;
    if (!impl || !impl->length) return text;
    if (!impl->data) throw std::invalid_argument("null WTF::StringImpl characters");
    text.is8Bit = impl->hashAndFlags & Is8BitFlag;
    if (text.is8Bit) {
        const auto* characters = static_cast<const unsigned char*>(impl->data);
        text.units.assign(characters, characters + impl->length);
    } else {
        const auto* characters = static_cast<const char16_t*>(impl->data);
        text.units.assign(characters, characters + impl->length);
    }
    return text;
}

StringImpl* Take(String string) {
    return std::exchange(string.impl, nullptr);
}

StringImpl* MakeString(std::u16string_view units, bool is8Bit) {
    if (is8Bit) {
        std::string latin1(units.size(), '\0');
        std::transform(units.begin(), units.end(), latin1.begin(), [](char16_t c) { return static_cast<char>(c); });
        return Take(_ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(latin1.data()), static_cast<unsigned>(latin1.size())));
    }
    return Take(_ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size())));
}

bool IsHTTPSpace(char16_t c) {
    return c <= ' ' && (c == ' ' || c == '\n' || c == '\t' || c == '\r');
}

bool IsASCIISpace(char16_t c) {
    return c <= ' ' && (c == ' ' || (c <= 0xd && c >= 0x9));
}

bool IsSpaceOrNewline(char16_t c) {
    if (c <= 0xff) return IsASCIISpace(c);
    return c == 0x1680 || (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x205f || c == 0x3000;
}

bool IsASCIIAlpha(char16_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool IsASCIIDigit(char16_t c) {
    return c >= '0' && c <= '9';
}

bool IsRFC7230TokenCharacter(char16_t c) {
    return IsASCIIAlpha(c) || IsASCIIDigit(c) || c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' || c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
}

bool IsValidHTTPToken(std::u16string_view value) {
    if (value.empty()) return false;
    return std::all_of(value.begin(), value.end(), IsRFC7230TokenCharacter);
}

bool IsQuotedStringTokenCharacter(char16_t c) {
    return (c >= ' ' && c <= '~') || (c >= 0x80 && c <= 0xff) || c == '\t';
}

bool IsTokenCharacter(char16_t c) {
    return c <= 0x7f && c > ' ' && c != '"' && c != '(' && c != ')' && c != ',' && c != '/' && (c < ':' || c > '@') && (c < '[' || c > ']');
}

std::u16string_view StripMatched(std::u16string_view value, bool (*matches)(char16_t)) {
    while (!value.empty() && matches(value.front())) value.remove_prefix(1);
    while (!value.empty() && matches(value.back())) value.remove_suffix(1);
    return value;
}

std::u16string ASCIILowercase(std::u16string_view value) {
    std::u16string result(value);
    for (auto& c : result) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char16_t>(c + ('a' - 'A'));
    }
    return result;
}

using Range = std::optional<std::u16string_view>;

void SkipSpaces(std::u16string_view input, unsigned& index) {
    while (index < input.size() && IsHTTPSpace(input[index])) ++index;
}

Range ParseToken(std::u16string_view input, unsigned& index, bool (*condition)(char16_t), Mode mode, bool skipTrailingWhitespace = false) {
    const unsigned length = static_cast<unsigned>(input.size());
    const unsigned start = index;
    if (index >= length) return std::nullopt;
    while (index < length && condition(input[index])) {
        if (mode == Mode::Rfc2045 && !IsTokenCharacter(input[index])) break;
        ++index;
    }
    if (index == start) return std::nullopt;
    if (skipTrailingWhitespace) {
        while (input[index - 1] == ' ') --index;
    }
    return input.substr(start, index - start);
}

bool IsNotQuoteOrBackslash(char16_t c) {
    return c != '"' && c != '\\';
}

bool IsNotForwardSlash(char16_t c) {
    return c != '/';
}

bool IsNotSemicolon(char16_t c) {
    return c != ';';
}

bool IsNotSemicolonOrEqualSign(char16_t c) {
    return c != ';' && c != '=';
}

std::u16string CollectHTTPQuotedString(std::u16string_view input, unsigned& index) {
    const unsigned length = static_cast<unsigned>(input.size());
    ++index;
    std::u16string builder;
    while (true) {
        const unsigned start = index;
        ParseToken(input, index, IsNotQuoteOrBackslash, Mode::MimeSniff);
        builder.append(input.substr(start, index - start));
        if (index >= length) break;
        const char16_t quoteOrBackslash = input[index++];
        if (quoteOrBackslash == '\\') {
            if (index >= length) {
                builder.push_back(quoteOrBackslash);
                break;
            }
            builder.push_back(input[index++]);
        } else break;
    }
    return builder;
}

bool ContainsNonTokenCharacters(std::u16string_view input, Mode mode) {
    if (mode == Mode::MimeSniff) return !IsValidHTTPToken(input);
    return !std::all_of(input.begin(), input.end(), IsTokenCharacter);
}

Range ParseQuotedString(std::u16string_view input, unsigned& index) {
    const unsigned length = static_cast<unsigned>(input.size());
    const unsigned start = index + 1;
    if (index >= length) return std::nullopt;
    if (input[index++] != '"' || index >= length) return std::nullopt;
    bool lastCharacterWasBackslash = false;
    char current;
    while ((current = static_cast<char>(input[index++])) != '"' || lastCharacterWasBackslash) {
        if (index >= length) return std::nullopt;
        if (current == '\\' && !lastCharacterWasBackslash) {
            lastCharacterWasBackslash = true;
            continue;
        }
        if (lastCharacterWasBackslash) lastCharacterWasBackslash = false;
    }
    if (input[index - 1] == '"') ++index;
    return input.substr(start, index - start);
}

unsigned DoubleHash(unsigned key) {
    key = ~key + (key >> 23);
    key ^= (key << 12);
    key ^= (key >> 7);
    key ^= (key << 2);
    key ^= (key >> 20);
    return key;
}

unsigned* Metadata(ParsedContentTypeParameter* table) {
    return reinterpret_cast<unsigned*>(table);
}

unsigned KeyHash(const StringImpl* key) {
    const unsigned hash = key->hashAndFlags >> HashFlagCount;
    return hash ? hash : _ZNK3WTF10StringImpl12hashSlowCaseEv(key);
}

bool ShouldExpand(unsigned keyAndDeletedCount, unsigned tableSize) {
    if (tableSize <= MaxSmallTableCapacity) return std::uint64_t(keyAndDeletedCount) * 4 >= std::uint64_t(tableSize) * 3;
    return std::uint64_t(keyAndDeletedCount) * 2 >= tableSize;
}

ParsedContentTypeParameter* AllocateTable(unsigned tableSize) {
    auto* storage = static_cast<unsigned char*>(_ZN3WTF16fastZeroedMallocEm(MetadataSize + std::size_t(tableSize) * sizeof(ParsedContentTypeParameter)));
    auto* table = reinterpret_cast<ParsedContentTypeParameter*>(storage + MetadataSize);
    Metadata(table)[TableSizeOffset] = tableSize;
    Metadata(table)[TableSizeMaskOffset] = tableSize - 1;
    return table;
}

ParsedContentTypeParameter* Probe(ParsedContentTypeParameter* table, const StringImpl* key, bool& found) {
    const unsigned sizeMask = Metadata(table)[TableSizeMaskOffset];
    const unsigned hash = KeyHash(key);
    unsigned index = hash & sizeMask;
    unsigned step = 0;
    while (true) {
        ParsedContentTypeParameter* entry = table + index;
        if (!entry->name) {
            found = false;
            return entry;
        }
        if (_ZN3WTF5equalEPKNS_10StringImplES2_(entry->name, key)) {
            found = true;
            return entry;
        }
        if (!step) step = 1 | DoubleHash(hash);
        index = (index + step) & sizeMask;
    }
}

void Rehash(ParsedContentTypeParameter*& table, unsigned newTableSize) {
    ParsedContentTypeParameter* oldTable = table;
    const unsigned oldTableSize = oldTable ? Metadata(oldTable)[TableSizeOffset] : 0;
    const unsigned keyCount = oldTable ? Metadata(oldTable)[KeyCountOffset] : 0;
    table = AllocateTable(newTableSize);
    Metadata(table)[KeyCountOffset] = keyCount;
    for (unsigned index = 0; index < oldTableSize; ++index) {
        if (!oldTable[index].name) continue;
        bool found = false;
        *Probe(table, oldTable[index].name, found) = oldTable[index];
    }
    if (oldTable) _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(oldTable) - MetadataSize);
}

void Expand(ParsedContentTypeParameter*& table) {
    const unsigned oldSize = table ? Metadata(table)[TableSizeOffset] : 0;
    if (!oldSize) return Rehash(table, MinimumTableSize);
    const bool rehashInPlace = Metadata(table)[KeyCountOffset] * MinLoad < oldSize * 2;
    Rehash(table, rehashInPlace ? oldSize : oldSize * 2);
}

ParsedContentTypeParameter* Find(ParsedContentTypeParameter* table, const StringImpl* key) {
    if (!table) return nullptr;
    bool found = false;
    ParsedContentTypeParameter* entry = Probe(table, key, found);
    return found ? entry : nullptr;
}

void SetParameter(ParsedContentTypeParameter*& table, StringImpl* name, StringImpl* value) {
    if (!table) Expand(table);
    bool found = false;
    ParsedContentTypeParameter* entry = Probe(table, name, found);
    if (found) {
        NKWebKit::WtfAbi::Release(std::exchange(entry->value, Retain(value)));
        return;
    }
    entry->name = Retain(name);
    entry->value = Retain(value);
    unsigned* metadata = Metadata(table);
    metadata[KeyCountOffset] += 1;
    if (ShouldExpand(metadata[KeyCountOffset] + metadata[DeletedCountOffset], metadata[TableSizeOffset])) Expand(table);
}

void AppendName(ParsedContentTypeNames& names, StringImpl* name) {
    if (names.size == names.capacity) {
        const unsigned grown = names.capacity + names.capacity / 4 + 1;
        const unsigned capacity = std::max({names.size + 1, MinimumVectorCapacity, grown});
        names.buffer = static_cast<StringImpl**>(_ZN3WTF11fastReallocEPvm(names.buffer, std::size_t(capacity) * sizeof(StringImpl*)));
        names.capacity = capacity;
    }
    names.buffer[names.size++] = Retain(name);
}

class Parser {
public:
    Parser(ParsedContentType& target, Text contentType)
        : m_target(target)
        , m_input(std::move(contentType)) {}

    bool Parse(Mode mode);

private:
    std::u16string_view Input() const { return m_input.units; }
    void SetContentType(std::u16string_view range, Mode mode);
    void SetContentTypeParameter(std::u16string_view name, std::u16string_view value, Mode mode);

    ParsedContentType& m_target;
    Text m_input;
};

void Parser::SetContentType(std::u16string_view range, Mode mode) {
    const std::u16string mimeType = mode == Mode::MimeSniff ? ASCIILowercase(StripMatched(range, IsHTTPSpace)) : std::u16string(StripMatched(range, IsSpaceOrNewline));
    NKWebKit::WtfAbi::Release(std::exchange(m_target.m_mimeType, MakeString(mimeType, m_input.is8Bit)));
}

void Parser::SetContentTypeParameter(std::u16string_view keyName, std::u16string_view keyValue, Mode mode) {
    std::u16string name(keyName);
    if (mode == Mode::MimeSniff) {
        String original(MakeString(name, m_input.is8Bit));
        if (Find(m_target.m_parameterValues, original.impl) || !IsValidHTTPToken(name) || !std::all_of(keyValue.begin(), keyValue.end(), IsQuotedStringTokenCharacter)) return;
        name = ASCIILowercase(name);
    }
    String nameString(MakeString(name, m_input.is8Bit));
    String valueString(MakeString(keyValue, m_input.is8Bit));
    SetParameter(m_target.m_parameterValues, nameString.impl, valueString.impl);
    AppendName(m_target.m_parameterNames, nameString.impl);
}

bool Parser::Parse(Mode mode) {
    const std::u16string_view input = Input();
    if (mode == Mode::Rfc2045 && input.find_first_of(u"\r\n") != std::u16string_view::npos) return false;
    unsigned index = 0;
    const unsigned length = static_cast<unsigned>(input.size());
    SkipSpaces(input, index);
    if (index >= length) return false;

    const unsigned contentTypeStart = index;
    const Range typeRange = ParseToken(input, index, IsNotForwardSlash, mode);
    if (!typeRange || ContainsNonTokenCharacters(*typeRange, mode)) return false;
    if (index >= length || input[index++] != '/') return false;

    const Range subTypeRange = ParseToken(input, index, IsNotSemicolon, mode, mode == Mode::MimeSniff);
    if (!subTypeRange || ContainsNonTokenCharacters(*subTypeRange, mode)) return false;

    const std::size_t semicolon = input.find(';', contentTypeStart);
    if (semicolon == std::u16string_view::npos) {
        SetContentType(input.substr(contentTypeStart), mode);
        return true;
    }

    SetContentType(input.substr(contentTypeStart, semicolon - contentTypeStart), mode);
    index = static_cast<unsigned>(semicolon) + 1;
    while (true) {
        SkipSpaces(input, index);
        const Range keyRange = ParseToken(input, index, IsNotSemicolonOrEqualSign, mode);
        if (mode == Mode::Rfc2045 && (!keyRange || index >= length)) return false;

        if (mode == Mode::Rfc2045) {
            if (index >= length || input[index++] != '=') return false;
        } else {
            if (index >= length) break;
            if (input[index] != '=' && input[index] != ';') return false;
            if (input[index++] == ';') continue;
        }

        std::optional<std::u16string> parameterValue;
        Range valueRange;
        if (index < length && input[index] == '"') {
            if (mode == Mode::MimeSniff) {
                parameterValue = CollectHTTPQuotedString(input, index);
                ParseToken(input, index, IsNotSemicolon, mode);
            } else valueRange = ParseQuotedString(input, index);
        } else valueRange = ParseToken(input, index, IsNotSemicolon, mode, mode == Mode::MimeSniff);

        if (!parameterValue) {
            if (!valueRange) {
                if (mode == Mode::MimeSniff) continue;
                return false;
            }
            parameterValue = std::u16string(*valueRange);
        }

        if (mode == Mode::Rfc2045 && index < length && input[index++] != ';') return false;

        if (keyRange) SetContentTypeParameter(*keyRange, *parameterValue, mode);

        if (index >= length) return true;
    }
    return true;
}

void Destroy(ParsedContentType& value) {
    if (ParsedContentTypeParameter* table = value.m_parameterValues) {
        const unsigned tableSize = Metadata(table)[TableSizeOffset];
        for (unsigned index = 0; index < tableSize; ++index) {
            NKWebKit::WtfAbi::Release(table[index].name);
            NKWebKit::WtfAbi::Release(table[index].value);
        }
        _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(table) - MetadataSize);
    }
    for (unsigned index = 0; index < value.m_parameterNames.size; ++index) NKWebKit::WtfAbi::Release(value.m_parameterNames.buffer[index]);
    if (value.m_parameterNames.buffer) _ZN3WTF8fastFreeEPv(value.m_parameterNames.buffer);
    NKWebKit::WtfAbi::Release(value.m_contentType);
    NKWebKit::WtfAbi::Release(value.m_mimeType);
    value = {};
}

}

extern "C" {

OptionalParsedContentType* APS5_VABI _ZN7WebCore17ParsedContentType6createERKN3WTF6StringENS_4ModeE(
    OptionalParsedContentType* result, const String* contentType, Mode mode) {
    if (!result || !contentType) throw std::invalid_argument("ParsedContentType::create requires a result and a content type");
    if (mode != Mode::Rfc2045 && mode != Mode::MimeSniff) throw std::invalid_argument("ParsedContentType::create mode is out of range");
    Text text = TextOf(contentType->impl);
    ParsedContentType value {};
    if (mode == Mode::Rfc2045) value.m_contentType = Retain(contentType->impl);
    else {
        const std::u16string_view stripped = StripMatched(text.units, IsHTTPSpace);
        if (stripped.size() == text.units.size()) value.m_contentType = Retain(contentType->impl);
        else {
            text.units = std::u16string(stripped);
            value.m_contentType = MakeString(text.units, text.is8Bit);
        }
    }
    bool parsed = false;
    try {
        parsed = Parser(value, std::move(text)).Parse(mode);
    } catch (...) {
        Destroy(value);
        throw;
    }
    if (!parsed) {
        Destroy(value);
        result->engaged = false;
        return result;
    }
    result->engaged = true;
    result->value = value;
    return result;
}

String APS5_VABI _ZNK7WebCore17ParsedContentType9serializeEv(const ParsedContentType* self) {
    if (!self) throw std::invalid_argument("ParsedContentType::serialize requires an object");
    Text mimeType = TextOf(self->m_mimeType);
    std::u16string builder = std::move(mimeType.units);
    bool is8Bit = mimeType.is8Bit;
    for (unsigned index = 0; index < self->m_parameterNames.size; ++index) {
        StringImpl* nameImpl = self->m_parameterNames.buffer[index];
        if (!nameImpl) throw std::invalid_argument("ParsedContentType parameter name is null");
        const Text name = TextOf(nameImpl);
        const ParsedContentTypeParameter* entry = Find(self->m_parameterValues, nameImpl);
        const Text value = TextOf(entry ? entry->value : nullptr);
        is8Bit = is8Bit && name.is8Bit && value.is8Bit;
        builder.push_back(';');
        builder.append(name.units);
        builder.push_back('=');
        if (value.units.empty() || !IsValidHTTPToken(value.units)) {
            builder.push_back('"');
            for (const char16_t c : value.units) {
                if (c == '\\' || c == '"') builder.push_back('\\');
                builder.push_back(c);
            }
            builder.push_back('"');
        } else builder.append(value.units);
    }
    return String(MakeString(builder, is8Bit));
}

}
