#include "prx/libSceSystemService/LanguageLocale.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void Require(bool value, std::string_view what) {
    if (value) return;
    std::fprintf(stderr, "check failed: %.*s\n", static_cast<int>(what.size()), what.data());
    std::abort();
}

struct ParsedTag {
    std::string_view text;
    std::string_view language;
    std::string_view script;
    std::string_view region;
};

void RequireBcp47Parsing() {
    constexpr ParsedTag accepted[] = {
        {"en", "en", "", ""},
        {"en-US", "en", "", "US"},
        {"EN-us", "en", "", "US"},
        {"pt-BR", "pt", "", "BR"},
        {"es-419", "es", "", "419"},
        {"zh-Hant-TW", "zh", "Hant", "TW"},
        {"zh-hans-cn", "zh", "Hans", "CN"},
        {"ZH-HANT", "zh", "Hant", ""},
        {"sr-Latn-RS", "sr", "Latn", "RS"},
        {"fil", "fil", "", ""},
        {"en-US-u-ca-gregory", "en", "", "US"},
        {"de-DE-1996", "de", "", "DE"},
        {"ca-ES-valencia", "ca", "", "ES"},
        {"en-x-private", "en", "", ""},
        {"en-Latn-US-x-a", "en", "Latn", "US"},
    };
    for (const auto& expected : accepted) {
        SystemServiceLocale::Tag tag;
        Require(SystemServiceLocale::ParseTag(expected.text, tag), expected.text);
        Require(tag.language == expected.language, expected.text);
        Require(tag.script == expected.script, expected.text);
        Require(tag.region == expected.region, expected.text);
    }
    for (std::string_view rejected : {"", "-", "en-", "-en", "en--US", "e", "english", "e1", "12", "en_US", "en-US.UTF-8", "en US", "en-U$", "en-abcdefghi", "C", "x-foo"}) {
        SystemServiceLocale::Tag tag{"keep", "keep", "keep"};
        const bool parsed = SystemServiceLocale::ParseTag(rejected, tag);
        Require(!parsed, rejected);
        Require(tag.language == "keep" && tag.script == "keep" && tag.region == "keep", rejected);
    }
}

void RequirePosixNormalisation() {
    constexpr std::pair<std::string_view, std::string_view> names[] = {
        {"en_US.UTF-8", "en-US"},
        {"en_GB.UTF-8@euro", "en-GB"},
        {"de_DE.ISO-8859-15@euro", "de-DE"},
        {"pt_BR", "pt-BR"},
        {"zh_TW", "zh-TW"},
        {"zh_Hant_TW", "zh-Hant-TW"},
        {"ca_ES@valencia", "ca-ES"},
        {"sr_RS@latin", "sr-Latn-RS"},
        {"sr@latin", "sr-Latn"},
        {"sr_Latn_RS@latin", "sr-Latn-RS"},
        {"uz_UZ@Cyrillic", "uz-Cyrl-UZ"},
        {"fr", "fr"},
        {"en-US", "en-US"},
        {"EN_us.utf8", "EN-us"},
        {"C", ""},
        {"POSIX", ""},
        {"c", ""},
        {"posix", ""},
        {"C.UTF-8", ""},
        {"C.utf8@euro", ""},
        {"", ""},
        {"@latin", ""},
        {".UTF-8", ""},
    };
    for (const auto& [name, expected] : names) {
        Require(SystemServiceLocale::NormalizePosixLocale(name) == expected, name);
    }
}

void RequireEitherForm() {
    constexpr ParsedTag locales[] = {
        {"en_US.UTF-8", "en", "", "US"},
        {"en-US", "en", "", "US"},
        {"zh_Hant_TW.UTF-8", "zh", "Hant", "TW"},
        {"zh-Hans-CN", "zh", "Hans", "CN"},
        {"sr_RS@latin", "sr", "Latn", "RS"},
        {"pt_BR.utf8", "pt", "", "BR"},
        {"es_419", "es", "", "419"},
        {"fr", "fr", "", ""},
    };
    for (const auto& expected : locales) {
        SystemServiceLocale::Tag tag;
        Require(SystemServiceLocale::ParseLocale(expected.text, tag), expected.text);
        Require(tag.language == expected.language && tag.script == expected.script && tag.region == expected.region, expected.text);
    }
    for (std::string_view none : {"", "C", "POSIX", "C.UTF-8", "@latin", "garbage!", "english", "e", "en__US", "_US", "123"}) {
        SystemServiceLocale::Tag tag{"keep", "keep", "keep"};
        Require(!SystemServiceLocale::ParseLocale(none, tag), none);
        Require(tag.language == "keep" && tag.script == "keep" && tag.region == "keep", none);
    }
}

