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

#include "prx/libSceNKWebKit/include/InspectorContentSearchRegexExports.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringImplFlag8Bit;
using Jsc::YarrAbi::RegularExpression;
using Jsc::YarrAbi::StringReference;

const char regexSpecialCharacters[] = "[](){}+-*.,?\\^$|";

char16_t CodeUnitAt(const StringImpl& impl, unsigned index) {
    if (impl.hashAndFlags & StringImplFlag8Bit) return static_cast<const std::uint8_t*>(impl.data)[index];
    return static_cast<const char16_t*>(impl.data)[index];
}

void AppendEscaped(std::u16string& result, const StringImpl* text) {
    if (!text) return;
    if (text->length && !text->data) throw std::invalid_argument("Inspector::ContentSearchUtilities::createRegularExpressionForSearchString: search string with null characters and a nonzero length");
    for (unsigned i = 0; i < text->length; ++i) {
        const char16_t character = CodeUnitAt(*text, i);
        if (character < 0x80 && std::strchr(regexSpecialCharacters, static_cast<char>(character))) result.push_back(u'\\');
        result.push_back(character);
    }
}

}

extern "C" {

RegularExpression* APS5_VABI _ZN9Inspector22ContentSearchUtilities38createRegularExpressionForSearchStringERKN3WTF6StringEbNS0_16SearchStringTypeE(RegularExpression* result, const StringReference* searchString, bool caseSensitive, int type) {
    if (!result) throw std::invalid_argument("Inspector::ContentSearchUtilities::createRegularExpressionForSearchString: null result");
    if (!searchString) throw std::invalid_argument("Inspector::ContentSearchUtilities::createRegularExpressionForSearchString: null search string");
    const int caseSensitivity = caseSensitive ? 0 : Jsc::YarrAbi::TextCaseInsensitive;
    std::u16string pattern;
    switch (type) {
    case NKWebKit::InspectorAbi::SearchStringTypeRegex:
        JscRegularExpressionConstruct_nid_no_patch(result, searchString, caseSensitivity, 0, 0);
        return result;
    case NKWebKit::InspectorAbi::SearchStringTypeExactString:
        pattern.push_back(u'^');
        AppendEscaped(pattern, searchString->impl);
        pattern.push_back(u'$');
        break;
    case NKWebKit::InspectorAbi::SearchStringTypeContainsString:
        AppendEscaped(pattern, searchString->impl);
        break;
    default:
        throw std::invalid_argument("Inspector::ContentSearchUtilities::createRegularExpressionForSearchString: unknown SearchStringType " + std::to_string(type));
    }
    StringImpl impl{2, static_cast<unsigned>(pattern.size()), pattern.data(), 0};
    const StringReference reference{&impl};
    JscRegularExpressionConstruct_nid_no_patch(result, &reference, caseSensitivity, 0, 0);
    return result;
}

int APS5_VABI _ZN9Inspector22ContentSearchUtilities29countRegularExpressionMatchesERKN3JSC4Yarr17RegularExpressionERKN3WTF6StringE(const RegularExpression* regex, const StringReference* content) {
    if (!regex) throw std::invalid_argument("Inspector::ContentSearchUtilities::countRegularExpressionMatches: null regular expression");
    if (!content) throw std::invalid_argument("Inspector::ContentSearchUtilities::countRegularExpressionMatches: null content");
    if (!content->impl || !content->impl->length) return 0;
    const unsigned length = content->impl->length;
    int result = 0;
    int position;
    unsigned start = 0;
    int matchLength;
    while ((position = JscRegularExpressionMatch_nid_no_patch(regex, content, static_cast<int>(start), &matchLength)) != -1) {
        if (start >= length) break;
        if (matchLength > 0) ++result;
        start = position + 1;
    }
    return result;
}

}
