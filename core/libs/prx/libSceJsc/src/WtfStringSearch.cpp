/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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
 * Derived from Source/WTF/wtf/text/StringImpl.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
 * Derived from Source/WTF/wtf/text/StringCommon.h@c55364d53f971f63a709976f168d932d558b8909
 */
/*

Copyright (C) 2014-2019 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Derived from Source/WTF/wtf/text/StringView.cpp@c55364d53f971f63a709976f168d932d558b8909
*/
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
 * Derived from Source/WTF/wtf/URL.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/WtfStringSearchExports.hpp"
#include "prx/libSceIcu/Properties.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

using namespace Jsc::WtfAbi;

constexpr std::size_t NotFound = static_cast<std::size_t>(-1);

struct Text {
    const void* data;
    unsigned length;
    bool is8Bit;

    char16_t operator[](unsigned index) const {
        return is8Bit ? static_cast<const std::uint8_t*>(data)[index] : static_cast<const char16_t*>(data)[index];
    }
};

Text Require(const StringImpl* impl, const char* function) {
    if (!impl) throw std::invalid_argument(function);
    return {impl->data, impl->length, (impl->hashAndFlags & StringImplFlag8Bit) != 0};
}

Text Require(const StringView* view, const char* function) {
    if (!view) throw std::invalid_argument(function);
    return {view->characters, view->length, view->is8Bit};
}

template<typename Visitor> auto Visit(const Text& first, const Text& second, Visitor visitor) {
    if (first.is8Bit) {
        const auto* a = static_cast<const std::uint8_t*>(first.data);
        if (second.is8Bit) return visitor(a, static_cast<const std::uint8_t*>(second.data));
        return visitor(a, static_cast<const char16_t*>(second.data));
    }
    const auto* a = static_cast<const char16_t*>(first.data);
    if (second.is8Bit) return visitor(a, static_cast<const std::uint8_t*>(second.data));
    return visitor(a, static_cast<const char16_t*>(second.data));
}

template<typename Character> unsigned ToASCIILower(Character character) {
    const unsigned value = character;
    return value | ((value >= 'A' && value <= 'Z') ? 0x20u : 0u);
}

template<typename A, typename B> bool EqualUnits(const A* a, const B* b, unsigned length) {
    for (unsigned i = 0; i < length; ++i) {
        if (static_cast<unsigned>(a[i]) != static_cast<unsigned>(b[i])) return false;
    }
    return true;
}

template<typename A, typename B> bool EqualUnitsIgnoringASCIICase(const A* a, const B* b, unsigned length) {
    for (unsigned i = 0; i < length; ++i) {
        if (ToASCIILower(a[i]) != ToASCIILower(b[i])) return false;
    }
    return true;
}

bool EqualAt(const Text& text, unsigned offset, const Text& match, bool ignoreASCIICase) {
    return Visit(text, match, [&](const auto* a, const auto* b) {
        return ignoreASCIICase ? EqualUnitsIgnoringASCIICase(a + offset, b, match.length) : EqualUnits(a + offset, b, match.length);
    });
}

bool StartsWith(const Text& reference, const Text& prefix, bool ignoreASCIICase) {
    return prefix.length <= reference.length && EqualAt(reference, 0, prefix, ignoreASCIICase);
}

bool EndsWith(const Text& reference, const Text& suffix, bool ignoreASCIICase) {
    return suffix.length <= reference.length && EqualAt(reference, reference.length - suffix.length, suffix, ignoreASCIICase);
}

bool HasInfixAt(const Text& text, unsigned startOffset, const Text& match) {
    if (startOffset > text.length || match.length > text.length || match.length + startOffset > text.length) return false;
    return EqualAt(text, startOffset, match, false);
}

std::size_t FindCharacter(const Text& text, char16_t character, unsigned index) {
    if (text.is8Bit && character > 0xFF) return NotFound;
    for (; index < text.length; ++index) {
        if (text[index] == character) return index;
    }
    return NotFound;
}