struct LocaleLanguage {
    std::string_view locale;
    int id;
};

int LanguageOf(std::string_view locale) {
    SystemServiceLocale::Tag tag;
    if (!SystemServiceLocale::ParseLocale(locale, tag)) return SystemServiceLocale::NoConsoleLanguage;
    return SystemServiceLocale::ConsoleLanguageForTag(tag);
}

void RequireLanguages(const LocaleLanguage* first, const LocaleLanguage* last) {
    for (const LocaleLanguage* entry = first; entry != last; ++entry) Require(LanguageOf(entry->locale) == entry->id, entry->locale);
}

template <std::size_t N>
void RequireLanguages(const LocaleLanguage (&entries)[N]) {
    RequireLanguages(entries, entries + N);
}

void RequireLatinEuropeanRows() {
    using namespace SystemServiceLocale;
    constexpr LocaleLanguage entries[] = {
        {"en", EnglishUs}, {"en-US", EnglishUs}, {"en_US.UTF-8", EnglishUs}, {"EN-us", EnglishUs}, {"en-CA", EnglishUs}, {"en-IN", EnglishUs},
        {"en-GB", EnglishUk}, {"en_GB.UTF-8@euro", EnglishUk}, {"en-IE", EnglishUk}, {"en-AU", EnglishUk}, {"en-NZ", EnglishUk}, {"en-Latn-GB", EnglishUk},
        {"fr", French}, {"fr-FR", French}, {"fr_BE.UTF-8", French}, {"fr-CH", French}, {"fr-CA", FrenchCanada}, {"fr_CA.UTF-8", FrenchCanada},
        {"de", German}, {"de-DE", German}, {"de_AT.UTF-8", German}, {"de-CH", German},
        {"it", Italian}, {"it-IT", Italian}, {"it_CH", Italian},
        {"nl", Dutch}, {"nl-NL", Dutch}, {"nl_BE.UTF-8", Dutch},
        {"ro", Romanian}, {"ro-RO", Romanian}, {"mo", Romanian},
    };
    RequireLanguages(entries);
}

void RequireIberianRows() {
    using namespace SystemServiceLocale;
    constexpr LocaleLanguage entries[] = {
        {"es", Spanish}, {"es-ES", Spanish}, {"es_ES.UTF-8@euro", Spanish}, {"ES-es", Spanish}, {"es-GQ", Spanish}, {"es-Latn-ES", Spanish},
        {"es-419", SpanishLatinAmerica}, {"es_MX.UTF-8", SpanishLatinAmerica}, {"es-AR", SpanishLatinAmerica}, {"es-CO", SpanishLatinAmerica},
        {"es_CL", SpanishLatinAmerica}, {"es-PE", SpanishLatinAmerica}, {"es-VE", SpanishLatinAmerica}, {"es-US", SpanishLatinAmerica},
        {"es_UY.utf8", SpanishLatinAmerica}, {"es-PR", SpanishLatinAmerica}, {"es-CU", SpanishLatinAmerica}, {"es-DO", SpanishLatinAmerica},
        {"pt", PortugueseBrazil}, {"pt-BR", PortugueseBrazil}, {"pt_BR.UTF-8", PortugueseBrazil}, {"PT-br", PortugueseBrazil},
        {"pt-PT", PortuguesePortugal}, {"pt_PT.UTF-8", PortuguesePortugal}, {"pt-AO", PortuguesePortugal}, {"pt-MZ", PortuguesePortugal},
        {"pt-CV", PortuguesePortugal}, {"pt-TL", PortuguesePortugal},
    };
    RequireLanguages(entries);
    constexpr const char* latinAmerica[] = {"MX", "AR", "BO", "CL", "CO", "CR", "CU", "DO", "EC", "GT", "HN", "NI", "PA", "PE", "PR", "PY", "SV", "US", "UY", "VE", "419"};
    for (const char* region : latinAmerica) Require(LanguageOf(std::string("es-") + region) == SpanishLatinAmerica, region);
}

