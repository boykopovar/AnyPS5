// Derived from Source/WTF/wtf/JSONValues.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/JsonExports.hpp"

#include <charconv>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>

namespace {

using NKWebKit::JsonAbi::Array;
using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::RefPtr;
using NKWebKit::JsonAbi::Value;
using NKWebKit::WtfAbi::String;

constexpr int StackLimit = 1000;

enum class Token {
    ObjectBegin,
    ObjectEnd,
    ArrayBegin,
    ArrayEnd,
    String,
    Number,
    BoolTrue,
    BoolFalse,
    Null,
    ListSeparator,
    ObjectPairSeparator,
    Invalid,
};

bool IsSpaceOrNewline(char16_t character) {
    if (character <= 0xFF) return character == ' ' || (character >= '\t' && character <= '\r');
    return character == 0x1680 || (character >= 0x2000 && character <= 0x200A) || character == 0x2028 || character == 0x205F || character == 0x3000;
}

bool IsASCIIHexDigit(char16_t character) {
    return (character >= '0' && character <= '9') || ((character | 0x20) >= 'a' && (character | 0x20) <= 'f');
}

unsigned ToASCIIHexValue(char16_t character) {
    return character < 'A' ? character - '0' : (character - 'A' + 10) & 0xF;
}

char16_t ToASCIIHexValue(char16_t upper, char16_t lower) {
    return static_cast<char16_t>((ToASCIIHexValue(upper) << 4 & 0xF0) | ToASCIIHexValue(lower));
}

bool ParseConstToken(const char16_t* start, const char16_t* end, const char16_t** tokenEnd, const char* token) {
    while (start < end && *token != '\0' && *start++ == static_cast<unsigned char>(*token++)) { }
    if (*token != '\0') return false;
    *tokenEnd = start;
    return true;
}

bool ReadInt(const char16_t* start, const char16_t* end, const char16_t** tokenEnd, bool canHaveLeadingZeros) {
    if (start == end) return false;
    bool haveLeadingZero = '0' == *start;
    int length = 0;
    while (start < end && '0' <= *start && *start <= '9') {
        ++start;
        ++length;
    }
    if (!length) return false;
    if (!canHaveLeadingZeros && length > 1 && haveLeadingZero) return false;
    *tokenEnd = start;
    return true;
}

bool ParseNumberToken(const char16_t* start, const char16_t* end, const char16_t** tokenEnd) {
    if (start == end) return false;
    char16_t c = *start;
    if ('-' == c) ++start;
    if (!ReadInt(start, end, &start, false)) return false;
    if (start == end) {
        *tokenEnd = start;
        return true;
    }
    c = *start;
    if ('.' == c) {
        ++start;
        if (!ReadInt(start, end, &start, true)) return false;
        if (start == end) {
            *tokenEnd = start;
            return true;
        }
        c = *start;
    }
    if ('e' == c || 'E' == c) {
        ++start;
        if (start == end) return false;
        c = *start;
        if ('-' == c || '+' == c) {
            ++start;
            if (start == end) return false;
        }
        if (!ReadInt(start, end, &start, true)) return false;
    }
    *tokenEnd = start;
    return true;
}

bool ReadHexDigits(const char16_t* start, const char16_t* end, const char16_t** tokenEnd, int digits) {
    if (end - start < digits) return false;
    for (int i = 0; i < digits; ++i) {
        if (!IsASCIIHexDigit(*start++)) return false;
    }
    *tokenEnd = start;
    return true;
}

bool ParseStringToken(const char16_t* start, const char16_t* end, const char16_t** tokenEnd) {
    while (start < end) {
        char16_t c = *start++;
        if ('\\' == c && start < end) {
            c = *start++;
            switch (c) {
            case 'x':
                if (!ReadHexDigits(start, end, &start, 2)) return false;
                break;
            case 'u':
                if (!ReadHexDigits(start, end, &start, 4)) return false;
                break;
            case '\\':
            case '/':
            case 'b':
            case 'f':
            case 'n':
            case 'r':
            case 't':
            case 'v':
            case '"':
                break;
            default:
                return false;
            }
        } else if ('"' == c) {
            *tokenEnd = start;
            return true;
        }
    }
    return false;
}

Token ParseToken(const char16_t* start, const char16_t* end, const char16_t** tokenStart, const char16_t** tokenEnd) {
    while (start < end && IsSpaceOrNewline(*start)) ++start;
    if (start == end) return Token::Invalid;
    *tokenStart = start;
    switch (*start) {
    case 'n':
        if (ParseConstToken(start, end, tokenEnd, "null")) return Token::Null;
        break;
    case 't':
        if (ParseConstToken(start, end, tokenEnd, "true")) return Token::BoolTrue;
        break;
    case 'f':
        if (ParseConstToken(start, end, tokenEnd, "false")) return Token::BoolFalse;
        break;
    case '[':
        *tokenEnd = start + 1;
        return Token::ArrayBegin;
    case ']':
        *tokenEnd = start + 1;
        return Token::ArrayEnd;
    case ',':
        *tokenEnd = start + 1;
        return Token::ListSeparator;
    case '{':
        *tokenEnd = start + 1;
        return Token::ObjectBegin;
    case '}':
        *tokenEnd = start + 1;
        return Token::ObjectEnd;
    case ':':
        *tokenEnd = start + 1;
        return Token::ObjectPairSeparator;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        if (ParseNumberToken(start, end, tokenEnd)) return Token::Number;
        break;
    case '"':
        if (ParseStringToken(start + 1, end, tokenEnd)) return Token::String;
        break;
    }
    return Token::Invalid;
}

bool DecodeString(const char16_t* start, const char16_t* end, std::u16string& output) {
    while (start < end) {
        char16_t c = *start++;
        if ('\\' != c) {
            output.push_back(c);
            continue;
        }
        if (start >= end) return false;
        c = *start++;
        switch (c) {
        case '"':
        case '/':
        case '\\':
            break;
        case 'b':
            c = '\b';
            break;
        case 'f':
            c = '\f';
            break;
        case 'n':
            c = '\n';
            break;
        case 'r':
            c = '\r';
            break;
        case 't':
            c = '\t';
            break;
        case 'v':
            c = '\v';
            break;
        case 'x':
            if (start + 1 >= end) return false;
            c = ToASCIIHexValue(start[0], start[1]);
            start += 2;
            break;
        case 'u':
            if (start + 3 >= end) return false;
            c = static_cast<char16_t>(ToASCIIHexValue(start[0], start[1]) << 8 | ToASCIIHexValue(start[2], start[3]));
            start += 4;
            break;
        default:
            return false;
        }
        output.push_back(c);
    }
    return true;
}

String MakeString(const std::u16string& characters) {
    bool is8Bit = true;
    for (char16_t character : characters) {
        if (character > 0xFF) {
            is8Bit = false;
            break;
        }
    }
    void* buffer = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(characters.size()), is8Bit, buffer));
    if (is8Bit) {
        auto* output = static_cast<unsigned char*>(buffer);
        for (std::size_t i = 0; i < characters.size(); ++i) output[i] = static_cast<unsigned char>(characters[i]);
    } else {
        std::memcpy(buffer, characters.data(), characters.size() * sizeof(char16_t));
    }
    return result;
}

