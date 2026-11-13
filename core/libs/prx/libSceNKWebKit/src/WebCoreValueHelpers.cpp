// Derived from Source/WebCore/platform/text/LocaleToScriptMapping.cpp, Source/WebCore/loader/ResourceLoadStatistics.cpp and Source/WebCore/page/SettingsBase.cpp (PS5-WebKit-11.x 80f77e94c63e)
/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WebCoreValueHelpersExports.hpp"
#include "unicode/uscript.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace {

static_assert(sizeof(UScriptCode) == sizeof(std::int32_t));

constexpr double TimestampResolution = 5.0;
constexpr unsigned Is8BitFlag = 4;
constexpr unsigned PackedLength = 4;

constexpr std::uint32_t Pack(const char (&name)[PackedLength + 1]) {
    std::uint32_t result = 0;
    for (unsigned index = 0; index < PackedLength; ++index) result = (result << 8) | static_cast<unsigned char>(name[index]);
    return result;
}

constexpr std::pair<std::uint32_t, UScriptCode> ScriptNameCodes[] = {
    { Pack("arab"), USCRIPT_ARABIC },
    { Pack("armn"), USCRIPT_ARMENIAN },
    { Pack("bali"), USCRIPT_BALINESE },
    { Pack("batk"), USCRIPT_BATAK },
    { Pack("beng"), USCRIPT_BENGALI },
    { Pack("blis"), USCRIPT_BLISSYMBOLS },
    { Pack("bopo"), USCRIPT_BOPOMOFO },
    { Pack("brah"), USCRIPT_BRAHMI },
    { Pack("brai"), USCRIPT_BRAILLE },
    { Pack("bugi"), USCRIPT_BUGINESE },
    { Pack("buhd"), USCRIPT_BUHID },
    { Pack("cans"), USCRIPT_CANADIAN_ABORIGINAL },
    { Pack("cham"), USCRIPT_CHAM },
    { Pack("cher"), USCRIPT_CHEROKEE },
    { Pack("cirt"), USCRIPT_CIRTH },
    { Pack("copt"), USCRIPT_COPTIC },
    { Pack("cprt"), USCRIPT_CYPRIOT },
    { Pack("cyrl"), USCRIPT_CYRILLIC },
    { Pack("cyrs"), USCRIPT_OLD_CHURCH_SLAVONIC_CYRILLIC },
    { Pack("deva"), USCRIPT_DEVANAGARI },
    { Pack("dsrt"), USCRIPT_DESERET },
    { Pack("egyd"), USCRIPT_DEMOTIC_EGYPTIAN },
    { Pack("egyh"), USCRIPT_HIERATIC_EGYPTIAN },
    { Pack("egyp"), USCRIPT_EGYPTIAN_HIEROGLYPHS },
    { Pack("ethi"), USCRIPT_ETHIOPIC },
    { Pack("geok"), USCRIPT_KHUTSURI },
    { Pack("geor"), USCRIPT_GEORGIAN },
    { Pack("glag"), USCRIPT_GLAGOLITIC },
    { Pack("goth"), USCRIPT_GOTHIC },
    { Pack("grek"), USCRIPT_GREEK },
    { Pack("gujr"), USCRIPT_GUJARATI },
    { Pack("guru"), USCRIPT_GURMUKHI },
    { Pack("hang"), USCRIPT_HANGUL },
    { Pack("hani"), USCRIPT_HAN },
    { Pack("hano"), USCRIPT_HANUNOO },
    { Pack("hans"), USCRIPT_SIMPLIFIED_HAN },
    { Pack("hant"), USCRIPT_TRADITIONAL_HAN },
    { Pack("hebr"), USCRIPT_HEBREW },
    { Pack("hira"), USCRIPT_KATAKANA_OR_HIRAGANA },
    { Pack("hmng"), USCRIPT_PAHAWH_HMONG },
    { Pack("hrkt"), USCRIPT_KATAKANA_OR_HIRAGANA },
    { Pack("hung"), USCRIPT_OLD_HUNGARIAN },
    { Pack("inds"), USCRIPT_HARAPPAN_INDUS },
    { Pack("ital"), USCRIPT_OLD_ITALIC },
    { Pack("java"), USCRIPT_JAVANESE },
    { Pack("jpan"), USCRIPT_KATAKANA_OR_HIRAGANA },
    { Pack("kali"), USCRIPT_KAYAH_LI },
    { Pack("kana"), USCRIPT_KATAKANA_OR_HIRAGANA },
    { Pack("khar"), USCRIPT_KHAROSHTHI },
    { Pack("khmr"), USCRIPT_KHMER },
    { Pack("knda"), USCRIPT_KANNADA },
    { Pack("kore"), USCRIPT_HANGUL },
    { Pack("laoo"), USCRIPT_LAO },
    { Pack("latf"), USCRIPT_LATIN_FRAKTUR },
    { Pack("latg"), USCRIPT_LATIN_GAELIC },
    { Pack("latn"), USCRIPT_LATIN },
    { Pack("lepc"), USCRIPT_LEPCHA },
    { Pack("limb"), USCRIPT_LIMBU },
    { Pack("lina"), USCRIPT_LINEAR_A },
    { Pack("linb"), USCRIPT_LINEAR_B },
    { Pack("mand"), USCRIPT_MANDAEAN },
    { Pack("maya"), USCRIPT_MAYAN_HIEROGLYPHS },
    { Pack("mero"), USCRIPT_MEROITIC },
    { Pack("mlym"), USCRIPT_MALAYALAM },
    { Pack("mong"), USCRIPT_MONGOLIAN },
    { Pack("mymr"), USCRIPT_MYANMAR },
    { Pack("nkoo"), USCRIPT_NKO },
    { Pack("ogam"), USCRIPT_OGHAM },
    { Pack("orkh"), USCRIPT_ORKHON },
    { Pack("orya"), USCRIPT_ORIYA },
    { Pack("osma"), USCRIPT_OSMANYA },
    { Pack("perm"), USCRIPT_OLD_PERMIC },
    { Pack("phag"), USCRIPT_PHAGS_PA },
    { Pack("phnx"), USCRIPT_PHOENICIAN },
    { Pack("plrd"), USCRIPT_PHONETIC_POLLARD },
    { Pack("qaai"), USCRIPT_INHERITED },
    { Pack("roro"), USCRIPT_RONGORONGO },
    { Pack("runr"), USCRIPT_RUNIC },
    { Pack("sara"), USCRIPT_SARATI },
    { Pack("shaw"), USCRIPT_SHAVIAN },
    { Pack("sinh"), USCRIPT_SINHALA },
    { Pack("sylo"), USCRIPT_SYLOTI_NAGRI },
    { Pack("syrc"), USCRIPT_SYRIAC },
    { Pack("syre"), USCRIPT_ESTRANGELO_SYRIAC },
    { Pack("syrj"), USCRIPT_WESTERN_SYRIAC },
    { Pack("syrn"), USCRIPT_EASTERN_SYRIAC },
    { Pack("tagb"), USCRIPT_TAGBANWA },
    { Pack("tale"), USCRIPT_TAI_LE },
    { Pack("talu"), USCRIPT_NEW_TAI_LUE },
    { Pack("taml"), USCRIPT_TAMIL },
    { Pack("telu"), USCRIPT_TELUGU },
    { Pack("teng"), USCRIPT_TENGWAR },
    { Pack("tfng"), USCRIPT_TIFINAGH },
    { Pack("tglg"), USCRIPT_TAGALOG },
    { Pack("thaa"), USCRIPT_THAANA },
    { Pack("thai"), USCRIPT_THAI },
    { Pack("tibt"), USCRIPT_TIBETAN },
    { Pack("ugar"), USCRIPT_UGARITIC },
    { Pack("vaii"), USCRIPT_VAI },
    { Pack("visp"), USCRIPT_VISIBLE_SPEECH },
    { Pack("xpeo"), USCRIPT_OLD_PERSIAN },
    { Pack("xsux"), USCRIPT_CUNEIFORM },
    { Pack("yiii"), USCRIPT_YI },
    { Pack("zxxx"), USCRIPT_UNWRITTEN_LANGUAGES },
    { Pack("zyyy"), USCRIPT_COMMON },
    { Pack("zzzz"), USCRIPT_UNKNOWN },
};

