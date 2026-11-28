// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/URLHelpers.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Igalia S.L.
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

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/ASCIICType.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceIcu/CodePoints.hpp"
#include "prx/libSceIcu/Properties.hpp"
#include "prx/libSceIcu/Script.hpp"
#include "prx/libSceIcu/Space.hpp"
#include "unicode/uchar.h"
#include "unicode/uscript.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;

template<typename CharacterType> bool IsASCIIHexDigit(CharacterType character) {
    const auto lower = character | 0x20;
    return (character >= '0' && character <= '9') || (lower >= 'a' && lower <= 'f');
}

template<typename CharacterType> std::uint8_t ToASCIIHexValue(CharacterType character) {
    return character < 'A' ? character - '0' : (character - 'A' + 10) & 0xF;
}

char LowerNibbleToASCIIHexDigit(std::uint8_t value) {
    const std::uint8_t nibble = value & 0xF;
    return static_cast<char>(nibble + (nibble < 10 ? '0' : 'A' - 10));
}

char UpperNibbleToASCIIHexDigit(std::uint8_t value) {
    const std::uint8_t nibble = value >> 4;
    return static_cast<char>(nibble + (nibble < 10 ? '0' : 'A' - 10));
}

bool IsASCIIDigitOrPunctuation(char16_t character) {
    return (character >= '!' && character <= '@') || (character >= '[' && character <= '`') || (character >= '{' && character <= '~');
}

template<UScriptCode> bool IsLookalikeCharacterOfScriptType(std::int32_t);

template<> bool IsLookalikeCharacterOfScriptType<USCRIPT_ARMENIAN>(std::int32_t codePoint) {
    switch (codePoint) {
    case 0x0548:
    case 0x054D:
    case 0x0551:
    case 0x0555:
    case 0x0578:
    case 0x057D:
    case 0x0581:
    case 0x0585:
        return true;
    default:
        return false;
    }
}

template<> bool IsLookalikeCharacterOfScriptType<USCRIPT_TAMIL>(std::int32_t codePoint) {
    return codePoint == 0x0BE6;
}

template<> bool IsLookalikeCharacterOfScriptType<USCRIPT_CANADIAN_ABORIGINAL>(std::int32_t codePoint) {
    switch (codePoint) {
    case 0x146D:
    case 0x146F:
    case 0x1472:
    case 0x14AA:
    case 0x157C:
    case 0x1587:
    case 0x15AF:
    case 0x15B4:
    case 0x15C5:
    case 0x15DE:
    case 0x15E9:
    case 0x15F1:
    case 0x15F4:
    case 0x166D:
    case 0x166E:
        return true;
    default:
        return false;
    }
}

template<> bool IsLookalikeCharacterOfScriptType<USCRIPT_THAI>(std::int32_t codePoint) {
    return codePoint == 0x0E01;
}

template<UScriptCode ScriptType> bool IsOfScriptType(std::int32_t codePoint) {
    std::int32_t error = 0;
    const auto script = uscript_getScript_nid_postfix(codePoint, &error);
    if (error) return false;
    return script == ScriptType;
}

bool IsASCIIDigitOrValidHostCharacter(char16_t character) {
    if (!IsASCIIDigitOrPunctuation(character)) return false;
    switch (character) {
    case '#':
    case '%':
    case '/':
    case ':':
    case '?':
    case '@':
    case '[':
    case '\\':
    case ']':
        return false;
    default:
        return true;
    }
}

template<UScriptCode ScriptType> bool IsLookalikeSequence(const std::optional<std::int32_t>& previousCodePoint, std::int32_t codePoint) {
    if (!previousCodePoint || *previousCodePoint == '/') return false;
    auto isLookalikePair = [](char16_t first, char16_t second) {
        return IsLookalikeCharacterOfScriptType<ScriptType>(first) && !(IsOfScriptType<ScriptType>(second) || IsASCIIDigitOrValidHostCharacter(second));
    };
    return isLookalikePair(static_cast<char16_t>(codePoint), static_cast<char16_t>(*previousCodePoint))
        || isLookalikePair(static_cast<char16_t>(*previousCodePoint), static_cast<char16_t>(codePoint));
}

template<> bool IsLookalikeSequence<USCRIPT_ARABIC>(const std::optional<std::int32_t>& previousCodePoint, std::int32_t codePoint) {
    const bool isArabicDiacritic = 0x064B <= codePoint && codePoint <= 0x065F;
    const bool previousIsArabic = previousCodePoint && ublock_getCode_nid_postfix(*previousCodePoint) == UBLOCK_ARABIC;
    return isArabicDiacritic && !previousIsArabic;
}

