// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/UUID.cpp
/*
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/URL.cpp
/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/URLParser.cpp
/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/UrlHelpers.hpp"

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using NKWebKit::UrlAbi::StringView;
using NKWebKit::UrlAbi::FormField;
using NKWebKit::UrlAbi::FormFields;
using NKWebKit::UrlAbi::OptionalString;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;

std::mutex portOverridesMutex;
std::map<std::u16string, std::uint16_t> portOverrides;

StringView View(const String& value) {
    if (!value.impl) return {nullptr, 0, true};
    return {value.impl->data, value.impl->length, (value.impl->hashAndFlags & 4) != 0};
}

void CheckCharacters(StringView value) {
    if (!value.characters && value.length) throw std::invalid_argument("WTF::StringView: null characters with nonzero length");
}

char16_t CharacterAt(StringView value, std::uint32_t index) {
    if (value.is8Bit) return static_cast<const std::uint8_t*>(value.characters)[index];
    return static_cast<const char16_t*>(value.characters)[index];
}

std::u16string CopyString(StringView value) {
    CheckCharacters(value);
    std::u16string result;
    result.reserve(value.length);
    for (std::uint32_t i = 0; i < value.length; ++i) result.push_back(CharacterAt(value, i));
    return result;
}

bool AsciiWhitespace(char16_t value) {
    return value == ' ' || (value >= '\t' && value <= '\r');
}

std::optional<std::uint64_t> HexGroup(StringView value, std::uint32_t start, std::uint32_t count, bool rejectLeadingPlus) {
    std::uint32_t end = start + count;
    if (rejectLeadingPlus && CharacterAt(value, start) == '+') return std::nullopt;
    while (start < end && AsciiWhitespace(CharacterAt(value, start))) ++start;
    if (start < end && CharacterAt(value, start) == '+') ++start;
    std::uint64_t result = 0;
    bool haveDigits = false;
    while (start < end) {
        const char16_t c = CharacterAt(value, start);
        std::uint32_t digit = 0;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else break;
        haveDigits = true;
        result = result * 16 + digit;
        ++start;
    }
    if (!haveDigits) return std::nullopt;
    while (start < end && AsciiWhitespace(CharacterAt(value, start))) ++start;
    if (start != end) return std::nullopt;
    return result;
}

bool UuidVersion4(StringView value) {
    if (value.length != 36) return false;
    if (!value.characters) throw std::invalid_argument("WTF::StringView: null UUID characters");
    for (const auto separator : {8u, 13u, 18u, 23u}) {
        if (CharacterAt(value, separator) != '-') return false;
    }
    const auto first = HexGroup(value, 0, 8, true);
    const auto second = HexGroup(value, 9, 4, true);
    const auto third = HexGroup(value, 14, 4, false);
    const auto fourth = HexGroup(value, 19, 4, true);
    const auto fifth = HexGroup(value, 24, 12, true);
    if (!first || !second || !third || !fourth || !fifth) return false;
    return ((*third & 0xf000u) == 0x4000u) && ((*fourth & 0xc000u) == 0x8000u);
}

std::optional<std::uint16_t> DefaultPort(StringView protocol) {
    const auto key = CopyString(protocol);
    if (protocol.characters) {
        const std::lock_guard lock(portOverridesMutex);
        if (const auto found = portOverrides.find(key); found != portOverrides.end()) return found->second;
    }
    if (key == u"ftp") return 21;
    if (key == u"http" || key == u"ws") return 80;
    if (key == u"https" || key == u"wss") return 443;
    return std::nullopt;
}

bool IsAsciiAlpha(char16_t value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

bool IsAsciiAlphanumeric(char16_t value) {
    return IsAsciiAlpha(value) || (value >= '0' && value <= '9');
}

CString StrictUtf8(StringView view) {
    CheckCharacters(view);
    const NKWebKit::WtfAbi::StringView input{view.characters, view.length, view.is8Bit};
    try {
        return _ZNK3WTF10StringView4utf8ENS_14ConversionModeE(&input, 1);
    } catch (const std::invalid_argument&) {
        return CString();
    }
}

int HexDigit(unsigned char byte) {
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return -1;
}

template<typename Char>
String DecodeFormComponent(std::basic_string_view<Char> component) {
    const auto utf8 = StrictUtf8({component.data(), static_cast<unsigned>(component.size()), sizeof(Char) == 1});
    if (!utf8.buffer) return String();

    const auto* bytes = reinterpret_cast<const unsigned char*>(utf8.buffer + 1);
    const auto length = utf8.buffer->length;
    std::string decoded;
    decoded.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        const auto byte = bytes[i];
        if (byte == '%' && length - i >= 3) {
            const int high = HexDigit(bytes[i + 1]);
            const int low = HexDigit(bytes[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>(high * 16 + low));
                i += 2;
                continue;
            }
        }
        decoded.push_back(byte == '+' ? ' ' : static_cast<char>(byte));
    }
    return NKWebKit::WtfAbi::FromUTF8ReplacingInvalidSequences_nid_no_patch(
        reinterpret_cast<const unsigned char*>(decoded.data()), decoded.size());
}

template<typename Char>
FormFields ParseForm(std::basic_string_view<Char> remaining) {
    std::vector<FormField> fields;
    while (!remaining.empty()) {
        const auto separator = remaining.find('&');
        const auto field = remaining.substr(0, separator);
        if (!field.empty()) {
            const auto equal = field.find('=');
            auto key = DecodeFormComponent(field.substr(0, equal));
            auto value = DecodeFormComponent(equal == field.npos ? field.substr(field.size()) : field.substr(equal + 1));
            if (key.impl && value.impl) fields.push_back({std::move(key), std::move(value)});
        }
        if (separator == remaining.npos) break;
        remaining.remove_prefix(separator + 1);
    }

    FormFields result(static_cast<unsigned>(fields.size()));
    for (auto& field : fields) {
        new (&result.buffer[result.size]) FormField(std::move(field));
        ++result.size;
    }
    return result;
}

void EncodeFormComponent(const String& value, std::string& output) {
    const auto utf8 = StrictUtf8(View(value));
    if (!utf8.buffer) return;
    const auto* bytes = reinterpret_cast<const unsigned char*>(utf8.buffer + 1);
    constexpr char hex[] = "0123456789ABCDEF";
    for (std::size_t i = 0; i < utf8.buffer->length; ++i) {
        const auto byte = bytes[i];
        if (byte == ' ') {
            output.push_back('+');
        } else if (IsAsciiAlphanumeric(byte) || byte == '*' || byte == '-' || byte == '.' || byte == '_') {
            output.push_back(static_cast<char>(byte));
        } else {
            output.push_back('%');
            output.push_back(hex[byte >> 4]);
            output.push_back(hex[byte & 15]);
        }
    }
}

} // namespace

extern "C" {

bool APS5_VABI _ZN3WTF14isVersion4UUIDENS_10StringViewE(StringView value) {
    return UuidVersion4(value);
}

bool APS5_VABI _ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(std::uint16_t port, StringView protocol) {
    return DefaultPort(protocol) == port;
}

void APS5_VABI _ZN3WTF40clearDefaultPortForProtocolMapForTestingEv() {
    const std::lock_guard lock(portOverridesMutex);
    portOverrides.clear();
}

void APS5_VABI _ZN3WTF40registerDefaultPortForProtocolForTestingEtRKNS_6StringE(std::uint16_t port, const String& protocol) {
    if (!protocol.impl) throw std::invalid_argument("WTF::defaultPortForProtocol: null override key");
    auto key = CopyString(View(protocol));
    const std::lock_guard lock(portOverridesMutex);
    portOverrides.try_emplace(std::move(key), port);
}

OptionalString APS5_VABI _ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(const String& scheme) {
    const auto view = View(scheme);
    CheckCharacters(view);
    if (!view.length || !IsAsciiAlpha(CharacterAt(view, 0))) return {};
    for (unsigned i = 1; i < view.length; ++i) {
        const auto character = CharacterAt(view, i);
        if (!IsAsciiAlphanumeric(character) && character != '+' && character != '-' && character != '.') return {};
    }
    return {true, _ZNK3WTF6String23convertToASCIILowercaseEv(&scheme)};
}

FormFields APS5_VABI _ZN3WTF9URLParser19parseURLEncodedFormENS_10StringViewE(StringView input) {
    CheckCharacters(input);
    if (!input.length) return {};
    if (input.is8Bit) return ParseForm(std::string_view(static_cast<const char*>(input.characters), input.length));
    return ParseForm(std::u16string_view(static_cast<const char16_t*>(input.characters), input.length));
}

String APS5_VABI _ZN3WTF9URLParser9serializeERKNS_6VectorINS_12KeyValuePairINS_6StringES3_EELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(const FormFields& fields) {
    if (!fields.size) return String();
    if (!fields.buffer || fields.size > fields.capacity) throw std::invalid_argument("WTF::URLParser: invalid form vector");
    std::string output;
    for (unsigned i = 0; i < fields.size; ++i) {
        if (i) output.push_back('&');
        EncodeFormComponent(fields[i].key, output);
        output.push_back('=');
        EncodeFormComponent(fields[i].value, output);
    }
    if (output.size() > 0x7FFFFFFF) throw std::length_error("WTF::URLParser: serialized form too long");
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(output.data()), static_cast<unsigned>(output.size()));
}

}
