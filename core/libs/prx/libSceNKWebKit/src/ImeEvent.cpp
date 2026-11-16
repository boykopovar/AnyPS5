// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebKit/UIProcess/API/C/neko/WKImeEvent.h and WKImeEvent.cpp
/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <array>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

namespace {

struct InputLanguageEntry {
    const char* attr;
    WKInputLanguage value;
};

constexpr InputLanguageEntry languageEntries[] = {
    { "da_Latn_DK",         INPUT_LANGUAGE_DANISH },
    { "de_Latn_DE",         INPUT_LANGUAGE_GERMAN },
    { "en_Latn_US",         INPUT_LANGUAGE_ENGLISH_US },
    { "es_Latn_ES",         INPUT_LANGUAGE_SPANISH },
    { "fr_Latn_FR",         INPUT_LANGUAGE_FRENCH },
    { "it_Latn_IT",         INPUT_LANGUAGE_ITALIAN },
    { "nl_Latn_NL",         INPUT_LANGUAGE_DUTCH },
    { "no_Latn_NO",         INPUT_LANGUAGE_NORWEGIAN },
    { "pl_Latn_PL",         INPUT_LANGUAGE_POLISH },
    { "pt_Latn_BR",         INPUT_LANGUAGE_PORTUGUESE_BR },
    { "ru_Cyrl_RU",         INPUT_LANGUAGE_RUSSIAN },
    { "fi_Latn_FI",         INPUT_LANGUAGE_FINNISH },
    { "se_Latn_NO",         INPUT_LANGUAGE_SWEDISH },
    { "sv_Latn_SE",         INPUT_LANGUAGE_SWEDISH },
    { "ja_Jpan_JP",         INPUT_LANGUAGE_JAPANESE },
    { "ko_Kore_KR",         INPUT_LANGUAGE_KOREAN },
    { "zh_Hans_CN",         INPUT_LANGUAGE_SIMPLIFIED_CHINESE },
    { "tr_Latn_TR",         INPUT_LANGUAGE_TURKISH },
    { "ar_Arab_EG",         INPUT_LANGUAGE_ARABIC },
    { "th_Thai_TH",         INPUT_LANGUAGE_THAI },
    { "cs_Latn_CZ",         INPUT_LANGUAGE_CZECH },
    { "el_Grek_GR",         INPUT_LANGUAGE_GREEK },
    { "id_Latn_ID",         INPUT_LANGUAGE_INDONESIAN },
    { "vi_Latn_VN",         INPUT_LANGUAGE_VIETNAMESE },
    { "ro_Latn_RO",         INPUT_LANGUAGE_ROMANIAN },
    { "hu_Latn_HU",         INPUT_LANGUAGE_HUNGARIAN },
    { "uk_Cyrl_UA",         INPUT_LANGUAGE_UKRAINIAN },
    { "en_Latn_GB",         INPUT_LANGUAGE_ENGLISH_GB },
    { "es_Latn_419",        INPUT_LANGUAGE_SPANISH_LATIN_AMERICA },
    { "pt_Latn_PT",         INPUT_LANGUAGE_PORTUGUESE_PT },
    { "zh_Hant_HK",         INPUT_LANGUAGE_TRADITIONAL_CHINESE },
    { "zh_Hant_TW",         INPUT_LANGUAGE_TRADITIONAL_CHINESE },
    { "fr_Latn_CA",         INPUT_LANGUAGE_CANADIAN_FRENCH },
};

struct LikelySubtagDefaults {
    const char* language;
    const char* script;
    const char* region;
};

constexpr LikelySubtagDefaults likelyDefaults[] = {
    { "ar", "Arab", "EG" },
    { "cs", "Latn", "CZ" },
    { "da", "Latn", "DK" },
    { "de", "Latn", "DE" },
    { "el", "Grek", "GR" },
    { "en", "Latn", "US" },
    { "es", "Latn", "ES" },
    { "fi", "Latn", "FI" },
    { "fr", "Latn", "FR" },
    { "hu", "Latn", "HU" },
    { "id", "Latn", "ID" },
    { "it", "Latn", "IT" },
    { "ja", "Jpan", "JP" },
    { "ko", "Kore", "KR" },
    { "nl", "Latn", "NL" },
    { "no", "Latn", "NO" },
    { "pl", "Latn", "PL" },
    { "pt", "Latn", "BR" },
    { "ro", "Latn", "RO" },
    { "ru", "Cyrl", "RU" },
    { "se", "Latn", "NO" },
    { "sv", "Latn", "SE" },
    { "th", "Thai", "TH" },
    { "tr", "Latn", "TR" },
    { "uk", "Cyrl", "UA" },
    { "vi", "Latn", "VN" },
    { "zh", "Hans", "CN" },
};

std::string ToLowerAscii(std::string_view view) {
    std::string result(view);
    for (char& ch : result) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

std::string ToUpperAscii(std::string_view view) {
    std::string result(view);
    for (char& ch : result) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return result;
}

std::string ToTitleAscii(std::string_view view) {
    std::string result = ToLowerAscii(view);
    if (!result.empty()) {
        result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
    }
    return result;
}

bool IsAlphaString(std::string_view view) {
    for (char ch : view) {
        if (!std::isalpha(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return !view.empty();
}

bool IsDigitString(std::string_view view) {
    for (char ch : view) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return !view.empty();
}

std::string ExpandLocaleSubtags(std::string_view code) {
    std::size_t keywordPos = code.find('@');
    if (keywordPos != std::string_view::npos) {
        code = code.substr(0, keywordPos);
    }
    std::array<std::string_view, 4> parts{};
    std::size_t count = 0;
    std::size_t pos = 0;
    while (pos <= code.size() && count < parts.size()) {
        std::size_t sep = code.find_first_of("-_", pos);
        if (sep == std::string_view::npos) {
            parts[count++] = code.substr(pos);
            break;
        }
        parts[count++] = code.substr(pos, sep - pos);
        pos = sep + 1;
    }
    if (count == 0 || !IsAlphaString(parts[0])) {
        return {};
    }
    std::string lang = ToLowerAscii(parts[0]);
    std::string script;
    std::string region;
    std::size_t index = 1;
    if (index < count && parts[index].size() == 4 && IsAlphaString(parts[index])) {
        script = ToTitleAscii(parts[index]);
        ++index;
    }
    if (index < count &&
        ((parts[index].size() == 2 && IsAlphaString(parts[index])) ||
         (parts[index].size() == 3 && IsDigitString(parts[index])))) {
        region = ToUpperAscii(parts[index]);
    }

    if (lang == "zh" && script.empty() && (region == "TW" || region == "HK" || region == "MO")) {
        script = "Hant";
    }
    if (lang == "zh" && script == "Hant" && region.empty()) {
        region = "TW";
    }

    const LikelySubtagDefaults* found = nullptr;
    for (const auto& defaults : likelyDefaults) {
        if (lang == defaults.language) {
            found = &defaults;
            break;
        }
    }
    if (!found) {
        return {};
    }
    if (script.empty()) {
        script = found->script;
    }
    if (region.empty()) {
        region = found->region;
    }
    return lang + "_" + script + "_" + region;
}

int CountCompositionCharacters(const char* compositionString) {
    if (!compositionString) {
        return 0;
    }
    int count = 0;
    for (const auto* p = reinterpret_cast<const unsigned char*>(compositionString); *p; ++p) {
        if ((*p & 0xC0u) != 0x80u) {
            ++count;
        }
    }
    return count;
}

}  // namespace

extern "C" {

WKInputLanguage APS5_VABI WKImeEventGetInputLanguage(const char* languageCode) {
    if (!languageCode || !languageCode[0]) {
        return InputLanguageUndefined;
    }
    const std::string expanded = ExpandLocaleSubtags(languageCode);
    const char* candidate = expanded.empty() ? languageCode : expanded.c_str();
    for (const auto& entry : languageEntries) {
        if (std::strcmp(candidate, entry.attr) == 0) {
            return entry.value;
        }
    }
    return InputLanguageUndefined;
}

WKImeEvent* APS5_VABI WKImeEventMake(WKImeEventType imeEventType, const char* compositionString, int keyCode) {
    auto* imeEvent = new WKImeEvent();
    imeEvent->imeEventType = imeEventType;
    imeEvent->compositionString = compositionString;
    imeEvent->keyCode = keyCode;
    imeEvent->caretIndex = CountCompositionCharacters(compositionString);
    return imeEvent;
}

void APS5_VABI WKImeEventRelease(WKImeEvent* imeEvent) {
    delete imeEvent;
}

}