void RequireChineseRows() {
    using namespace SystemServiceLocale;
    constexpr LocaleLanguage entries[] = {
        {"zh", ChineseSimplified}, {"zh-CN", ChineseSimplified}, {"zh_CN.UTF-8", ChineseSimplified}, {"zh-SG", ChineseSimplified}, {"zh_SG.utf8", ChineseSimplified},
        {"zh-Hans", ChineseSimplified}, {"zh-Hans-CN", ChineseSimplified}, {"zh-hans-sg", ChineseSimplified}, {"zh-Latn", ChineseSimplified}, {"zh-MY", ChineseSimplified},
        {"zh-Hant", ChineseTraditional}, {"zh-Hant-TW", ChineseTraditional}, {"zh_Hant_TW.UTF-8", ChineseTraditional}, {"zh-TW", ChineseTraditional}, {"zh_TW.UTF-8", ChineseTraditional},
        {"zh-HK", ChineseTraditional}, {"zh_HK.UTF-8", ChineseTraditional}, {"zh-MO", ChineseTraditional}, {"zh-Hant-HK", ChineseTraditional}, {"zh-Hant-MO", ChineseTraditional},
        {"zh-Hans-TW", ChineseSimplified}, {"zh-Hans-HK", ChineseSimplified}, {"zh-Hant-CN", ChineseTraditional},
        {"cmn", ChineseSimplified}, {"cmn-Hans-CN", ChineseSimplified}, {"cmn-Hant", ChineseTraditional}, {"cmn-TW", ChineseTraditional},
        {"yue", ChineseTraditional}, {"yue-HK", ChineseTraditional}, {"yue-Hans-CN", ChineseSimplified},
    };
    RequireLanguages(entries);
}

void RequireJapaneseKoreanRows() {
    using namespace SystemServiceLocale;
    constexpr LocaleLanguage entries[] = {
        {"ja", Japanese}, {"ja-JP", Japanese}, {"ja_JP.UTF-8", Japanese}, {"JA-jp", Japanese}, {"ja-Jpan-JP", Japanese}, {"ja_JP.eucJP", Japanese},
        {"ko", Korean}, {"ko-KR", Korean}, {"ko_KR.UTF-8", Korean}, {"ko-KP", Korean}, {"ko-Kore-KR", Korean},
    };
    RequireLanguages(entries);
}

void RequireSlavicNordicRows() {
    using namespace SystemServiceLocale;
    constexpr LocaleLanguage entries[] = {
        {"ru", Russian}, {"ru-RU", Russian}, {"ru_RU.UTF-8", Russian}, {"ru-UA", Russian}, {"ru-BY", Russian},
        {"uk", Ukrainian}, {"uk-UA", Ukrainian}, {"uk_UA.UTF-8", Ukrainian},
        {"pl", Polish}, {"pl-PL", Polish}, {"pl_PL.UTF-8", Polish},
        {"cs", Czech}, {"cs-CZ", Czech}, {"cs_CZ.UTF-8", Czech},
        {"fi", Finnish}, {"fi-FI", Finnish}, {"fi_FI.UTF-8@euro", Finnish},
        {"sv", Swedish}, {"sv-SE", Swedish}, {"sv_FI.UTF-8", Swedish},
        {"da", Danish}, {"da-DK", Danish}, {"da_DK.UTF-8", Danish},
        {"no", Norwegian}, {"no-NO", Norwegian}, {"nb", Norwegian}, {"nb-NO", Norwegian}, {"nb_NO.UTF-8", Norwegian}, {"nn", Norwegian}, {"nn-NO", Norwegian}, {"nn_NO.UTF-8", Norwegian},
    };
    RequireLanguages(entries);
    for (std::string_view unmapped : {"lt-LT", "lv-LV", "et-EE", "sk-SK", "sl-SI", "bg-BG", "sr-RS", "sr_RS@latin", "hr-HR", "be-BY", "is-IS"}) {
        Require(LanguageOf(unmapped) == NoConsoleLanguage, unmapped);
    }
}