std::size_t ReverseFindCharacter(const Text& text, char16_t character, unsigned index) {
    if (text.is8Bit && character > 0xFF) return NotFound;
    if (!text.length) return NotFound;
    if (index >= text.length) index = text.length - 1;
    while (text[index] != character) {
        if (!index--) return NotFound;
    }
    return index;
}

std::size_t FindCommon(const Text& haystack, const Text& needle, unsigned start) {
    if (needle.length == 1) return FindCharacter(haystack, needle[0], start);
    if (start > haystack.length) return NotFound;
    if (!needle.length) return start;
    const unsigned searchLength = haystack.length - start;
    if (needle.length > searchLength) return NotFound;
    const unsigned delta = searchLength - needle.length;
    for (unsigned i = 0; i <= delta; ++i) {
        if (EqualAt(haystack, start + i, needle, false)) return start + i;
    }
    return NotFound;
}

std::size_t FindIgnoringASCIICase(const Text& source, const Text& match, unsigned startOffset) {
    if (!match.length) return std::min(startOffset, source.length);
    if (startOffset > source.length) return NotFound;
    const unsigned searchLength = source.length - startOffset;
    if (match.length > searchLength) return NotFound;
    const unsigned delta = searchLength - match.length;
    for (unsigned i = 0; i <= delta; ++i) {
        if (EqualAt(source, startOffset + i, match, true)) return startOffset + i;
    }
    return NotFound;
}

std::size_t ReverseFind(const Text& text, const Text& match, unsigned index) {
    if (!match.length) return std::min(index, text.length);
    if (match.length == 1) return ReverseFindCharacter(text, match[0], index);
    if (match.length > text.length) return NotFound;
    for (unsigned delta = std::min(index, text.length - match.length);; --delta) {
        if (EqualAt(text, delta, match, false)) return delta;
        if (!delta) return NotFound;
    }
}

Text CString(const char* string, const char* function) {
    if (!string) throw std::invalid_argument(function);
    return {string, static_cast<unsigned>(std::strlen(string)), true};
}

bool IsASCIIAlphaCaselessEqual(char16_t codeUnit, char expected) {
    return static_cast<int>(codeUnit | 0x20) == static_cast<int>(expected);
}

} // namespace

