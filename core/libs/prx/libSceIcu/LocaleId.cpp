// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace {

const char* const Languages[]{
    "aa", "ab", "ace", "ach", "ada", "ady", "ae", "aeb", "af", "afh",
    "agq", "ain", "ak", "akk", "akz", "ale", "aln", "alt", "am", "an",
    "ang", "anp", "ar", "arc", "arn", "aro", "arp", "arq", "ars", "arw",
    "ary", "arz", "as", "asa", "ase", "ast", "av", "avk", "awa", "ay",
    "az", "ba", "bal", "ban", "bar", "bas", "bax", "bbc", "bbj", "be",
    "bej", "bem", "bew", "bez", "bfd", "bfq", "bg", "bgn", "bho", "bi",
    "bik", "bin", "bjn", "bkm", "bla", "bm", "bn", "bo", "bpy", "bqi",
    "br", "bra", "brh", "brx", "bs", "bss", "bua", "bug", "bum", "byn",
    "byv", "ca", "cad", "car", "cay", "cch", "ccp", "ce", "ceb", "cgg",
    "ch", "chb", "chg", "chk", "chm", "chn", "cho", "chp", "chr", "chy",
    "ckb", "co", "cop", "cps", "cr", "crh", "cs", "csb", "cu", "cv",
    "cy", "da", "dak", "dar", "dav", "de", "del", "den", "dgr", "din",
    "dje", "doi", "dsb", "dtp", "dua", "dum", "dv", "dyo", "dyu", "dz",
    "dzg", "ebu", "ee", "efi", "egl", "egy", "eka", "el", "elx", "en",
    "enm", "eo", "es", "esu", "et", "eu", "ewo", "ext", "fa", "fan",
    "fat", "ff", "fi", "fil", "fit", "fj", "fo", "fon", "fr", "frc",
    "frm", "fro", "frp", "frr", "frs", "fur", "fy", "ga", "gaa", "gag",
    "gan", "gay", "gba", "gbz", "gd", "gez", "gil", "gl", "glk", "gmh",
    "gn", "goh", "gom", "gon", "gor", "got", "grb", "grc", "gsw", "gu",
    "guc", "gur", "guz", "gv", "gwi", "ha", "hai", "hak", "haw", "he",
    "hi", "hif", "hil", "hit", "hmn", "ho", "hr", "hsb", "hsn", "ht",
    "hu", "hup", "hy", "hz", "ia", "iba", "ibb", "id", "ie", "ig",
    "ii", "ik", "ilo", "inh", "io", "is", "it", "iu", "izh", "ja",
    "jam", "jbo", "jgo", "jmc", "jpr", "jrb", "jut", "jv", "ka", "kaa",
    "kab", "kac", "kaj", "kam", "kaw", "kbd", "kbl", "kcg", "kde", "kea",
    "ken", "kfo", "kg", "kgp", "kha", "kho", "khq", "khw", "ki", "kiu",
    "kj", "kk", "kkj", "kl", "kln", "km", "kmb", "kn", "ko", "koi",
    "kok", "kos", "kpe", "kr", "krc", "kri", "krj", "krl", "kru", "ks",
    "ksb", "ksf", "ksh", "ku", "kum", "kut", "kv", "kw", "ky", "la",
    "lad", "lag", "lah", "lam", "lb", "lez", "lfn", "lg", "li", "lij",
    "liv", "lkt", "lmo", "ln", "lo", "lol", "loz", "lrc", "lt", "ltg",
    "lu", "lua", "lui", "lun", "luo", "lus", "luy", "lv", "lzh", "lzz",
    "mad", "maf", "mag", "mai", "mak", "man", "mas", "mde", "mdf", "mdh",
    "mdr", "men", "mer", "mfe", "mg", "mga", "mgh", "mgo", "mh", "mi",
    "mic", "min", "mis", "mk", "ml", "mn", "mnc", "mni", "mo", "moh",
    "mos", "mr", "mrj", "ms", "mt", "mua", "mul", "mus", "mwl", "mwr",
    "mwv", "my", "mye", "myv", "mzn", "na", "nan", "nap", "naq", "nb",
    "nd", "nds", "ne", "new", "ng", "nia", "niu", "njo", "nl", "nmg",
    "nn", "nnh", "no", "nog", "non", "nov", "nqo", "nr", "nso", "nus",
    "nv", "nwc", "ny", "nym", "nyn", "nyo", "nzi", "oc", "oj", "om",
    "or", "os", "osa", "ota", "pa", "pag", "pal", "pam", "pap", "pau",
    "pcd", "pcm", "pdc", "pdt", "peo", "pfl", "phn", "pi", "pl", "pms",
    "pnt", "pon", "prg", "pro", "ps", "pt", "qu", "quc", "qug", "raj",
    "rap", "rar", "rgn", "rif", "rm", "rn", "ro", "rof", "rom", "rtm",
    "ru", "rue", "rug", "rup", "rw", "rwk", "sa", "sad", "sah", "sam",
    "saq", "sas", "sat", "saz", "sba", "sbp", "sc", "scn", "sco", "sd",
    "sdc", "sdh", "se", "see", "seh", "sei", "sel", "ses", "sg", "sga",
    "sgs", "shi", "shn", "shu", "si", "sid", "sk", "sl", "sli", "sly",
    "sm", "sma", "smj", "smn", "sms", "sn", "snk", "so", "sog", "sq",
    "sr", "srn", "srr", "ss", "ssy", "st", "stq", "su", "suk", "sus",
    "sux", "sv", "sw", "swb", "swc", "syc", "syr", "szl", "ta", "tcy",
    "te", "tem", "teo", "ter", "tet", "tg", "th", "ti", "tig", "tiv",
    "tk", "tkl", "tkr", "tl", "tlh", "tli", "tly", "tmh", "tn", "to",
    "tog", "tpi", "tr", "tru", "trv", "ts", "tsd", "tsi", "tt", "ttt",
    "tum", "tvl", "tw", "twq", "ty", "tyv", "tzm", "udm", "ug", "uga",
    "uk", "umb", "und", "ur", "uz", "vai", "ve", "vec", "vep", "vi",
    "vls", "vmf", "vo", "vot", "vro", "vun", "wa", "wae", "wal", "war",
    "was", "wbp", "wo", "wuu", "xal", "xh", "xmf", "xog", "yao", "yap",
    "yav", "ybb", "yi", "yo", "yrl", "yue", "za", "zap", "zbl", "zea",
    "zen", "zgh", "zh", "zu", "zun", "zxx", "zza", nullptr, "in", "iw",
    "ji", "jw", "sh", nullptr,
};

