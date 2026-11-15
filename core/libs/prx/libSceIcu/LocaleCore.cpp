// Derived from icu4c/source/common/uloc.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the LANGUAGES_3 and COUNTRIES_3 code pairs below.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uloc.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libc/include/General.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t kIllegalArgument = 1;
constexpr std::int32_t kBufferOverflow = 15;
constexpr std::int32_t kStringNotTerminated = -124;

constexpr std::array<std::pair<std::string_view, std::string_view>, 185> Languages{{
    {"aar", "aa"}, {"abk", "ab"}, {"afr", "af"}, {"aka", "ak"}, {"amh", "am"}, {"ara", "ar"}, {"arg", "an"},
    {"asm", "as"}, {"ava", "av"}, {"ave", "ae"}, {"aym", "ay"}, {"aze", "az"}, {"bak", "ba"}, {"bam", "bm"},
    {"bel", "be"}, {"ben", "bn"}, {"bis", "bi"}, {"bod", "bo"}, {"bos", "bs"}, {"bre", "br"}, {"bul", "bg"},
    {"cat", "ca"}, {"ces", "cs"}, {"cha", "ch"}, {"che", "ce"}, {"chu", "cu"}, {"chv", "cv"}, {"cor", "kw"},
    {"cos", "co"}, {"cre", "cr"}, {"cym", "cy"}, {"dan", "da"}, {"deu", "de"}, {"div", "dv"}, {"dzo", "dz"},
    {"ell", "el"}, {"eng", "en"}, {"epo", "eo"}, {"est", "et"}, {"eus", "eu"}, {"ewe", "ee"}, {"fao", "fo"},
    {"fas", "fa"}, {"fij", "fj"}, {"fin", "fi"}, {"fra", "fr"}, {"fry", "fy"}, {"ful", "ff"}, {"gla", "gd"},
    {"gle", "ga"}, {"glg", "gl"}, {"glv", "gv"}, {"grn", "gn"}, {"guj", "gu"}, {"hat", "ht"}, {"hau", "ha"},
    {"heb", "he"}, {"her", "hz"}, {"hin", "hi"}, {"hmo", "ho"}, {"hrv", "hr"}, {"hun", "hu"}, {"hye", "hy"},
    {"ibo", "ig"}, {"ido", "io"}, {"iii", "ii"}, {"iku", "iu"}, {"ile", "ie"}, {"ina", "ia"}, {"ind", "id"},
    {"ipk", "ik"}, {"isl", "is"}, {"ita", "it"}, {"jav", "jv"}, {"jaw", "jw"}, {"jpn", "ja"}, {"kal", "kl"},
    {"kan", "kn"}, {"kas", "ks"}, {"kat", "ka"}, {"kau", "kr"}, {"kaz", "kk"}, {"khm", "km"}, {"kik", "ki"},
    {"kin", "rw"}, {"kir", "ky"}, {"kom", "kv"}, {"kon", "kg"}, {"kor", "ko"}, {"kua", "kj"}, {"kur", "ku"},
    {"lao", "lo"}, {"lat", "la"}, {"lav", "lv"}, {"lim", "li"}, {"lin", "ln"}, {"lit", "lt"}, {"ltz", "lb"},
    {"lub", "lu"}, {"lug", "lg"}, {"mah", "mh"}, {"mal", "ml"}, {"mar", "mr"}, {"mkd", "mk"}, {"mlg", "mg"},
    {"mlt", "mt"}, {"mol", "mo"}, {"mon", "mn"}, {"mri", "mi"}, {"msa", "ms"}, {"mya", "my"}, {"nau", "na"},
    {"nav", "nv"}, {"nbl", "nr"}, {"nde", "nd"}, {"ndo", "ng"}, {"nep", "ne"}, {"nld", "nl"}, {"nno", "nn"},
    {"nob", "nb"}, {"nor", "no"}, {"nya", "ny"}, {"oci", "oc"}, {"oji", "oj"}, {"ori", "or"}, {"orm", "om"},
    {"oss", "os"}, {"pan", "pa"}, {"pli", "pi"}, {"pol", "pl"}, {"por", "pt"}, {"pus", "ps"}, {"que", "qu"},
    {"roh", "rm"}, {"ron", "ro"}, {"run", "rn"}, {"rus", "ru"}, {"sag", "sg"}, {"san", "sa"}, {"sin", "si"},
    {"slk", "sk"}, {"slv", "sl"}, {"sme", "se"}, {"smo", "sm"}, {"sna", "sn"}, {"snd", "sd"}, {"som", "so"},
    {"sot", "st"}, {"spa", "es"}, {"sqi", "sq"}, {"srd", "sc"}, {"srp", "sr"}, {"ssw", "ss"}, {"sun", "su"},
    {"swa", "sw"}, {"swe", "sv"}, {"tah", "ty"}, {"tam", "ta"}, {"tat", "tt"}, {"tel", "te"}, {"tgk", "tg"},
    {"tgl", "tl"}, {"tha", "th"}, {"tir", "ti"}, {"ton", "to"}, {"tsn", "tn"}, {"tso", "ts"}, {"tuk", "tk"},
    {"tur", "tr"}, {"twi", "tw"}, {"uig", "ug"}, {"ukr", "uk"}, {"urd", "ur"}, {"uzb", "uz"}, {"ven", "ve"},
    {"vie", "vi"}, {"vol", "vo"}, {"wln", "wa"}, {"wol", "wo"}, {"xho", "xh"}, {"yid", "yi"}, {"yor", "yo"},
    {"zha", "za"}, {"zho", "zh"}, {"zul", "zu"}
}};