bool IsLookalikeCharacter(const std::optional<std::int32_t>& previousCodePoint, std::int32_t codePoint) {
    if (!u_isprint_nid_postfix(codePoint)
        || u_isUWhiteSpace_nid_postfix(codePoint)
        || u_hasBinaryProperty_nid_postfix(codePoint, UCHAR_DEFAULT_IGNORABLE_CODE_POINT))
        return true;

    const auto block = ublock_getCode_nid_postfix(codePoint);
    if (block == UBLOCK_IPA_EXTENSIONS || block == UBLOCK_DESERET) return true;

    switch (codePoint) {
    case 0x00BC:
    case 0x00BD:
    case 0x00BE:
    case 0x01C0:
    case 0x01C3:
    case 0x1E9C:
    case 0x1E9D:
    case 0x1EFE:
    case 0x1EFF:
    case 0x0237:
    case 0x0251:
    case 0x0261:
    case 0x02D0:
    case 0x0335:
    case 0x0337:
    case 0x0338:
    case 0x0589:
    case 0x05B4:
    case 0x05BC:
    case 0x05C3:
    case 0x05F4:
    case 0x0609:
    case 0x060A:
    case 0x0650:
    case 0x0660:
    case 0x066A:
    case 0x06D4:
    case 0x06F0:
    case 0x0701:
    case 0x0702:
    case 0x0703:
    case 0x0704:
    case 0x1735:
    case 0x1D04:
    case 0x1D0F:
    case 0x1D1C:
    case 0x1D20:
    case 0x1D21:
    case 0x1D22:
    case 0x1ECD:
    case 0x2010:
    case 0x2011:
    case 0x2024:
    case 0x2027:
    case 0x2039:
    case 0x203A:
    case 0x2041:
    case 0x2044:
    case 0x2052:
    case 0x2153:
    case 0x2154:
    case 0x2155:
    case 0x2156:
    case 0x2157:
    case 0x2158:
    case 0x2159:
    case 0x215A:
    case 0x215B:
    case 0x215C:
    case 0x215D:
    case 0x215E:
    case 0x215F:
    case 0x2212:
    case 0x2215:
    case 0x2216:
    case 0x2236:
    case 0x233F:
    case 0x23AE:
    case 0x244A:
    case 0x2571:
    case 0x2572:
    case 0x29F6:
    case 0x29F8:
    case 0x2AFB:
    case 0x2AFD:
    case 0x2FF0:
    case 0x2FF1:
    case 0x2FF2:
    case 0x2FF3:
    case 0x2FF4:
    case 0x2FF5:
    case 0x2FF6:
    case 0x2FF7:
    case 0x2FF8:
    case 0x2FF9:
    case 0x2FFA:
    case 0x2FFB:
    case 0x3002:
    case 0x3008:
    case 0x3014:
    case 0x3015:
    case 0x3033:
    case 0x3035:
    case 0x321D:
    case 0x321E:
    case 0x33AE:
    case 0x33AF:
    case 0x33C6:
    case 0x33DF:
    case 0x05B9:
    case 0x05BA:
    case 0x05C1:
    case 0x05C2:
    case 0x05C4:
    case 0xA731:
    case 0xA771:
    case 0xA789:
    case 0xFE14:
    case 0xFE15:
    case 0xFE3F:
    case 0xFE5D:
    case 0xFE5E:
    case 0xFF0E:
    case 0xFF0F:
    case 0xFF61:
    case 0xFFFC:
    case 0xFFFD:
    case 0x1F50F:
    case 0x1F510:
    case 0x1F511:
    case 0x1F512:
    case 0x1F513:
        return true;
    case 0x0307:
        return previousCodePoint == 0x0237 || previousCodePoint == 0x0131 || previousCodePoint == 0x05D5;
    case '.':
        return false;
    default:
        return IsLookalikeSequence<USCRIPT_ARMENIAN>(previousCodePoint, codePoint)
            || IsLookalikeSequence<USCRIPT_TAMIL>(previousCodePoint, codePoint)
            || IsLookalikeSequence<USCRIPT_CANADIAN_ABORIGINAL>(previousCodePoint, codePoint)
            || IsLookalikeSequence<USCRIPT_THAI>(previousCodePoint, codePoint)
            || IsLookalikeSequence<USCRIPT_ARABIC>(previousCodePoint, codePoint);
    }
}