const char* const LanguageTriples[]{
    "aar", "abk", "ace", "ach", "ada", "ady", "ave", "aeb", "afr", "afh",
    "agq", "ain", "aka", "akk", "akz", "ale", "aln", "alt", "amh", "arg",
    "ang", "anp", "ara", "arc", "arn", "aro", "arp", "arq", "ars", "arw",
    "ary", "arz", "asm", "asa", "ase", "ast", "ava", "avk", "awa", "aym",
    "aze", "bak", "bal", "ban", "bar", "bas", "bax", "bbc", "bbj", "bel",
    "bej", "bem", "bew", "bez", "bfd", "bfq", "bul", "bgn", "bho", "bis",
    "bik", "bin", "bjn", "bkm", "bla", "bam", "ben", "bod", "bpy", "bqi",
    "bre", "bra", "brh", "brx", "bos", "bss", "bua", "bug", "bum", "byn",
    "byv", "cat", "cad", "car", "cay", "cch", "ccp", "che", "ceb", "cgg",
    "cha", "chb", "chg", "chk", "chm", "chn", "cho", "chp", "chr", "chy",
    "ckb", "cos", "cop", "cps", "cre", "crh", "ces", "csb", "chu", "chv",
    "cym", "dan", "dak", "dar", "dav", "deu", "del", "den", "dgr", "din",
    "dje", "doi", "dsb", "dtp", "dua", "dum", "div", "dyo", "dyu", "dzo",
    "dzg", "ebu", "ewe", "efi", "egl", "egy", "eka", "ell", "elx", "eng",
    "enm", "epo", "spa", "esu", "est", "eus", "ewo", "ext", "fas", "fan",
    "fat", "ful", "fin", "fil", "fit", "fij", "fao", "fon", "fra", "frc",
    "frm", "fro", "frp", "frr", "frs", "fur", "fry", "gle", "gaa", "gag",
    "gan", "gay", "gba", "gbz", "gla", "gez", "gil", "glg", "glk", "gmh",
    "grn", "goh", "gom", "gon", "gor", "got", "grb", "grc", "gsw", "guj",
    "guc", "gur", "guz", "glv", "gwi", "hau", "hai", "hak", "haw", "heb",
    "hin", "hif", "hil", "hit", "hmn", "hmo", "hrv", "hsb", "hsn", "hat",
    "hun", "hup", "hye", "her", "ina", "iba", "ibb", "ind", "ile", "ibo",
    "iii", "ipk", "ilo", "inh", "ido", "isl", "ita", "iku", "izh", "jpn",
    "jam", "jbo", "jgo", "jmc", "jpr", "jrb", "jut", "jav", "kat", "kaa",
    "kab", "kac", "kaj", "kam", "kaw", "kbd", "kbl", "kcg", "kde", "kea",
    "ken", "kfo", "kon", "kgp", "kha", "kho", "khq", "khw", "kik", "kiu",
    "kua", "kaz", "kkj", "kal", "kln", "khm", "kmb", "kan", "kor", "koi",
    "kok", "kos", "kpe", "kau", "krc", "kri", "krj", "krl", "kru", "kas",
    "ksb", "ksf", "ksh", "kur", "kum", "kut", "kom", "cor", "kir", "lat",
    "lad", "lag", "lah", "lam", "ltz", "lez", "lfn", "lug", "lim", "lij",
    "liv", "lkt", "lmo", "lin", "lao", "lol", "loz", "lrc", "lit", "ltg",
    "lub", "lua", "lui", "lun", "luo", "lus", "luy", "lav", "lzh", "lzz",
    "mad", "maf", "mag", "mai", "mak", "man", "mas", "mde", "mdf", "mdh",
    "mdr", "men", "mer", "mfe", "mlg", "mga", "mgh", "mgo", "mah", "mri",
    "mic", "min", "mis", "mkd", "mal", "mon", "mnc", "mni", "mol", "moh",
    "mos", "mar", "mrj", "msa", "mlt", "mua", "mul", "mus", "mwl", "mwr",
    "mwv", "mya", "mye", "myv", "mzn", "nau", "nan", "nap", "naq", "nob",
    "nde", "nds", "nep", "new", "ndo", "nia", "niu", "njo", "nld", "nmg",
    "nno", "nnh", "nor", "nog", "non", "nov", "nqo", "nbl", "nso", "nus",
    "nav", "nwc", "nya", "nym", "nyn", "nyo", "nzi", "oci", "oji", "orm",
    "ori", "oss", "osa", "ota", "pan", "pag", "pal", "pam", "pap", "pau",
    "pcd", "pcm", "pdc", "pdt", "peo", "pfl", "phn", "pli", "pol", "pms",
    "pnt", "pon", "prg", "pro", "pus", "por", "que", "quc", "qug", "raj",
    "rap", "rar", "rgn", "rif", "roh", "run", "ron", "rof", "rom", "rtm",
    "rus", "rue", "rug", "rup", "kin", "rwk", "san", "sad", "sah", "sam",
    "saq", "sas", "sat", "saz", "sba", "sbp", "srd", "scn", "sco", "snd",
    "sdc", "sdh", "sme", "see", "seh", "sei", "sel", "ses", "sag", "sga",
    "sgs", "shi", "shn", "shu", "sin", "sid", "slk", "slv", "sli", "sly",
    "smo", "sma", "smj", "smn", "sms", "sna", "snk", "som", "sog", "sqi",
    "srp", "srn", "srr", "ssw", "ssy", "sot", "stq", "sun", "suk", "sus",
    "sux", "swe", "swa", "swb", "swc", "syc", "syr", "szl", "tam", "tcy",
    "tel", "tem", "teo", "ter", "tet", "tgk", "tha", "tir", "tig", "tiv",
    "tuk", "tkl", "tkr", "tgl", "tlh", "tli", "tly", "tmh", "tsn", "ton",
    "tog", "tpi", "tur", "tru", "trv", "tso", "tsd", "tsi", "tat", "ttt",
    "tum", "tvl", "twi", "twq", "tah", "tyv", "tzm", "udm", "uig", "uga",
    "ukr", "umb", "und", "urd", "uzb", "vai", "ven", "vec", "vep", "vie",
    "vls", "vmf", "vol", "vot", "vro", "vun", "wln", "wae", "wal", "war",
    "was", "wbp", "wol", "wuu", "xal", "xho", "xmf", "xog", "yao", "yap",
    "yav", "ybb", "yid", "yor", "yrl", "yue", "zha", "zap", "zbl", "zea",
    "zen", "zgh", "zho", "zul", "zun", "zxx", "zza", nullptr, "ind", "heb",
    "yid", "jaw", "srp", nullptr,
};