constexpr std::array<std::pair<std::string_view, std::string_view>, 259> Regions{{
    {"ABW", "AW"}, {"AFG", "AF"}, {"AGO", "AO"}, {"AIA", "AI"}, {"ALA", "AX"}, {"ALB", "AL"}, {"AND", "AD"},
    {"ANT", "AN"}, {"ARE", "AE"}, {"ARG", "AR"}, {"ARM", "AM"}, {"ASM", "AS"}, {"ATA", "AQ"}, {"ATF", "TF"},
    {"ATG", "AG"}, {"AUS", "AU"}, {"AUT", "AT"}, {"AZE", "AZ"}, {"BDI", "BI"}, {"BEL", "BE"}, {"BEN", "BJ"},
    {"BES", "BQ"}, {"BFA", "BF"}, {"BGD", "BD"}, {"BGR", "BG"}, {"BHR", "BH"}, {"BHS", "BS"}, {"BIH", "BA"},
    {"BLM", "BL"}, {"BLR", "BY"}, {"BLZ", "BZ"}, {"BMU", "BM"}, {"BOL", "BO"}, {"BRA", "BR"}, {"BRB", "BB"},
    {"BRN", "BN"}, {"BTN", "BT"}, {"BUR", "BU"}, {"BVT", "BV"}, {"BWA", "BW"}, {"CAF", "CF"}, {"CAN", "CA"},
    {"CCK", "CC"}, {"CHE", "CH"}, {"CHL", "CL"}, {"CHN", "CN"}, {"CIV", "CI"}, {"CMR", "CM"}, {"COD", "CD"},
    {"COG", "CG"}, {"COK", "CK"}, {"COL", "CO"}, {"COM", "KM"}, {"CPV", "CV"}, {"CRI", "CR"}, {"CUB", "CU"},
    {"CUW", "CW"}, {"CXR", "CX"}, {"CYM", "KY"}, {"CYP", "CY"}, {"CZE", "CZ"}, {"DEU", "DE"}, {"DJI", "DJ"},
    {"DMA", "DM"}, {"DNK", "DK"}, {"DOM", "DO"}, {"DZA", "DZ"}, {"ECU", "EC"}, {"EGY", "EG"}, {"ERI", "ER"},
    {"ESH", "EH"}, {"ESP", "ES"}, {"EST", "EE"}, {"ETH", "ET"}, {"FIN", "FI"}, {"FJI", "FJ"}, {"FLK", "FK"},
    {"FRA", "FR"}, {"FRO", "FO"}, {"FSM", "FM"}, {"FXX", "FX"}, {"GAB", "GA"}, {"GBR", "GB"}, {"GEO", "GE"},
    {"GGY", "GG"}, {"GHA", "GH"}, {"GIB", "GI"}, {"GIN", "GN"}, {"GLP", "GP"}, {"GMB", "GM"}, {"GNB", "GW"},
    {"GNQ", "GQ"}, {"GRC", "GR"}, {"GRD", "GD"}, {"GRL", "GL"}, {"GTM", "GT"}, {"GUF", "GF"}, {"GUM", "GU"},
    {"GUY", "GY"}, {"HKG", "HK"}, {"HMD", "HM"}, {"HND", "HN"}, {"HRV", "HR"}, {"HTI", "HT"}, {"HUN", "HU"},
    {"IDN", "ID"}, {"IMN", "IM"}, {"IND", "IN"}, {"IOT", "IO"}, {"IRL", "IE"}, {"IRN", "IR"}, {"IRQ", "IQ"},
    {"ISL", "IS"}, {"ISR", "IL"}, {"ITA", "IT"}, {"JAM", "JM"}, {"JEY", "JE"}, {"JOR", "JO"}, {"JPN", "JP"},
    {"KAZ", "KZ"}, {"KEN", "KE"}, {"KGZ", "KG"}, {"KHM", "KH"}, {"KIR", "KI"}, {"KNA", "KN"}, {"KOR", "KR"},
    {"KWT", "KW"}, {"LAO", "LA"}, {"LBN", "LB"}, {"LBR", "LR"}, {"LBY", "LY"}, {"LCA", "LC"}, {"LIE", "LI"},
    {"LKA", "LK"}, {"LSO", "LS"}, {"LTU", "LT"}, {"LUX", "LU"}, {"LVA", "LV"}, {"MAC", "MO"}, {"MAF", "MF"},
    {"MAR", "MA"}, {"MCO", "MC"}, {"MDA", "MD"}, {"MDG", "MG"}, {"MDV", "MV"}, {"MEX", "MX"}, {"MHL", "MH"},
    {"MKD", "MK"}, {"MLI", "ML"}, {"MLT", "MT"}, {"MMR", "MM"}, {"MNE", "ME"}, {"MNG", "MN"}, {"MNP", "MP"},
    {"MOZ", "MZ"}, {"MRT", "MR"}, {"MSR", "MS"}, {"MTQ", "MQ"}, {"MUS", "MU"}, {"MWI", "MW"}, {"MYS", "MY"},
    {"MYT", "YT"}, {"NAM", "NA"}, {"NCL", "NC"}, {"NER", "NE"}, {"NFK", "NF"}, {"NGA", "NG"}, {"NIC", "NI"},
    {"NIU", "NU"}, {"NLD", "NL"}, {"NOR", "NO"}, {"NPL", "NP"}, {"NRU", "NR"}, {"NZL", "NZ"}, {"OMN", "OM"},
    {"PAK", "PK"}, {"PAN", "PA"}, {"PCN", "PN"}, {"PER", "PE"}, {"PHL", "PH"}, {"PLW", "PW"}, {"PNG", "PG"},
    {"POL", "PL"}, {"PRI", "PR"}, {"PRK", "KP"}, {"PRT", "PT"}, {"PRY", "PY"}, {"PSE", "PS"}, {"PYF", "PF"},
    {"QAT", "QA"}, {"REU", "RE"}, {"ROM", "RO"}, {"ROU", "RO"}, {"RUS", "RU"}, {"RWA", "RW"}, {"SAU", "SA"},
    {"SCG", "CS"}, {"SDN", "SD"}, {"SEN", "SN"}, {"SGP", "SG"}, {"SGS", "GS"}, {"SHN", "SH"}, {"SJM", "SJ"},
    {"SLB", "SB"}, {"SLE", "SL"}, {"SLV", "SV"}, {"SMR", "SM"}, {"SOM", "SO"}, {"SPM", "PM"}, {"SRB", "RS"},
    {"SSD", "SS"}, {"STP", "ST"}, {"SUN", "SU"}, {"SUR", "SR"}, {"SVK", "SK"}, {"SVN", "SI"}, {"SWE", "SE"},
    {"SWZ", "SZ"}, {"SXM", "SX"}, {"SYC", "SC"}, {"SYR", "SY"}, {"TCA", "TC"}, {"TCD", "TD"}, {"TGO", "TG"},
    {"THA", "TH"}, {"TJK", "TJ"}, {"TKL", "TK"}, {"TKM", "TM"}, {"TLS", "TL"}, {"TMP", "TP"}, {"TON", "TO"},
    {"TTO", "TT"}, {"TUN", "TN"}, {"TUR", "TR"}, {"TUV", "TV"}, {"TWN", "TW"}, {"TZA", "TZ"}, {"UGA", "UG"},
    {"UKR", "UA"}, {"UMI", "UM"}, {"URY", "UY"}, {"USA", "US"}, {"UZB", "UZ"}, {"VAT", "VA"}, {"VCT", "VC"},
    {"VEN", "VE"}, {"VGB", "VG"}, {"VIR", "VI"}, {"VNM", "VN"}, {"VUT", "VU"}, {"WLF", "WF"}, {"WSM", "WS"},
    {"YEM", "YE"}, {"YMD", "YD"}, {"YUG", "YU"}, {"ZAF", "ZA"}, {"ZAR", "ZR"}, {"ZMB", "ZM"}, {"ZWE", "ZW"}
}};

char Lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

char Upper(char value) {
    return value >= 'a' && value <= 'z' ? static_cast<char>(value - ('a' - 'A')) : value;
}

bool Alpha(char value) {
    const char lower = Lower(value);
    return lower >= 'a' && lower <= 'z';
}

bool Digit(char value) {
    return value >= '0' && value <= '9';
}

std::string ChangeCase(std::string_view value, bool upper) {
    std::string result(value);
    for (char& part : result) part = upper ? Upper(part) : Lower(part);
    return result;
}

bool IsScript(std::string_view value) {
    return value.size() == 4 && std::all_of(value.begin(), value.end(), Alpha);
}

template<std::size_t TSize>
std::string ShortCode(const std::array<std::pair<std::string_view, std::string_view>, TSize>& codes,
                      const std::string& value) {
    const auto found = std::lower_bound(codes.begin(), codes.end(), value,
                                        [](const auto& entry, const std::string& key) { return entry.first < key; });
    return found != codes.end() && found->first == value ? std::string(found->second) : value;
}

std::string Region(std::string_view value) {
    return ShortCode(Regions, ChangeCase(value, true));
}

bool IsRegion(std::string_view value) {
    if (value.size() == 2 || value.size() == 3)
        return std::all_of(value.begin(), value.end(), Alpha) ||
               (value.size() == 3 && std::all_of(value.begin(), value.end(), Digit));
    return false;
}

