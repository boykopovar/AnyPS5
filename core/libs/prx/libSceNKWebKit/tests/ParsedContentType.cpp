/*
 * Copyright (C) 2019 Igalia, S.L. All rights reserved.
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

/*
 * Derived from Tools/TestWebKitAPI/Tests/WebCore/ParsedContentType.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using WebCore::Mode;
using WebCore::OptionalParsedContentType;
using WebCore::ParsedContentType;
using WebCore::ParsedContentTypeParameter;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ParsedContentType check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

String Make(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

String Make16(const std::u16string& text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

bool Is8Bit(const StringImpl* impl) {
    return impl->hashAndFlags & 4u;
}

std::u16string Units(const StringImpl* impl) {
    if (!impl || !impl->length) return {};
    if (Is8Bit(impl)) {
        const auto* characters = static_cast<const unsigned char*>(impl->data);
        return std::u16string(characters, characters + impl->length);
    }
    const auto* characters = static_cast<const char16_t*>(impl->data);
    return std::u16string(characters, characters + impl->length);
}

bool Equals(const StringImpl* impl, const char* expected) {
    const std::u16string units = Units(impl);
    if (units.size() != std::strlen(expected)) return false;
    for (std::size_t index = 0; index < units.size(); ++index) {
        if (units[index] != static_cast<unsigned char>(expected[index])) return false;
    }
    return true;
}

unsigned TableSize(const ParsedContentTypeParameter* table) {
    return reinterpret_cast<const unsigned*>(table)[-1];
}

unsigned KeyCount(const ParsedContentTypeParameter* table) {
    return reinterpret_cast<const unsigned*>(table)[-3];
}

const StringImpl* Lookup(const ParsedContentType& value, const char* name) {
    if (!value.m_parameterValues) return nullptr;
    for (unsigned index = 0; index < TableSize(value.m_parameterValues); ++index) {
        const ParsedContentTypeParameter& entry = value.m_parameterValues[index];
        if (entry.name && Equals(entry.name, name)) return entry.value;
    }
    return nullptr;
}

OptionalParsedContentType Create(const String& input, Mode mode) {
    OptionalParsedContentType result {};
    Require(_ZN7WebCore17ParsedContentType6createERKN3WTF6StringENS_4ModeE(&result, &input, mode) == &result);
    return result;
}

void Destroy(OptionalParsedContentType& result) {
    if (!result.engaged) return;
    ParsedContentType& value = result.value;
    if (ParsedContentTypeParameter* table = value.m_parameterValues) {
        for (unsigned index = 0; index < TableSize(table); ++index) {
            NKWebKit::WtfAbi::Release(table[index].name);
            NKWebKit::WtfAbi::Release(table[index].value);
        }
        _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(table) - 16);
    }
    for (unsigned index = 0; index < value.m_parameterNames.size; ++index) NKWebKit::WtfAbi::Release(value.m_parameterNames.buffer[index]);
    if (value.m_parameterNames.buffer) _ZN3WTF8fastFreeEPv(value.m_parameterNames.buffer);
    NKWebKit::WtfAbi::Release(value.m_contentType);
    NKWebKit::WtfAbi::Release(value.m_mimeType);
    result.engaged = false;
}

bool IsValid(const char* input, Mode mode) {
    String string = Make(input);
    OptionalParsedContentType result = Create(string, mode);
    const bool valid = result.engaged;
    Destroy(result);
    return valid;
}

std::string Escape(const StringImpl* impl) {
    std::string result;
    for (const char16_t c : Units(impl)) {
        if (c >= 0x20 && c < 0x7f) {
            result.push_back(static_cast<char>(c));
            continue;
        }
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "{%x}", static_cast<unsigned>(c));
        result += buffer;
    }
    return result;
}

std::string SerializeIfValid(const char* input) {
    String string = Make(input);
    OptionalParsedContentType result = Create(string, Mode::MimeSniff);
    if (!result.engaged) return "NOTVALID";
    String serialized = _ZNK7WebCore17ParsedContentType9serializeEv(&result.value);
    std::string escaped = Escape(serialized.impl);
    Destroy(result);
    return escaped;
}

template<typename Function>
void RequireThrows(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

void TestValidity() {
    Require(IsValid("text/plain", Mode::MimeSniff));
    Require(IsValid(" text/plain", Mode::MimeSniff));
    Require(IsValid(" text/plain ", Mode::MimeSniff));
    Require(!IsValid("text /plain", Mode::MimeSniff));
    Require(!IsValid("text/ plain", Mode::MimeSniff));
    Require(!IsValid("text / plain", Mode::MimeSniff));
    Require(!IsValid("te xt/plain", Mode::MimeSniff));
    Require(!IsValid("text/pla in", Mode::MimeSniff));
    Require(!IsValid("text", Mode::MimeSniff));
    Require(!IsValid("text/", Mode::MimeSniff));
    Require(!IsValid("/plain", Mode::MimeSniff));
    Require(IsValid("text/plain;", Mode::MimeSniff));
    Require(IsValid("text/plain;;", Mode::MimeSniff));
    Require(IsValid("text/plain;;;", Mode::MimeSniff));
    Require(IsValid("text/plain;test", Mode::MimeSniff));
    Require(IsValid("text/plain; test", Mode::MimeSniff));
    Require(IsValid("text/plain;test=", Mode::MimeSniff));
    Require(IsValid("text/plain;test=value", Mode::MimeSniff));
    Require(IsValid("text/plain;;test=value", Mode::MimeSniff));
    Require(IsValid("text/plain;;;test=value", Mode::MimeSniff));
    Require(IsValid("text/plain;test=value;", Mode::MimeSniff));
    Require(IsValid("text/plain;test=value;;", Mode::MimeSniff));
    Require(IsValid("text/plain;test=value;;;", Mode::MimeSniff));
    Require(IsValid("text/plain; test=value", Mode::MimeSniff));
    Require(IsValid("text/plain;test =value", Mode::MimeSniff));
    Require(IsValid("text/plain;test= value", Mode::MimeSniff));
    Require(IsValid("text/plain;test=value ", Mode::MimeSniff));
    Require(IsValid("text/plain;=;test=value", Mode::MimeSniff));
    Require(IsValid("text/plain;test=value;=", Mode::MimeSniff));
    Require(IsValid("text/plain;wrong=;test=value", Mode::MimeSniff));
    Require(IsValid("text/plain;=wrong;test=value", Mode::MimeSniff));
    Require(IsValid("text/plain;test=value;wrong=", Mode::MimeSniff));
    Require(IsValid("text/plain;test=value;=wrong", Mode::MimeSniff));
    Require(IsValid("text/plain;test=\"value\"", Mode::MimeSniff));
    Require(IsValid("text/plain;test=\"value", Mode::MimeSniff));
    Require(IsValid("text/plain;test=\"value\"foo", Mode::MimeSniff));
    Require(IsValid("text/plain;test=\"value\"foo;foo=bar", Mode::MimeSniff));
    Require(IsValid("text/plain", Mode::Rfc2045));
    Require(IsValid(" text/plain", Mode::Rfc2045));
    Require(IsValid(" text/plain ", Mode::Rfc2045));
    Require(!IsValid("text /plain", Mode::Rfc2045));
    Require(!IsValid("text/ plain", Mode::Rfc2045));
    Require(!IsValid("text / plain", Mode::Rfc2045));
    Require(!IsValid("te xt/plain", Mode::Rfc2045));
    Require(IsValid("text/pla in", Mode::Rfc2045));
    Require(!IsValid("text", Mode::Rfc2045));
    Require(!IsValid("text/", Mode::Rfc2045));
    Require(!IsValid("/plain", Mode::Rfc2045));
    Require(!IsValid("text/plain;", Mode::Rfc2045));
    Require(!IsValid("text/plain;;", Mode::Rfc2045));
    Require(!IsValid("text/plain;;;", Mode::Rfc2045));
    Require(!IsValid("text/plain;test", Mode::Rfc2045));
    Require(!IsValid("text/plain; test", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=", Mode::Rfc2045));
    Require(IsValid("text/plain;test=value", Mode::Rfc2045));
    Require(!IsValid("text/plain;;test=value", Mode::Rfc2045));
    Require(!IsValid("text/plain;;;test=value", Mode::Rfc2045));
    Require(IsValid("text/plain;test=value;", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=value;;", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=value;;;", Mode::Rfc2045));
    Require(IsValid("text/plain; test=value", Mode::Rfc2045));
    Require(!IsValid("text/plain;test =value", Mode::Rfc2045));
    Require(!IsValid("text/plain;test= value", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=value ", Mode::Rfc2045));
    Require(!IsValid("text/plain;=;test=value", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=value;=", Mode::Rfc2045));
    Require(!IsValid("text/plain;wrong=;test=value", Mode::Rfc2045));
    Require(!IsValid("text/plain;=wrong;test=value", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=value;wrong=", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=value;=wrong", Mode::Rfc2045));
    Require(IsValid("text/plain;test=\"value\"", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=\"value", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=\"value\"foo", Mode::Rfc2045));
    Require(!IsValid("text/plain;test=\"value\"foo;foo=bar", Mode::Rfc2045));
}

void TestSerialize() {
    Require(SerializeIfValid("") == "NOTVALID");
    Require(SerializeIfValid(" ") == "NOTVALID");
    Require(SerializeIfValid("  ") == "NOTVALID");
    Require(SerializeIfValid("\t") == "NOTVALID");
    Require(SerializeIfValid(";") == "NOTVALID");
    Require(SerializeIfValid(";=") == "NOTVALID");
    Require(SerializeIfValid("=") == "NOTVALID");
    Require(SerializeIfValid("=;") == "NOTVALID");
    Require(SerializeIfValid("text") == "NOTVALID");
    Require(SerializeIfValid("text/") == "NOTVALID");
    Require(SerializeIfValid("text/\0") == "NOTVALID");
    Require(SerializeIfValid("text/ ") == "NOTVALID");
    Require(SerializeIfValid("{/}") == "NOTVALID");
    Require(SerializeIfValid("x/x ;x=x") == "x/x;x=x");
    Require(SerializeIfValid("text/plain") == "text/plain");
    Require(SerializeIfValid("text/plain\0") == "text/plain");
    Require(SerializeIfValid(" text/plain") == "text/plain");
    Require(SerializeIfValid("\ttext/plain") == "text/plain");
    Require(SerializeIfValid("\ntext/plain") == "text/plain");
    Require(SerializeIfValid("\rtext/plain") == "text/plain");
    Require(SerializeIfValid("\btext/plain") == "NOTVALID");
    Require(SerializeIfValid("\x0Btext/plain") == "NOTVALID");
    Require(SerializeIfValid("\x0Btext/plain") == "NOTVALID");
    Require(SerializeIfValid("text/plain ") == "text/plain");
    Require(SerializeIfValid("text/plain\t") == "text/plain");
    Require(SerializeIfValid("text/plain\n") == "text/plain");
    Require(SerializeIfValid("text/plain\r") == "text/plain");
    Require(SerializeIfValid("text/plain\b") == "NOTVALID");
    Require(SerializeIfValid("text/plain\x0B") == "NOTVALID");
    Require(SerializeIfValid("text/plain\x0B") == "NOTVALID");
    Require(SerializeIfValid(" text/plain ") == "text/plain");
    Require(SerializeIfValid("\ttext/plain\t") == "text/plain");
    Require(SerializeIfValid("\ntext/plain\n") == "text/plain");
    Require(SerializeIfValid("\rtext/plain\r") == "text/plain");
    Require(SerializeIfValid("\btext/plain\b") == "NOTVALID");
    Require(SerializeIfValid("text/PLAIN") == "text/plain");
    Require(SerializeIfValid("text /plain") == "NOTVALID");
    Require(SerializeIfValid("text/ plain") == "NOTVALID");
    Require(SerializeIfValid("text / plain") == "NOTVALID");
    Require(SerializeIfValid("te xt/plain") == "NOTVALID");
    Require(SerializeIfValid("text/pla in") == "NOTVALID");
    Require(SerializeIfValid("text\t/plain") == "NOTVALID");
    Require(SerializeIfValid("text/\tplain") == "NOTVALID");
    Require(SerializeIfValid("text\t/\tplain") == "NOTVALID");
    Require(SerializeIfValid("te\txt/plain") == "NOTVALID");
    Require(SerializeIfValid("text/pla\tin") == "NOTVALID");
    Require(SerializeIfValid("text\n/plain") == "NOTVALID");
    Require(SerializeIfValid("text/\nplain") == "NOTVALID");
    Require(SerializeIfValid("text\n/\nplain") == "NOTVALID");
    Require(SerializeIfValid("te\nxt/plain") == "NOTVALID");
    Require(SerializeIfValid("text/pla\nin") == "NOTVALID");
    Require(SerializeIfValid("text\r/plain") == "NOTVALID");
    Require(SerializeIfValid("text/\rplain") == "NOTVALID");
    Require(SerializeIfValid("text\r/\rplain") == "NOTVALID");
    Require(SerializeIfValid("te\rxt/plain") == "NOTVALID");
    Require(SerializeIfValid("text/pla\rin") == "NOTVALID");
    Require(SerializeIfValid("text") == "NOTVALID");
    Require(SerializeIfValid("text/") == "NOTVALID");
    Require(SerializeIfValid("/plain") == "NOTVALID");
    Require(SerializeIfValid("\xE2\x80\xA0/\xE2\x80\xA0") == "NOTVALID");
    Require(SerializeIfValid("text/\xD8\x88\x12\x34") == "NOTVALID");
    Require(SerializeIfValid("text/plain;") == "text/plain");
    Require(SerializeIfValid("text/plain;;") == "text/plain");
    Require(SerializeIfValid("text/plain;;;") == "text/plain");
    Require(SerializeIfValid("text/plain;test") == "text/plain");
    Require(SerializeIfValid("text/plain; test") == "text/plain");
    Require(SerializeIfValid("text/plain;\ttest") == "text/plain");
    Require(SerializeIfValid("text/plain;\ntest") == "text/plain");
    Require(SerializeIfValid("text/plain;\rtest") == "text/plain");
    Require(SerializeIfValid("text/plain;\btest") == "text/plain");
    Require(SerializeIfValid("text/plain;test ") == "text/plain");
    Require(SerializeIfValid("text/plain;test\t") == "text/plain");
    Require(SerializeIfValid("text/plain;test\n") == "text/plain");
    Require(SerializeIfValid("text/plain;test\r") == "text/plain");
    Require(SerializeIfValid("text/plain;test\b") == "text/plain");
    Require(SerializeIfValid("text/plain;test=") == "text/plain");
    Require(SerializeIfValid("text/plain;=;test=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value;=") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;wrong=;test=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;=wrong;test=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value;wrong=") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value;=wrong") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;;test=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;;;test=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value;") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value;;") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value;;;") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;TEST=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=VALUE") == "text/plain;test=VALUE");
    Require(SerializeIfValid("text/plain; test=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;\ttest=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;\ntest=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;\rtest=value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;\btest=value") == "text/plain");
    Require(SerializeIfValid("text/plain;\0test=value") == "text/plain");
    Require(SerializeIfValid("text/plain;test =value") == "text/plain");
    Require(SerializeIfValid("text/plain;test\t=value") == "text/plain");
    Require(SerializeIfValid("text/plain;test\n=value") == "text/plain");
    Require(SerializeIfValid("text/plain;test\r=value") == "text/plain");
    Require(SerializeIfValid("text/plain;test\b=value") == "text/plain");
    Require(SerializeIfValid("text/plain;test= value") == "text/plain;test=\" value\"");
    Require(SerializeIfValid("text/plain;test=\tvalue") == "text/plain;test=\"{9}value\"");
    Require(SerializeIfValid("text/plain;test=\nvalue") == "text/plain");
    Require(SerializeIfValid("text/plain;test=\rvalue") == "text/plain");
    Require(SerializeIfValid("text/plain;test=\bvalue") == "text/plain");
    Require(SerializeIfValid("text/plain;test=value ") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value\t") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value\n") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value\r") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=value\b") == "text/plain");
    Require(SerializeIfValid("text/plain;test=\"value\"") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=\"value") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=\"") == "text/plain;test=\"\"");
    Require(SerializeIfValid("text/plain;test=\"value\"foo") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=\"value\"foo;foo=bar") == "text/plain;test=value;foo=bar");
    Require(SerializeIfValid("text/plain;test=\"val\\ue\"") == "text/plain;test=value");
    Require(SerializeIfValid("text/plain;test=\"val\\\"ue\"") == "text/plain;test=\"val\\\"ue\"");
    Require(SerializeIfValid("text/plain;test='value'") == "text/plain;test='value'");
    Require(SerializeIfValid("text/plain;test='value") == "text/plain;test='value");
    Require(SerializeIfValid("text/plain;test=value'") == "text/plain;test=value'");
    Require(SerializeIfValid("text/plain;test={value}") == "text/plain;test=\"{value}\"");
}

void TestFieldsAndOwnership() {
    String input = Make("Text/HTML; Charset=UTF-8; boundary=\"a b\"");
    const unsigned references = input.impl->refCount;
    OptionalParsedContentType result = Create(input, Mode::MimeSniff);
    Require(result.engaged);
    const ParsedContentType& value = result.value;
    Require(value.m_contentType == input.impl);
    Require(input.impl->refCount == references + 2);
    Require(Equals(value.m_mimeType, "text/html"));
    Require(Is8Bit(value.m_mimeType));
    Require(value.m_parameterNames.size == 2);
    Require(value.m_parameterNames.capacity >= 2);
    Require(Equals(value.m_parameterNames.buffer[0], "charset"));
    Require(Equals(value.m_parameterNames.buffer[1], "boundary"));
    Require(value.m_parameterValues);
    Require(TableSize(value.m_parameterValues) == 8);
    Require(KeyCount(value.m_parameterValues) == 2);
    Require(Equals(Lookup(value, "charset"), "UTF-8"));
    Require(Equals(Lookup(value, "boundary"), "a b"));
    Destroy(result);
    Require(input.impl->refCount == references);
}

void TestStrippedContentType() {
    String input = Make("  text/plain\t");
    OptionalParsedContentType sniffed = Create(input, Mode::MimeSniff);
    Require(sniffed.engaged);
    Require(sniffed.value.m_contentType != input.impl);
    Require(Equals(sniffed.value.m_contentType, "text/plain"));
    Require(!sniffed.value.m_parameterValues);
    Require(!sniffed.value.m_parameterNames.buffer);
    Destroy(sniffed);
    OptionalParsedContentType rfc = Create(input, Mode::Rfc2045);
    Require(rfc.engaged);
    Require(rfc.value.m_contentType == input.impl);
    Require(Equals(rfc.value.m_mimeType, "text/plain"));
    Destroy(rfc);
}

void TestRfc2045() {
    String input = Make("Text/Plain;Charset=\"UTF-8\"");
    OptionalParsedContentType result = Create(input, Mode::Rfc2045);
    Require(result.engaged);
    Require(Equals(result.value.m_mimeType, "Text/Plain"));
    Require(Equals(result.value.m_parameterNames.buffer[0], "Charset"));
    Require(Equals(Lookup(result.value, "Charset"), "UTF-8\""));
    Destroy(result);
    Require(!IsValid("text/plain;test=\"value\";a=b", Mode::Rfc2045));
}

void TestTableGrowth() {
    std::string text = "a/b";
    for (int index = 0; index < 20; ++index) text += ";p" + std::to_string(index) + "=" + std::to_string(index);
    String input = Make(text.c_str());
    OptionalParsedContentType result = Create(input, Mode::MimeSniff);
    Require(result.engaged);
    Require(KeyCount(result.value.m_parameterValues) == 20);
    Require(TableSize(result.value.m_parameterValues) == 32);
    Require(result.value.m_parameterNames.size == 20);
    String serialized = _ZNK7WebCore17ParsedContentType9serializeEv(&result.value);
    Require(Equals(serialized.impl, text.c_str()));
    Destroy(result);
}

void TestSixteenBit() {
    String input = Make16(u"text/plain;name=\"\u0142\"");
    OptionalParsedContentType result = Create(input, Mode::MimeSniff);
    Require(result.engaged);
    Require(!Is8Bit(result.value.m_mimeType));
    Require(Equals(result.value.m_mimeType, "text/plain"));
    Require(result.value.m_parameterNames.size == 0);
    Destroy(result);
    String latin = Make16(u"text/plain;name=\u00e9");
    result = Create(latin, Mode::MimeSniff);
    Require(result.engaged);
    String serialized = _ZNK7WebCore17ParsedContentType9serializeEv(&result.value);
    Require(!Is8Bit(serialized.impl));
    Require(Units(serialized.impl) == u"text/plain;name=\"\u00e9\"");
    Destroy(result);
}

void TestDuplicateParameters() {
    Require(SerializeIfValid("a/b;x=1;x=2") == "a/b;x=1");
    Require(SerializeIfValid("a/b;x=1;X=2") == "a/b;x=2;x=2");
}

void TestNullArguments() {
    String input = Make("text/plain");
    OptionalParsedContentType result {};
    RequireThrows([&] { _ZN7WebCore17ParsedContentType6createERKN3WTF6StringENS_4ModeE(nullptr, &input, Mode::MimeSniff); });
    RequireThrows([&] { _ZN7WebCore17ParsedContentType6createERKN3WTF6StringENS_4ModeE(&result, nullptr, Mode::MimeSniff); });
    RequireThrows([&] { _ZN7WebCore17ParsedContentType6createERKN3WTF6StringENS_4ModeE(&result, &input, static_cast<Mode>(2)); });
    RequireThrows([] { _ZNK7WebCore17ParsedContentType9serializeEv(nullptr); });
    String empty;
    result = Create(empty, Mode::MimeSniff);
    Require(!result.engaged);
}

template<typename Function>
void RequireNotImplemented(Function function) {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    try {
        function();
    } catch (const std::invalid_argument&) {
        Require(false);
    } catch (const std::runtime_error&) {
        Require(NotImplementedCalls_nid_no_patch() == before + 1);
        return;
    }
    Require(false);
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }

    template<typename T>
    T* SplitReadWriteToReadOnly() const {
        return reinterpret_cast<T*>(base + pageSize - sizeof(T) / 2);
    }

    template<typename T>
    T* SplitReadOnlyToGuard() const {
        return reinterpret_cast<T*>(base + pageSize * 2 - sizeof(T) / 2);
    }
};

void TestGuestPointers() {
    GuardedPages pages;
    constexpr auto create = &_ZN7WebCore17ParsedContentType6createERKN3WTF6StringENS_4ModeE;
    constexpr auto serialize = &_ZNK7WebCore17ParsedContentType9serializeEv;

    String validInput = Make("text/plain;charset=utf-8");
    auto* unmappedResult = reinterpret_cast<OptionalParsedContentType*>(pages.Guard());
    auto* readOnlyResult = reinterpret_cast<OptionalParsedContentType*>(pages.ReadOnly());
    auto* splitResult = pages.SplitReadWriteToReadOnly<OptionalParsedContentType>();
    const auto* unmappedString = reinterpret_cast<const String*>(pages.Guard());
    const auto* splitString = pages.SplitReadOnlyToGuard<const String>();

    OptionalParsedContentType sentinel{};
    std::memset(&sentinel, 0x5A, sizeof(sentinel));
    RequireNotImplemented([&] { create(&sentinel, unmappedString, Mode::MimeSniff); });
    for (std::size_t i = 0; i < sizeof(sentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&sentinel)[i] == 0x5A);
    RequireNotImplemented([&] { create(&sentinel, splitString, Mode::MimeSniff); });
    for (std::size_t i = 0; i < sizeof(sentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&sentinel)[i] == 0x5A);

    RequireNotImplemented([&] { create(unmappedResult, &validInput, Mode::MimeSniff); });
    RequireNotImplemented([&] { create(readOnlyResult, &validInput, Mode::MimeSniff); });
    const std::size_t splitResultHead = sizeof(OptionalParsedContentType) / 2;
    std::memset(splitResult, 0x6B, splitResultHead);
    RequireNotImplemented([&] { create(splitResult, &validInput, Mode::MimeSniff); });
    for (std::size_t i = 0; i < splitResultHead; ++i) Require(reinterpret_cast<const unsigned char*>(splitResult)[i] == 0x6B);

    String badImpl;
    badImpl.impl = reinterpret_cast<StringImpl*>(pages.Guard());
    RequireNotImplemented([&] { create(&sentinel, &badImpl, Mode::MimeSniff); });
    badImpl.impl = nullptr;
    for (std::size_t i = 0; i < sizeof(sentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&sentinel)[i] == 0x5A);

    StringImpl badDataImpl{2, 16, pages.Guard(), 4};
    String badData;
    badData.impl = &badDataImpl;
    RequireNotImplemented([&] { create(&sentinel, &badData, Mode::MimeSniff); });
    badData.impl = nullptr;
    for (std::size_t i = 0; i < sizeof(sentinel); ++i) Require(reinterpret_cast<const unsigned char*>(&sentinel)[i] == 0x5A);

    String readOnlyImpl;
    readOnlyImpl.impl = reinterpret_cast<StringImpl*>(pages.ReadOnly());
    RequireNotImplemented([&] { create(&sentinel, &readOnlyImpl, Mode::Rfc2045); });
    readOnlyImpl.impl = nullptr;

    const auto* unmappedSelf = reinterpret_cast<const ParsedContentType*>(pages.Guard());
    const auto* splitSelf = pages.SplitReadOnlyToGuard<const ParsedContentType>();
    RequireNotImplemented([&] { serialize(unmappedSelf); });
    RequireNotImplemented([&] { serialize(splitSelf); });

    ParsedContentType badMime{};
    badMime.m_mimeType = reinterpret_cast<StringImpl*>(pages.Guard());
    RequireNotImplemented([&] { serialize(&badMime); });
    badMime.m_mimeType = &badDataImpl;
    RequireNotImplemented([&] { serialize(&badMime); });

    String mime = Make("text/plain");
    ParsedContentType badNames{};
    badNames.m_mimeType = mime.impl;
    badNames.m_parameterNames.buffer = reinterpret_cast<StringImpl**>(pages.Guard());
    badNames.m_parameterNames.capacity = 1;
    badNames.m_parameterNames.size = 1;
    RequireNotImplemented([&] { serialize(&badNames); });

    StringImpl* badNamePtr = reinterpret_cast<StringImpl*>(pages.Guard());
    badNames.m_parameterNames.buffer = &badNamePtr;
    RequireNotImplemented([&] { serialize(&badNames); });

    String paramName = Make("charset");
    StringImpl* validNamePtr = paramName.impl;
    ParsedContentType badTable{};
    badTable.m_mimeType = mime.impl;
    badTable.m_parameterNames.buffer = &validNamePtr;
    badTable.m_parameterNames.capacity = 1;
    badTable.m_parameterNames.size = 1;
    badTable.m_parameterValues = reinterpret_cast<ParsedContentTypeParameter*>(pages.Guard());
    RequireNotImplemented([&] { serialize(&badTable); });
    badTable.m_parameterValues = reinterpret_cast<ParsedContentTypeParameter*>(pages.ReadOnly());
    RequireNotImplemented([&] { serialize(&badTable); });
}

}

int main() {
    TestValidity();
    TestSerialize();
    TestFieldsAndOwnership();
    TestStrippedContentType();
    TestRfc2045();
    TestTableGrowth();
    TestSixteenBit();
    TestDuplicateParameters();
    TestNullArguments();
    TestGuestPointers();
    return 0;
}
