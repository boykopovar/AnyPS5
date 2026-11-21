// Derived from Source/JavaScriptCore/inspector/ContentSearchUtilities.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/StylesheetSourceMapExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

using NKWebKit::WtfAbi::CodeUnitAt;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;

constexpr std::string_view CommentOpen = "/*";
constexpr std::string_view CommentClose = "*/";
constexpr std::string_view SourceMapName = "sourceMappingURL=";

bool IsPatternSpace(char16_t value) {
    return value == ' ' || (value >= '\t' && value <= '\r') || value == 0x00A0 || value == 0x1680
        || (value >= 0x2000 && value <= 0x200A) || value == 0x2028 || value == 0x2029 || value == 0x202F
        || value == 0x205F || value == 0x3000 || value == 0xFEFF;
}

bool IsBlank(char16_t value) {
    return value == ' ' || value == '\t';
}

bool IsValueCharacter(char16_t value) {
    return !IsPatternSpace(value) && value != '\'' && value != '"';
}

bool HasAt(const StringView& view, unsigned position, std::string_view text) {
    if (position > view.length || view.length - position < text.size()) return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (CodeUnitAt(view, position + static_cast<unsigned>(i)) != static_cast<unsigned char>(text[i])) return false;
    }
    return true;
}

unsigned SkipBlanks(const StringView& view, unsigned position) {
    while (position < view.length && IsBlank(CodeUnitAt(view, position))) ++position;
    return position;
}

std::optional<std::pair<unsigned, unsigned>> MatchSourceMapComment(const StringView& view, unsigned start) {
    if (!HasAt(view, start, CommentOpen)) return std::nullopt;
    unsigned position = start + static_cast<unsigned>(CommentOpen.size());
    if (position >= view.length) return std::nullopt;
    char16_t marker = CodeUnitAt(view, position);
    if (marker != '#' && marker != '@') return std::nullopt;
    ++position;
    if (position >= view.length || !IsBlank(CodeUnitAt(view, position))) return std::nullopt;
    ++position;
    if (!HasAt(view, position, SourceMapName)) return std::nullopt;
    unsigned valueStart = SkipBlanks(view, position + static_cast<unsigned>(SourceMapName.size()));
    unsigned valueEnd = valueStart;
    while (valueEnd < view.length && IsValueCharacter(CodeUnitAt(view, valueEnd))) ++valueEnd;
    for (unsigned end = valueEnd;; --end) {
        if (HasAt(view, SkipBlanks(view, end), CommentClose)) return std::pair{valueStart, end};
        if (end == valueStart) return std::nullopt;
    }
}

}

extern "C" {

String APS5_VABI _ZN9Inspector22ContentSearchUtilities26findStylesheetSourceMapURLERKN3WTF6StringE(const String* content) {
    if (!content) throw std::invalid_argument("Inspector::ContentSearchUtilities::findStylesheetSourceMapURL: null WTF::String");
    const StringImpl* impl = content->impl;
    if (!impl || !impl->length) return String();
    StringView view = NKWebKit::WtfAbi::ViewOf(impl);
    for (unsigned start = 0; start < view.length; ++start) {
        if (auto match = MatchSourceMapComment(view, start)) {
            return _ZN3WTF10StringImpl9substringEjj(impl, match->first, match->second - match->first);
        }
    }
    return String();
}

}