static_assert(std::is_sorted(std::begin(ScriptNameCodes), std::end(ScriptNameCodes), [](const auto& a, const auto& b) { return a.first < b.first; }));
static_assert(Pack("arab") == 0x61726162U);
static_assert(Pack("zzzz") == 0x7a7a7a7aU);

template <typename Character>
bool PackFolded(const Character* characters, unsigned length, std::uint32_t& packed) {
    if (length > PackedLength) return false;
    packed = 0;
    for (unsigned index = 0; index < length; ++index) {
        auto code = static_cast<std::uint32_t>(characters[index]);
        if (code >= 0x80) return false;
        packed |= (code | 0x20) << ((PackedLength - index - 1) * 8);
    }
    return true;
}

}  // namespace

extern "C" {

WTF::WallTime APS5_VABI _ZN7WebCore22ResourceLoadStatistics20reduceTimeResolutionEN3WTF8WallTimeE(WTF::WallTime time) {
    return WTF::WallTime{std::floor(time.m_value / TimestampResolution) * TimestampResolution};
}

bool APS5_VABI _ZN7WebCore12SettingsBase33platformDefaultMediaSourceEnabledEv() {
    return true;
}

std::int32_t APS5_VABI _ZN7WebCore16scriptNameToCodeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scriptName) {
    std::uint32_t packed = 0;
    const NKWebKit::WtfAbi::StringImpl* impl = scriptName.impl;
    if (impl) {
        bool parsed = impl->hashAndFlags & Is8BitFlag ? PackFolded(static_cast<const unsigned char*>(impl->data), impl->length, packed) : PackFolded(static_cast<const char16_t*>(impl->data), impl->length, packed);
        if (!parsed) return USCRIPT_INVALID_CODE;
    }
    auto entry = std::lower_bound(std::begin(ScriptNameCodes), std::end(ScriptNameCodes), packed, [](const auto& pair, std::uint32_t value) { return pair.first < value; });
    if (entry == std::end(ScriptNameCodes) || entry->first != packed) return USCRIPT_INVALID_CODE;
    return entry->second;
}

}
