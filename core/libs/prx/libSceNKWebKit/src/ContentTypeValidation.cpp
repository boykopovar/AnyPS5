 /*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WebCoreParsersExports.hpp"

#include <stdexcept>
#include <string_view>

namespace {

using WebCore::Mode;

bool IsHTTPSpace(char16_t c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool IsTokenCharacter(char16_t c) {
    return c <= 0x7f && c > ' ' && c != '"' && c != '(' && c != ')' && c != ',' && c != '/'
        && (c < ':' || c > '@') && (c < '[' || c > ']');
}

bool IsHTTPTokenCharacter(char16_t c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c && std::u16string_view(u"!#$%&'*+-.^_`|~").find(c) != std::u16string_view::npos);
}

template<typename Character> bool Parse(std::basic_string_view<Character> input, Mode mode) {
    bool rfc = mode == Mode::Rfc2045;
    if (!rfc) {
        while (!input.empty() && IsHTTPSpace(input.front())) input.remove_prefix(1);
        while (!input.empty() && IsHTTPSpace(input.back())) input.remove_suffix(1);
    } else {
        for (auto c : input) if (c == '\r' || c == '\n') return false;
    }
    std::size_t index = 0;
    auto skipSpaces = [&] {
        while (index < input.size() && IsHTTPSpace(input[index])) ++index;
    };
    auto token = [&](auto condition, bool trailingWhitespace = false) {
        auto start = index;
        if (index >= input.size()) return false;
        while (index < input.size() && condition(input[index])) {
            if (rfc && !IsTokenCharacter(input[index])) break;
            ++index;
        }
        if (index == start) return false;
        if (trailingWhitespace) {
            while (index && (rfc ? input[index - 1] == ' ' : IsHTTPSpace(input[index - 1]))) --index;
        }
        return true;
    };
    auto validRange = [&](std::size_t start) {
        if (mode == Mode::MimeSniff && start == index) return false;
        for (; start < index; ++start) {
            if (mode == Mode::MimeSniff ? !IsHTTPTokenCharacter(input[start]) : !IsTokenCharacter(input[start])) return false;
        }
        return true;
    };
    skipSpaces();
    if (index >= input.size()) return false;
    auto contentStart = index;
    auto start = index;
    if (!token([](auto c) { return c != '/'; }) || !validRange(start)) return false;
    if (index >= input.size() || input[index++] != '/') return false;
    start = index;
    if (!token([](auto c) { return c != ';'; }, mode == Mode::MimeSniff) || !validRange(start)) return false;
    auto semicolon = input.find(';', contentStart);
    if (semicolon == input.npos) return true;
    index = semicolon + 1;
    while (true) {
        skipSpaces();
        bool key = token([](auto c) { return c != ';' && c != '='; });
        if (rfc && (!key || index >= input.size())) return false;
        if (rfc) {
            if (index >= input.size() || input[index++] != '=') return false;
        } else {
            if (index >= input.size()) break;
            if (input[index] != '=' && input[index] != ';') return false;
            if (input[index++] == ';') continue;
        }
        bool value = false;
        if (index < input.size() && input[index] == '"') {
            if (mode == Mode::MimeSniff) {
                ++index;
                while (index < input.size()) {
                    auto c = input[index++];
                    if (c == '"') break;
                    if (c == '\\' && index < input.size()) ++index;
                }
                value = true;
                token([](auto c) { return c != ';'; });
            } else {
                ++index;
                if (index < input.size()) {
                    bool backslash = false;
                    while (true) {
                        char c = static_cast<char>(input[index++]);
                        if (c == '"' && !backslash) { value = true; break; }
                        if (index >= input.size()) break;
                        if (c == '\\' && !backslash) { backslash = true; continue; }
                        if (backslash) backslash = false;
                    }
                    if (value && input[index - 1] == '"') ++index;
                }
            }
        } else value = token([](auto c) { return c != ';'; }, mode == Mode::MimeSniff);
        if (!value) {
            if (mode == Mode::MimeSniff) continue;
            return false;
        }
        if (rfc && index < input.size() && input[index++] != ';') return false;
        if (index >= input.size()) return true;
    }
    return true;
}

}  // namespace

extern "C" bool APS5_VABI _ZN7WebCore18isValidContentTypeERKN3WTF6StringENS_4ModeE(
    const NKWebKit::WtfAbi::String& input, Mode mode) {
    if (!input.impl || !input.impl->length) return false;
    if (!input.impl->data) throw std::invalid_argument("null WTF::StringImpl characters");
    if (input.impl->hashAndFlags & 4)
        return Parse(std::u8string_view(static_cast<const char8_t*>(input.impl->data), input.impl->length), mode);
    return Parse(std::u16string_view(static_cast<const char16_t*>(input.impl->data), input.impl->length), mode);
}