void RequireOtherRows() {
    using namespace SystemServiceLocale;
    constexpr LocaleLanguage entries[] = {
        {"ar", Arabic}, {"ar-AE", Arabic}, {"ar-SA", Arabic}, {"ar_EG.UTF-8", Arabic}, {"ar-MA", Arabic}, {"ar-001", Arabic},
        {"th", Thai}, {"th-TH", Thai}, {"th_TH.UTF-8", Thai},
        {"vi", Vietnamese}, {"vi-VN", Vietnamese}, {"vi_VN.utf8", Vietnamese},
        {"id", Indonesian}, {"id-ID", Indonesian}, {"id_ID.UTF-8", Indonesian}, {"in", Indonesian}, {"in_ID", Indonesian},
        {"tr", Turkish}, {"tr-TR", Turkish}, {"tr_TR.UTF-8", Turkish}, {"tr-CY", Turkish},
        {"el", Greek}, {"el-GR", Greek}, {"el_GR.UTF-8", Greek}, {"el-CY", Greek},
        {"hu", Hungarian}, {"hu-HU", Hungarian}, {"hu_HU.UTF-8", Hungarian},
    };
    RequireLanguages(entries);
}

void RequireFallbackRules() {
    using namespace SystemServiceLocale;
    constexpr const char* perLanguage[31] = {
        "ja-JP", "en-US", "fr-FR", "es-ES", "de-DE", "it-IT", "nl-NL", "pt-PT", "ru-RU", "ko-KR", "zh-Hant", "zh-Hans", "fi-FI", "sv-SE", "da-DK", "no-NO",
        "pl-PL", "pt-BR", "en-GB", "tr-TR", "es-419", "ar-AE", "fr-CA", "cs-CZ", "hu-HU", "el-GR", "ro-RO", "th-TH", "vi-VN", "id-ID", "uk-UA"};
    for (int id = 0; id <= 30; ++id) {
        const Match match = MatchLocale(perLanguage[id]);
        Require(match.recognised && match.id == id, perLanguage[id]);
    }
    for (std::string_view unrecognised : {"", "C", "POSIX", "C.UTF-8", "c", "garbage", "xx", "xx-YY", "x-foo", "en_", "_US", "lt-LT", "sw-KE", "he-IL", "hi-IN", "sr-Latn-RS", "ca-ES", "eu", "gl-ES", "qaa", "en--US", "12345", "\t"}) {
        const Match match = MatchLocale(unrecognised);
        Require(!match.recognised && match.id == EnglishUs, unrecognised);
    }
    Require(MatchLocale("en-US").recognised && MatchLocale("en-ZZ").recognised && MatchLocale("en-ZZ").id == EnglishUs, "en-ZZ");
    Require(MatchLocale("fr-ZZ").recognised && MatchLocale("fr-ZZ").id == French, "fr-ZZ");
    Require(MatchLocale("zh-ZZ").recognised && MatchLocale("zh-ZZ").id == ChineseSimplified, "zh-ZZ");
    Require(MatchLocale("pt-ZZ").recognised && MatchLocale("pt-ZZ").id == PortugueseBrazil, "pt-ZZ");
    Require(MatchLocale("es-ZZ").recognised && MatchLocale("es-ZZ").id == Spanish, "es-ZZ");
    Require(MatchLocale("de-Cyrl-RU").recognised && MatchLocale("de-Cyrl-RU").id == German, "de-Cyrl-RU");
}

std::vector<std::string> Posix(const std::map<std::string, std::string>& variables) {
    return SystemServiceLocale::PosixHostLocales([&variables](const char* name) -> const char* {
        const auto found = variables.find(name);
        return found == variables.end() ? nullptr : found->second.c_str();
    });
}