const char* const Countries[]{
    "AD", "AE", "AF", "AG", "AI", "AL", "AM", "AO", "AQ", "AR",
    "AS", "AT", "AU", "AW", "AX", "AZ", "BA", "BB", "BD", "BE",
    "BF", "BG", "BH", "BI", "BJ", "BL", "BM", "BN", "BO", "BQ",
    "BR", "BS", "BT", "BV", "BW", "BY", "BZ", "CA", "CC", "CD",
    "CF", "CG", "CH", "CI", "CK", "CL", "CM", "CN", "CO", "CR",
    "CU", "CV", "CW", "CX", "CY", "CZ", "DE", "DJ", "DK", "DM",
    "DO", "DZ", "EC", "EE", "EG", "EH", "ER", "ES", "ET", "FI",
    "FJ", "FK", "FM", "FO", "FR", "GA", "GB", "GD", "GE", "GF",
    "GG", "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GS",
    "GT", "GU", "GW", "GY", "HK", "HM", "HN", "HR", "HT", "HU",
    "ID", "IE", "IL", "IM", "IN", "IO", "IQ", "IR", "IS", "IT",
    "JE", "JM", "JO", "JP", "KE", "KG", "KH", "KI", "KM", "KN",
    "KP", "KR", "KW", "KY", "KZ", "LA", "LB", "LC", "LI", "LK",
    "LR", "LS", "LT", "LU", "LV", "LY", "MA", "MC", "MD", "ME",
    "MF", "MG", "MH", "MK", "ML", "MM", "MN", "MO", "MP", "MQ",
    "MR", "MS", "MT", "MU", "MV", "MW", "MX", "MY", "MZ", "NA",
    "NC", "NE", "NF", "NG", "NI", "NL", "NO", "NP", "NR", "NU",
    "NZ", "OM", "PA", "PE", "PF", "PG", "PH", "PK", "PL", "PM",
    "PN", "PR", "PS", "PT", "PW", "PY", "QA", "RE", "RO", "RS",
    "RU", "RW", "SA", "SB", "SC", "SD", "SE", "SG", "SH", "SI",
    "SJ", "SK", "SL", "SM", "SN", "SO", "SR", "SS", "ST", "SV",
    "SX", "SY", "SZ", "TC", "TD", "TF", "TG", "TH", "TJ", "TK",
    "TL", "TM", "TN", "TO", "TR", "TT", "TV", "TW", "TZ", "UA",
    "UG", "UM", "US", "UY", "UZ", "VA", "VC", "VE", "VG", "VI",
    "VN", "VU", "WF", "WS", "YE", "YT", "ZA", "ZM", "ZW", nullptr,
    "AN", "BU", "CS", "FX", "RO", "SU", "TP", "YD", "YU", "ZR",
    nullptr,
};