struct Parts {
    std::string language;
    std::string script;
    std::string country;
    std::string variant;
    std::string encoding;
    std::string keywords;
};

Parts Parse(const char* locale) {
    const std::string_view input(locale);
    const auto keyword = input.find('@');
    const std::string_view head = input.substr(0, keyword);
    Parts parts;
    if (keyword != std::string_view::npos) parts.keywords = std::string(input.substr(keyword + 1));

    const auto encoding = head.find('.');
    const std::string_view base = head.substr(0, encoding);
    if (encoding != std::string_view::npos) parts.encoding = std::string(head.substr(encoding));
    const std::string special = ChangeCase(base, false);
    if (special == "i-klingon" || special == "x-private") {
        parts.language = special;
        return parts;
    }

    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (start <= base.size()) {
        const auto end = base.find_first_of("_-", start);
        tokens.emplace_back(base.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }

    if (!tokens.empty()) parts.language = ChangeCase(tokens.front(), false);
    if (parts.language == "und" || parts.language == "root") parts.language.clear();
    if (parts.language.size() == 3) parts.language = ShortCode(Languages, parts.language);
    std::size_t index = 1;
    if (index < tokens.size() && IsScript(tokens[index])) {
        parts.script = ChangeCase(tokens[index], false);
        parts.script.front() = Upper(parts.script.front());
        ++index;
    }
    if (index < tokens.size() && IsRegion(tokens[index])) {
        parts.country = Region(tokens[index]);
        ++index;
    } else if (index < tokens.size() && tokens[index].empty()) {
        ++index;
        if (index < tokens.size() && IsRegion(tokens[index])) {
            parts.country = Region(tokens[index]);
            ++index;
        } else if (index < tokens.size()) {
            parts.variant += "_";
        }
    }
    for (; index < tokens.size(); ++index) {
        if (tokens[index].empty()) continue;
        if (!parts.variant.empty() && parts.variant.back() != '_') parts.variant.push_back('_');
        parts.variant += ChangeCase(tokens[index], true);
    }
    return parts;
}

std::string BaseName(const Parts& parts) {
    if (parts.language == "i-klingon") return "tlh";
    if (parts.language == "x-private") return "";
    std::string result = parts.language;
    if (!parts.script.empty()) result += "_" + parts.script;
    if (!parts.country.empty()) result += "_" + parts.country;
    if (!parts.variant.empty()) {
        if (parts.country.empty()) result += "_";
        result += parts.variant.front() == '_' ? parts.variant : "_" + parts.variant;
    }
    return result + parts.encoding;
}

std::int32_t Write(std::string_view value, char* destination, std::int32_t capacity,
                   std::int32_t* status, bool fullOnly) {
    if (!status || *status > 0) return 0;
    if (capacity < 0 || (capacity > 0 && !destination)) {
        *status = kIllegalArgument;
        return 0;
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        *status = kIllegalArgument;
        return 0;
    }
    const auto size = static_cast<std::int32_t>(value.size());
    if (size > 0 && destination && capacity > 0 && (!fullOnly || capacity >= size)) {
        std::memcpy(destination, value.data(), static_cast<std::size_t>(std::min(capacity, size)));
    }
    if (capacity > size && destination) destination[size] = '\0';
    else if (capacity == size) *status = kStringNotTerminated;
    else *status = kBufferOverflow;
    return size;
}

std::int32_t Component(const char* locale, char* result, std::int32_t capacity,
                       std::int32_t* status, int which, const char* nullLocale) {
    if (!status || *status > 0) return 0;
    if (!locale) NotImplemented_nid_no_patch(nullLocale);
    Parts parts = Parse(locale);
    const std::string& value = which == 0 ? parts.language : which == 1 ? parts.country : parts.script;
    return Write(value, result, capacity, status, true);
}

}

