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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <type_traits>
#include <vector>

namespace {

using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringImplFlag8Bit;
using Jsc::YarrAbi::RegularExpression;
using Jsc::YarrAbi::RegularExpressionPrivate;
using Jsc::YarrAbi::StringReference;

static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(std::is_trivially_copyable_v<StringReference>);
static_assert(std::is_trivially_copyable_v<RegularExpression>);
static_assert(std::is_trivially_copyable_v<RegularExpressionPrivate>);

const char regexSpecialCharacters[] = "[](){}+-*.,?\\^$|";

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, size);
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, size);
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

struct HostStringSnapshot {
    bool isNull = true;
    bool is8Bit = true;
    std::vector<std::uint8_t> narrow;
    std::vector<char16_t> wide;
    StringImpl impl{};

    StringReference Reference() {
        if (isNull) return StringReference{nullptr};
        if (is8Bit) {
            impl = StringImpl{2, static_cast<unsigned>(narrow.size()), narrow.empty() ? nullptr : narrow.data(), StringImplFlag8Bit};
        } else {
            impl = StringImpl{2, static_cast<unsigned>(wide.size()), wide.empty() ? nullptr : wide.data(), 0};
        }
        return StringReference{&impl};
    }

    unsigned Length() const {
        return is8Bit ? static_cast<unsigned>(narrow.size()) : static_cast<unsigned>(wide.size());
    }
};

HostStringSnapshot CopyGuestString(const StringReference* reference, const char* function, const char* nullDataMessage) {
    const StringReference ref = ReadGuestValue(reference, function);
    HostStringSnapshot snapshot{};
    if (!ref.impl) return snapshot;
    const StringImpl header = ReadGuestValue(ref.impl, function);
    snapshot.isNull = false;
    snapshot.is8Bit = (header.hashAndFlags & StringImplFlag8Bit) != 0;
    if (!header.length) return snapshot;
    if (!header.data) throw std::invalid_argument(nullDataMessage);
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * (snapshot.is8Bit ? 1u : sizeof(char16_t));
    RequireReadableGuestBytes(header.data, byteLength, function);
    if (snapshot.is8Bit) {
        snapshot.narrow.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(snapshot.narrow.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    } else {
        snapshot.wide.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(snapshot.wide.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    return snapshot;
}

void AppendEscaped(std::u16string& result, const HostStringSnapshot& text) {
    if (text.isNull) return;
    const unsigned length = text.Length();
    for (unsigned i = 0; i < length; ++i) {
        const char16_t character = text.is8Bit ? static_cast<char16_t>(text.narrow[i]) : text.wide[i];
        if (character < 0x80 && std::strchr(regexSpecialCharacters, static_cast<char>(character))) result.push_back(u'\\');
        result.push_back(character);
    }
}

struct RegularExpressionGuard {
    RegularExpression regex{};

    ~RegularExpressionGuard() {
        if (!regex.d) return;
        try {
            JscRegularExpressionRelease_nid_no_patch(regex.d);
        } catch (...) {
        }
    }
};

}

extern "C" {

RegularExpression* APS5_VABI _ZN9Inspector22ContentSearchUtilities38createRegularExpressionForSearchStringERKN3WTF6StringEbNS0_16SearchStringTypeE(RegularExpression* result, const StringReference* searchString, bool caseSensitive, int type) {
    if (!result) throw std::invalid_argument("Inspector::ContentSearchUtilities::createRegularExpressionForSearchString: null result");
    if (!searchString) throw std::invalid_argument("Inspector::ContentSearchUtilities::createRegularExpressionForSearchString: null search string");
    RequireWritableGuestBytes(result, sizeof(RegularExpression), __func__);
    HostStringSnapshot source = CopyGuestString(
        searchString,
        __func__,
        "Inspector::ContentSearchUtilities::createRegularExpressionForSearchString: search string with null characters and a nonzero length");
    const int caseSensitivity = caseSensitive ? 0 : Jsc::YarrAbi::TextCaseInsensitive;
    std::u16string pattern;
    RegularExpressionGuard constructed{};
    switch (type) {
    case NKWebKit::InspectorAbi::SearchStringTypeRegex: {
        const StringReference hostRef = source.Reference();
        JscRegularExpressionConstruct_nid_no_patch(&constructed.regex, &hostRef, caseSensitivity, 0, 0);
        break;
    }
    case NKWebKit::InspectorAbi::SearchStringTypeExactString:
        pattern.push_back(u'^');
        AppendEscaped(pattern, source);
        pattern.push_back(u'$');
        {
            StringImpl impl{2, static_cast<unsigned>(pattern.size()), pattern.data(), 0};
            const StringReference reference{&impl};
            JscRegularExpressionConstruct_nid_no_patch(&constructed.regex, &reference, caseSensitivity, 0, 0);
        }
        break;
    case NKWebKit::InspectorAbi::SearchStringTypeContainsString:
        AppendEscaped(pattern, source);
        {
            StringImpl impl{2, static_cast<unsigned>(pattern.size()), pattern.data(), 0};
            const StringReference reference{&impl};
            JscRegularExpressionConstruct_nid_no_patch(&constructed.regex, &reference, caseSensitivity, 0, 0);
        }
        break;
    default:
        throw std::invalid_argument("Inspector::ContentSearchUtilities::createRegularExpressionForSearchString: unknown SearchStringType " + std::to_string(type));
    }
    if (!GuestCopyOut_nid_no_patch(result, &constructed.regex, sizeof(constructed.regex)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    constructed.regex.d = nullptr;
    return result;
}

int APS5_VABI _ZN9Inspector22ContentSearchUtilities29countRegularExpressionMatchesERKN3JSC4Yarr17RegularExpressionERKN3WTF6StringE(const RegularExpression* regex, const StringReference* content) {
    if (!regex) throw std::invalid_argument("Inspector::ContentSearchUtilities::countRegularExpressionMatches: null regular expression");
    if (!content) throw std::invalid_argument("Inspector::ContentSearchUtilities::countRegularExpressionMatches: null content");
    const RegularExpression localRegex = ReadGuestValue(regex, __func__);
    HostStringSnapshot hostContent = CopyGuestString(
        content,
        __func__,
        "Inspector::ContentSearchUtilities::countRegularExpressionMatches: content string with null characters and a nonzero length");
    const unsigned length = hostContent.Length();
    if (hostContent.isNull || !length) return 0;
    if (localRegex.d) {
        RequireReadableGuestBytes(localRegex.d, sizeof(RegularExpressionPrivate), __func__);
        RequireWritableGuestBytes(localRegex.d, sizeof(RegularExpressionPrivate), __func__);
    }
    const StringReference hostContentRef = hostContent.Reference();
    int result = 0;
    int position;
    unsigned start = 0;
    int matchLength;
    while ((position = JscRegularExpressionMatch_nid_no_patch(&localRegex, &hostContentRef, static_cast<int>(start), &matchLength)) != -1) {
        if (start >= length) break;
        if (matchLength > 0) ++result;
        start = position + 1;
    }
    return result;
}

}