const char* const CountryTriples[]{
    "AND", "ARE", "AFG", "ATG", "AIA", "ALB", "ARM", "AGO", "ATA", "ARG",
    "ASM", "AUT", "AUS", "ABW", "ALA", "AZE", "BIH", "BRB", "BGD", "BEL",
    "BFA", "BGR", "BHR", "BDI", "BEN", "BLM", "BMU", "BRN", "BOL", "BES",
    "BRA", "BHS", "BTN", "BVT", "BWA", "BLR", "BLZ", "CAN", "CCK", "COD",
    "CAF", "COG", "CHE", "CIV", "COK", "CHL", "CMR", "CHN", "COL", "CRI",
    "CUB", "CPV", "CUW", "CXR", "CYP", "CZE", "DEU", "DJI", "DNK", "DMA",
    "DOM", "DZA", "ECU", "EST", "EGY", "ESH", "ERI", "ESP", "ETH", "FIN",
    "FJI", "FLK", "FSM", "FRO", "FRA", "GAB", "GBR", "GRD", "GEO", "GUF",
    "GGY", "GHA", "GIB", "GRL", "GMB", "GIN", "GLP", "GNQ", "GRC", "SGS",
    "GTM", "GUM", "GNB", "GUY", "HKG", "HMD", "HND", "HRV", "HTI", "HUN",
    "IDN", "IRL", "ISR", "IMN", "IND", "IOT", "IRQ", "IRN", "ISL", "ITA",
    "JEY", "JAM", "JOR", "JPN", "KEN", "KGZ", "KHM", "KIR", "COM", "KNA",
    "PRK", "KOR", "KWT", "CYM", "KAZ", "LAO", "LBN", "LCA", "LIE", "LKA",
    "LBR", "LSO", "LTU", "LUX", "LVA", "LBY", "MAR", "MCO", "MDA", "MNE",
    "MAF", "MDG", "MHL", "MKD", "MLI", "MMR", "MNG", "MAC", "MNP", "MTQ",
    "MRT", "MSR", "MLT", "MUS", "MDV", "MWI", "MEX", "MYS", "MOZ", "NAM",
    "NCL", "NER", "NFK", "NGA", "NIC", "NLD", "NOR", "NPL", "NRU", "NIU",
    "NZL", "OMN", "PAN", "PER", "PYF", "PNG", "PHL", "PAK", "POL", "SPM",
    "PCN", "PRI", "PSE", "PRT", "PLW", "PRY", "QAT", "REU", "ROU", "SRB",
    "RUS", "RWA", "SAU", "SLB", "SYC", "SDN", "SWE", "SGP", "SHN", "SVN",
    "SJM", "SVK", "SLE", "SMR", "SEN", "SOM", "SUR", "SSD", "STP", "SLV",
    "SXM", "SYR", "SWZ", "TCA", "TCD", "ATF", "TGO", "THA", "TJK", "TKL",
    "TLS", "TKM", "TUN", "TON", "TUR", "TTO", "TUV", "TWN", "TZA", "UKR",
    "UGA", "UMI", "USA", "URY", "UZB", "VAT", "VCT", "VEN", "VGB", "VIR",
    "VNM", "VUT", "WLF", "WSM", "YEM", "MYT", "ZAF", "ZMB", "ZWE", nullptr,
    "ANT", "BUR", "SCG", "FXX", "ROM", "SUN", "TMP", "YMD", "YUG", "ZAR",
    nullptr,
};