extern "C" {

bool APS5_VABI _ZNK3WTF10StringImpl10startsWithEPKS0_(const StringImpl* self, const StringImpl* prefix) {
    const Text text = Require(self, __func__);
    return !prefix || StartsWith(text, Require(prefix, __func__), false);
}

bool APS5_VABI _ZNK3WTF10StringImpl10startsWithERKS0_(const StringImpl* self, const StringImpl* prefix) {
    return StartsWith(Require(self, __func__), Require(prefix, __func__), false);
}

bool APS5_VABI _ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_(const StringImpl* self, const StringImpl* prefix) {
    const Text text = Require(self, __func__);
    return prefix && StartsWith(text, Require(prefix, __func__), true);
}

bool APS5_VABI _ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(const StringImpl* self, const StringImpl* prefix) {
    return StartsWith(Require(self, __func__), Require(prefix, __func__), true);
}

bool APS5_VABI _ZN3WTF10StringImpl8endsWithEPS0_(StringImpl* self, StringImpl* suffix) {
    const Text text = Require(self, __func__);
    return suffix && EndsWith(text, Require(suffix, __func__), false);
}

bool APS5_VABI _ZN3WTF10StringImpl8endsWithERS0_(StringImpl* self, StringImpl* suffix) {
    return EndsWith(Require(self, __func__), Require(suffix, __func__), false);
}

bool APS5_VABI _ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseEPKS0_(const StringImpl* self, const StringImpl* suffix) {
    const Text text = Require(self, __func__);
    return suffix && EndsWith(text, Require(suffix, __func__), true);
}

bool APS5_VABI _ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_(const StringImpl* self, const StringImpl* suffix) {
    return EndsWith(Require(self, __func__), Require(suffix, __func__), true);
}

bool APS5_VABI _ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(const StringImpl* self, const StringImpl* match, unsigned startOffset) {
    return HasInfixAt(Require(self, __func__), startOffset, Require(match, __func__));
}

bool APS5_VABI _ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(const StringImpl* self, const StringImpl* match, unsigned endOffset) {
    const Text text = Require(self, __func__);
    const Text infix = Require(match, __func__);
    return endOffset >= infix.length && HasInfixAt(text, endOffset - infix.length, infix);
}

std::size_t APS5_VABI _ZN3WTF10StringImpl4findEPFbDsEj(StringImpl* self, CodeUnitMatchFunction matchFunction, unsigned start) {
    const Text text = Require(self, __func__);
    if (!matchFunction) throw std::invalid_argument(__func__);
    for (unsigned index = start; index < text.length; ++index) {
        if (matchFunction(text[index])) return index;
    }
    return NotFound;
}

std::size_t APS5_VABI _ZN3WTF10StringImpl4findEPS0_(StringImpl* self, StringImpl* match) {
    const Text text = Require(self, __func__);
    return match ? FindCommon(text, Require(match, __func__), 0) : NotFound;
}

std::size_t APS5_VABI _ZN3WTF10StringImpl4findEPS0_j(StringImpl* self, StringImpl* match, unsigned index) {
    const Text text = Require(self, __func__);
    return match ? FindCommon(text, Require(match, __func__), index) : NotFound;
}

std::size_t APS5_VABI _ZN3WTF10StringImpl11reverseFindEPS0_j(StringImpl* self, StringImpl* match, unsigned index) {
    const Text text = Require(self, __func__);
    return match ? ReverseFind(text, Require(match, __func__), index) : NotFound;
}

std::size_t APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_(const StringImpl* self, const StringImpl* match) {
    const Text text = Require(self, __func__);
    return match ? FindIgnoringASCIICase(text, Require(match, __func__), 0) : NotFound;
}

std::size_t APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j(const StringImpl* self, const StringImpl* match, unsigned startOffset) {
    const Text text = Require(self, __func__);
    return match ? FindIgnoringASCIICase(text, Require(match, __func__), startOffset) : NotFound;
}

std::size_t APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(const StringImpl* self, const StringImpl* match) {
    return FindIgnoringASCIICase(Require(self, __func__), Require(match, __func__), 0);
}

std::size_t APS5_VABI _ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(const StringImpl* self, const StringImpl* match, unsigned startOffset) {
    return FindIgnoringASCIICase(Require(self, __func__), Require(match, __func__), startOffset);
}

bool APS5_VABI _ZNK3WTF10StringView8containsEPKc(const StringView* self, const char* string) {
    const Text text = Require(self, __func__);
    return FindCommon(text, CString(string, __func__), 0) != NotFound;
}

bool APS5_VABI _ZN3WTF10protocolIsENS_10StringViewEPKc(StringView string, const char* protocol) {
    const Text text = Require(&string, __func__);
    bool isLeading = true;
    for (unsigned index = 0; index < text.length; ++index) {
        const char16_t codeUnit = text[index];
        if (isLeading) {
            if (codeUnit <= ' ') continue;
            isLeading = false;
        } else if (codeUnit == '\t' || codeUnit == '\r' || codeUnit == '\n') continue;
        if (!protocol) throw std::invalid_argument(__func__);
        const char expected = *protocol++;
        if (!expected) return codeUnit == ':';
        if (!IsASCIIAlphaCaselessEqual(codeUnit, expected)) return false;
    }
    return false;
}

std::int32_t APS5_VABI _ZN3WTF10StringImpl23defaultWritingDirectionEPb(StringImpl* self, bool* hasStrongDirectionality) {
    constexpr std::int32_t LeftToRight = 0;
    constexpr std::int32_t RightToLeft = 1;
    constexpr std::int32_t RightToLeftArabic = 13;
    const Text text = Require(self, __func__);
    for (unsigned index = 0; index < text.length; ++index) {
        const std::int32_t direction = u_charDirection_nid_postfix(text[index]);
        if (direction == LeftToRight || direction == RightToLeft || direction == RightToLeftArabic) {
            if (hasStrongDirectionality) *hasStrongDirectionality = true;
            return direction == LeftToRight ? LeftToRight : RightToLeft;
        }
    }
    if (hasStrongDirectionality) *hasStrongDirectionality = false;
    return LeftToRight;
}

}