extern "C" {

std::int32_t APS5_VABI uloc_getLanguage(const char* locale, char* result,
                                       std::int32_t capacity, std::int32_t* status) {
    return Component(locale, result, capacity, status, 0,
                     "uloc_getLanguage with a null locale, which names the default locale");
}

std::int32_t APS5_VABI uloc_getCountry(const char* locale, char* result,
                                      std::int32_t capacity, std::int32_t* status) {
    return Component(locale, result, capacity, status, 1,
                     "uloc_getCountry with a null locale, which names the default locale");
}

std::int32_t APS5_VABI uloc_getScript(const char* locale, char* result,
                                     std::int32_t capacity, std::int32_t* status) {
    return Component(locale, result, capacity, status, 2,
                     "uloc_getScript with a null locale, which names the default locale");
}

std::int32_t APS5_VABI uloc_getBaseName(const char* locale, char* result,
                                       std::int32_t capacity, std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!locale) NotImplemented_nid_no_patch("uloc_getBaseName with a null locale, which names the default locale");
    return Write(BaseName(Parse(locale)), result, capacity, status, false);
}

std::int32_t APS5_VABI uloc_getKeywordValue(const char* locale, const char* keyword,
                                           char* result, std::int32_t capacity,
                                           std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!locale || !keyword || !*keyword) {
        *status = kIllegalArgument;
        return 0;
    }
    const std::string requested = ChangeCase(keyword, false);
    const std::string values = Parse(locale).keywords;
    std::size_t start = 0;
    while (start < values.size()) {
        const auto end = values.find(';', start);
        const auto item = std::string_view(values).substr(
            start, end == std::string::npos ? end : end - start);
        const auto equal = item.find('=');
        if (equal == std::string_view::npos) {
            *status = kIllegalArgument;
            return 0;
        }
        if (ChangeCase(item.substr(0, equal), false) == requested) {
            if (equal + 1 == item.size()) {
                *status = kIllegalArgument;
                return 0;
            }
            return Write(item.substr(equal + 1), result, capacity, status, false);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return Write("", result, capacity, status, false);
}

std::int32_t APS5_VABI uloc_canonicalize(const char* locale, char* result,
                                        std::int32_t capacity, std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!locale) NotImplemented_nid_no_patch("uloc_canonicalize with a null locale, which names the default locale");
    Parts parts = Parse(locale);
    parts.encoding.clear();
    std::string canonical = parts.language == "x-private" ? "@x=private" : BaseName(parts);
    if (!parts.keywords.empty()) {
        std::vector<std::string> keywords;
        std::size_t start = 0;
        while (start < parts.keywords.size()) {
            const auto end = parts.keywords.find(';', start);
            const std::string item = parts.keywords.substr(
                start, end == std::string::npos ? end : end - start);
            if (!item.empty()) keywords.push_back(item);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        std::sort(keywords.begin(), keywords.end());
        for (const auto& keyword : keywords)
            canonical += std::string(canonical.find('@') == std::string::npos ? "@" : ";") + keyword;
    }
    return Write(canonical, result, capacity, status, false);
}

}