char Lower(char value) { return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value; }
char Upper(char value) { return value >= 'a' && value <= 'z' ? value - ('a' - 'A') : value; }
bool Separator(char value) { return value == '_' || value == '-'; }
bool Terminator(char value) { return !value || value == '.' || value == '@'; }
bool Letter(char value) { return Lower(value) >= 'a' && Lower(value) <= 'z'; }

int Find(const char* const* table, std::string_view key) {
    int index = 0;
    for (int pass = 0; pass < 2; ++pass, ++index)
        for (; table[index]; ++index)
            if (key == table[index]) return index;
    return -1;
}

const char* LanguageEnd(const char* text) {
    const std::string_view id(text);
    if (id.size() == 4 && Lower(text[0]) == 'r' && Lower(text[1]) == 'o' && Lower(text[2]) == 'o' && Lower(text[3]) == 't') return text + 4;
    if (id.size() >= 3 && Lower(text[0]) == 'u' && Lower(text[1]) == 'n' && Lower(text[2]) == 'd' && (!text[3] || Separator(text[3]) || text[3] == '@')) text += 3;
    if ((Lower(text[0]) == 'i' || Lower(text[0]) == 'x') && Separator(text[1])) text += 2;
    while (!Terminator(*text) && !Separator(*text)) ++text;
    return text;
}