unsigned U16Length(std::int32_t codePoint) {
    return static_cast<std::uint32_t>(codePoint) <= 0xFFFF ? 1 : 2;
}

String EscapeUnsafeCharacters(const String& source) {
    const auto view = NKWebKit::WtfAbi::ViewOf(source.impl);
    const unsigned length = view.length;
    std::optional<std::int32_t> previousCodePoint;

    unsigned i = 0;
    while (i < length) {
        const auto c = _ZNK3WTF6String19characterStartingAtEj(&source, i);
        if (IsLookalikeCharacter(previousCodePoint, c)) break;
        previousCodePoint = c;
        i += U16Length(c);
    }
    if (i == length) return source;

    std::u16string output;
    for (unsigned j = 0; j < i; ++j) output.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, j));
    while (i < length) {
        const auto c = _ZNK3WTF6String19characterStartingAtEj(&source, i);
        const unsigned characterLength = U16Length(c);
        if (IsLookalikeCharacter(previousCodePoint, c)) {
            char16_t units[2] = {static_cast<char16_t>(c), 0};
            if (characterLength == 2) {
                units[0] = NKWebKit::WtfAbi::CodeUnitAt(view, i);
                units[1] = NKWebKit::WtfAbi::CodeUnitAt(view, i + 1);
            }
            const CString utf8 = _ZN3WTF10StringImpl17utf8ForCharactersEPKDsjNS_14ConversionModeE(units, characterLength, 1);
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(utf8.buffer + 1);
            for (std::size_t j = 0; j < utf8.buffer->length; ++j) {
                output.push_back(u'%');
                output.push_back(static_cast<char16_t>(UpperNibbleToASCIIHexDigit(bytes[j])));
                output.push_back(static_cast<char16_t>(LowerNibbleToASCIIHexDigit(bytes[j])));
            }
        } else {
            for (unsigned j = 0; j < characterLength; ++j) output.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, i + j));
        }
        previousCodePoint = c;
        i += characterLength;
    }

    String result;
    _ZN3WTF6StringC1EPKDsj(&result, output.data(), static_cast<unsigned>(output.size()));
    return result;
}

String FromUTF8(const std::string& bytes) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(bytes.c_str()));
}

}  // namespace

extern "C" String APS5_VABI _ZN3WTF10URLHelpers14userVisibleURLERKNS_7CStringE(const CString& url) {
    const std::size_t length = url.buffer ? url.buffer->length : 0;
    if (!length) return String();
    if (length > (static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) - 1) / 3) return String();
    const auto* before = reinterpret_cast<const unsigned char*>(url.buffer + 1);

    bool mayNeedHostNameDecoding = false;
    std::string after;
    after.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char c = before[i];
        if (c == '%' && i + 2 < length && IsASCIIHexDigit(before[i + 1]) && IsASCIIHexDigit(before[i + 2])) {
            const auto u = ToASCIIHexValue(before[i + 1]) << 4 | ToASCIIHexValue(before[i + 2]);
            if (u > 0x7F) after.push_back(static_cast<char>(u));
            else after.append(reinterpret_cast<const char*>(before + i), 3);
            i += 2;
        } else {
            after.push_back(static_cast<char>(c));
            const auto size = after.size();
            if (c == '-' && i >= 3 && size >= 4 && !mayNeedHostNameDecoding && (after[size - 4] | 0x20) == 'x' && (after[size - 3] | 0x20) == 'n' && after[size - 2] == '-')
                mayNeedHostNameDecoding = true;
        }
    }

    String result = FromUTF8(after);
    if (!result.impl) {
        std::string escaped;
        for (const char byte : after) {
            if (!byte) break;
            const auto c = static_cast<std::uint8_t>(byte);
            if (c > 0x7F) {
                escaped.push_back('%');
                escaped.push_back(UpperNibbleToASCIIHexDigit(c));
                escaped.push_back(LowerNibbleToASCIIHexDigit(c));
            } else escaped.push_back(byte);
        }
        result = FromUTF8(escaped);
    }

    if (mayNeedHostNameDecoding) NotImplemented_nid_no_patch(__func__);

    const String normalized = _ZN3WTF13normalizedNFCERKNS_6StringE(&result);
    return EscapeUnsafeCharacters(normalized);
}