bool DecodeString(const char16_t* start, const char16_t* end, String& output) {
    if (start == end) {
        output = String(NKWebKit::WtfAbi::EmptyStringImpl());
        return true;
    }
    if (start > end) return false;
    std::u16string buffer;
    buffer.reserve(static_cast<std::size_t>(end - start));
    if (!DecodeString(start, end, buffer)) return false;
    output = MakeString(buffer);
    return true;
}

double CharactersToDouble(const char16_t* start, const char16_t* end) {
    std::string ascii(start, end);
    double value = 0;
    auto parsed = std::from_chars(ascii.data(), ascii.data() + ascii.size(), value);
    if (parsed.ec == std::errc::result_out_of_range) return std::strtod(ascii.c_str(), nullptr);
    return value;
}

RefPtr<Value> BuildValue(const char16_t* start, const char16_t* end, const char16_t** valueTokenEnd, int depth) {
    if (depth > StackLimit) return RefPtr<Value>();
    RefPtr<Value> result;
    const char16_t* tokenStart = nullptr;
    const char16_t* tokenEnd = nullptr;
    Token token = ParseToken(start, end, &tokenStart, &tokenEnd);
    switch (token) {
    case Token::Invalid:
        return RefPtr<Value>();
    case Token::Null:
        result = RefPtr<Value>::Adopt(_ZN3WTF8JSONImpl5Value4nullEv().Leak());
        break;
    case Token::BoolTrue:
        result = RefPtr<Value>::Adopt(_ZN3WTF8JSONImpl5Value6createEb(true).Leak());
        break;
    case Token::BoolFalse:
        result = RefPtr<Value>::Adopt(_ZN3WTF8JSONImpl5Value6createEb(false).Leak());
        break;
    case Token::Number:
        result = RefPtr<Value>::Adopt(_ZN3WTF8JSONImpl5Value6createEd(CharactersToDouble(tokenStart, tokenEnd)).Leak());
        break;
    case Token::String: {
        String value;
        if (!DecodeString(tokenStart + 1, tokenEnd - 1, value)) return RefPtr<Value>();
        result = RefPtr<Value>::Adopt(_ZN3WTF8JSONImpl5Value6createERKNS_6StringE(value).Leak());
        break;
    }
    case Token::ArrayBegin: {
        Ref<Array> array = _ZN3WTF8JSONImpl5Array6createEv();
        start = tokenEnd;
        token = ParseToken(start, end, &tokenStart, &tokenEnd);
        while (token != Token::ArrayEnd) {
            RefPtr<Value> arrayNode = BuildValue(start, end, &tokenEnd, depth + 1);
            if (!arrayNode) return RefPtr<Value>();
            NKWebKit::JsonAbi::PushValue(*array, Ref<Value>::Adopt(arrayNode.Leak()));
            start = tokenEnd;
            token = ParseToken(start, end, &tokenStart, &tokenEnd);
            if (token == Token::ListSeparator) {
                start = tokenEnd;
                token = ParseToken(start, end, &tokenStart, &tokenEnd);
                if (token == Token::ArrayEnd) return RefPtr<Value>();
            } else if (token != Token::ArrayEnd) {
                return RefPtr<Value>();
            }
        }
        result = RefPtr<Value>::Adopt(array.Leak());
        break;
    }
    case Token::ObjectBegin: {
        Ref<Object> object = _ZN3WTF8JSONImpl6Object6createEv();
        start = tokenEnd;
        token = ParseToken(start, end, &tokenStart, &tokenEnd);
        while (token != Token::ObjectEnd) {
            if (token != Token::String) return RefPtr<Value>();
            String key;
            if (!DecodeString(tokenStart + 1, tokenEnd - 1, key)) return RefPtr<Value>();
            start = tokenEnd;
            token = ParseToken(start, end, &tokenStart, &tokenEnd);
            if (token != Token::ObjectPairSeparator) return RefPtr<Value>();
            start = tokenEnd;
            RefPtr<Value> value = BuildValue(start, end, &tokenEnd, depth + 1);
            if (!value) return RefPtr<Value>();
            NKWebKit::JsonAbi::SetValue(*object, key, Ref<Value>::Adopt(value.Leak()));
            start = tokenEnd;
            token = ParseToken(start, end, &tokenStart, &tokenEnd);
            if (token == Token::ListSeparator) {
                start = tokenEnd;
                token = ParseToken(start, end, &tokenStart, &tokenEnd);
                if (token == Token::ObjectEnd) return RefPtr<Value>();
            } else if (token != Token::ObjectEnd) {
                return RefPtr<Value>();
            }
        }
        result = RefPtr<Value>::Adopt(object.Leak());
        break;
    }
    default:
        return RefPtr<Value>();
    }
    *valueTokenEnd = tokenEnd;
    return result;
}

}  // namespace

namespace NKWebKit::JsonAbi {

RefPtr<Value> ParseJSON(const String& json) {
    std::u16string characters;
    if (json.impl) {
        const auto view = NKWebKit::WtfAbi::ViewOf(json.impl);
        characters.resize(json.impl->length);
        for (unsigned i = 0U; i < json.impl->length; ++i) characters[i] = NKWebKit::WtfAbi::CodeUnitAt(view, i);
    }
    const char16_t* start = characters.data();
    const char16_t* end = start + characters.size();
    const char16_t* tokenEnd = nullptr;
    RefPtr<Value> result = BuildValue(start, end, &tokenEnd, 0);
    if (!result) return RefPtr<Value>();
    for (const char16_t* valueEnd = tokenEnd; valueEnd < end; ++valueEnd) {
        if (!IsSpaceOrNewline(*valueEnd)) return RefPtr<Value>();
    }
    return result;
}

}  // namespace NKWebKit::JsonAbi