const char* RequireId(const char* text) {
    if (!text) NotImplemented_nid_no_patch(__func__);
    return text;
}

}

extern "C" {

const char* const* APS5_VABI uloc_getISOLanguages_nid_postfix() { return Languages; }
const char* const* APS5_VABI uloc_getISOCountries_nid_postfix() { return Countries; }

const char* APS5_VABI uloc_getISO3Language_nid_postfix(const char* localeId) {
    const auto* text = RequireId(localeId);
    const auto* end = LanguageEnd(text);
    if (end - text >= 12) return "";
    char language[12]{};
    for (std::ptrdiff_t i = 0; i < end - text; ++i) language[i] = Lower(text[i]);
    if (std::string_view(language) == "root" || std::string_view(language) == "und") language[0] = 0;
    if (Separator(language[1]) && (language[0] == 'i' || language[0] == 'x')) language[1] = '-';
    auto index = Find(LanguageTriples, language);
    if (end - text == 3 && index >= 0) return LanguageTriples[index];
    index = Find(Languages, language);
    return index >= 0 ? LanguageTriples[index] : "";
}

const char* APS5_VABI uloc_getISO3Country_nid_postfix(const char* localeId) {
    const auto* text = LanguageEnd(RequireId(localeId));
    if (!Separator(*text)) return "";
    const auto* script = text + 1;
    auto* end = script;
    while (!Terminator(*end) && !Separator(*end) && Letter(*end)) ++end;
    if (end - script == 4) text = end;
    if (!Separator(*text)) return "";
    ++text;
    end = text;
    while (!Terminator(*end) && !Separator(*end)) ++end;
    if (end - text != 2 && end - text != 3) return "";
    char country[4]{};
    for (std::ptrdiff_t i = 0; i < end - text; ++i) country[i] = Upper(text[i]);
    auto index = Find(CountryTriples, country);
    if (end - text == 3 && index >= 0) return CountryTriples[index];
    index = Find(Countries, country);
    return index >= 0 ? CountryTriples[index] : "";
}

std::int32_t APS5_VABI uloc_getParent_nid_postfix(const char* localeId, char* parent, std::int32_t capacity, std::int32_t* status) {
    if (!status) throw std::invalid_argument("ICU locale parent: null status");
    if (*status > 0) return 0;
    auto* text = RequireId(localeId);
    const auto* last = std::strrchr(text, '_');
    auto length = last ? Icu::Abi::CheckedInt_nid_no_patch(last - text) : 0;
    if (length > 0) {
        bool shift = length >= 3 && Lower(text[0]) == 'u' && Lower(text[1]) == 'n' && Lower(text[2]) == 'd' && text[3] == '_';
        if (shift) { text += 3; length -= 3; }
        const auto count = std::min(length, capacity);
        if (count < 0 || (count && !parent)) throw std::invalid_argument("ICU locale parent: invalid destination");
        if (shift) { if (count) std::memmove(parent, text, count); }
        else if (parent != text && count) {
            const auto source = reinterpret_cast<std::uintptr_t>(text);
            const auto destination = reinterpret_cast<std::uintptr_t>(parent);
            const auto size = static_cast<std::uintptr_t>(count);
            if (source <= destination ? destination - source < size : source - destination < size)
                throw std::invalid_argument("ICU locale parent: overlapping copy");
            std::memcpy(parent, text, count);
        }
    }
    if (length < capacity) {
        if (!parent) throw std::invalid_argument("ICU locale parent: null destination");
        parent[length] = 0;
        if (*status == -124) *status = 0;
    } else *status = length == capacity ? -124 : 15;
    return length;
}

}