void RequirePosixSource() {
    using Locales = std::vector<std::string>;
    Require(Posix({}).empty(), "nothing set");
    Require(Posix({{"LANG", ""}, {"LC_ALL", ""}}).empty(), "all empty");
    Require(Posix({{"LANG", "de_DE.UTF-8"}}) == Locales{"de_DE.UTF-8"}, "LANG alone");
    Require(Posix({{"LC_MESSAGES", "fr_FR.UTF-8"}, {"LANG", "de_DE.UTF-8"}}) == Locales{"fr_FR.UTF-8"}, "LC_MESSAGES over LANG");
    Require(Posix({{"LC_ALL", "it_IT.UTF-8"}, {"LC_MESSAGES", "fr_FR.UTF-8"}, {"LANG", "de_DE.UTF-8"}}) == Locales{"it_IT.UTF-8"}, "LC_ALL over the rest");
    Require(Posix({{"LC_ALL", ""}, {"LC_MESSAGES", ""}, {"LANG", "nl_NL"}}) == Locales{"nl_NL"}, "empty values are skipped");
    Require(Posix({{"LC_ALL", ""}, {"LC_MESSAGES", "es_ES"}, {"LANG", "nl_NL"}}) == Locales{"es_ES"}, "empty LC_ALL is skipped");
    Require(Posix({{"LC_CTYPE", "ja_JP.UTF-8"}, {"LC_TIME", "ko_KR"}}).empty(), "other categories are not read");
    Require(Posix({{"LANG", "de_DE.UTF-8"}, {"LANGUAGE", "es:fr"}}) == Locales{"es", "fr", "de_DE.UTF-8"}, "LANGUAGE precedes a real locale");
    Require(Posix({{"LANG", "de_DE.UTF-8"}, {"LANGUAGE", ":pt_BR::en:"}}) == Locales{"pt_BR", "en", "de_DE.UTF-8"}, "empty LANGUAGE entries are skipped");
    Require(Posix({{"LANG", "de_DE.UTF-8"}, {"LANGUAGE", ""}}) == Locales{"de_DE.UTF-8"}, "empty LANGUAGE");
    Require(Posix({{"LANG", "C"}, {"LANGUAGE", "es"}}) == Locales{"C"}, "LANGUAGE is ignored under C");
    Require(Posix({{"LC_ALL", "POSIX"}, {"LANG", "de_DE"}, {"LANGUAGE", "es"}}) == Locales{"POSIX"}, "LANGUAGE is ignored under POSIX");
    Require(Posix({{"LANG", "C.UTF-8"}, {"LANGUAGE", "es"}}) == Locales{"C.UTF-8"}, "LANGUAGE is ignored under C.UTF-8");
    Require(Posix({{"LANGUAGE", "es"}}).empty(), "LANGUAGE alone is ignored");
    Require(Posix({{"LC_ALL", "C"}, {"LANG", "de_DE"}}) == Locales{"C"}, "LC_ALL=C overrides LANG");
}

int Resolve(const std::vector<std::string>& locales, std::vector<std::string>& notices) {
    return SystemServiceLocale::ResolveHostLanguage([&locales] { return locales; }, [&notices](const std::string& line) { notices.push_back(line); });
}

void RequireHostResolution() {
    using namespace SystemServiceLocale;
    std::vector<std::string> notices;
    Require(Resolve({"de_DE.UTF-8"}, notices) == German && notices.empty(), "one recognised locale");
    Require(Resolve({"pt_BR"}, notices) == PortugueseBrazil && notices.empty(), "pt_BR");
    Require(Resolve({"lt", "uk", "fr"}, notices) == Ukrainian && notices.empty(), "first recognised entry wins");
    Require(Resolve({"es", "fr", "de_DE.UTF-8"}, notices) == Spanish && notices.empty(), "LANGUAGE order");
    Require(Resolve({"en-GB", "fr"}, notices) == EnglishUk && notices.empty(), "first entry before later ones");
    Require(Resolve({"zh-Hant-TW"}, notices) == ChineseTraditional && notices.empty(), "zh-Hant-TW");
    Require(Resolve({"lt_LT.UTF-8"}, notices) == EnglishUs && notices.size() == 1, "unlisted language");
    Require(notices[0] == "ANYPS5_LANGUAGE=auto: the host locale \"lt_LT.UTF-8\" has no console language, using English (United States), language number 1", notices[0]);
    notices.clear();
    Require(Resolve({"C"}, notices) == EnglishUs && notices.size() == 1, "C");
    Require(notices[0].find("\"C\"") != std::string::npos && notices[0].find("English (United States)") != std::string::npos, notices[0]);
    notices.clear();
    Require(Resolve({"lt", "lv"}, notices) == EnglishUs && notices.size() == 1 && notices[0].find("\"lt\"") != std::string::npos, "first unrecognised entry is named");
    notices.clear();
    Require(Resolve({}, notices) == EnglishUs && notices.size() == 1, "no locale");
    Require(notices[0] == "ANYPS5_LANGUAGE=auto: the host reports no locale, using English (United States), language number 1", notices[0]);
    notices.clear();
    Require(Resolve({"garbage!", "ja_JP"}, notices) == Japanese && notices.empty(), "malformed entry is skipped");
}

}

int main() {
    RequireBcp47Parsing();
    RequirePosixNormalisation();
    RequireEitherForm();
    RequireLatinEuropeanRows();
    RequireIberianRows();
    RequireChineseRows();
    RequireJapaneseKoreanRows();
    RequireSlavicNordicRows();
    RequireOtherRows();
    RequireFallbackRules();
    RequirePosixSource();
    RequireHostResolution();
}
