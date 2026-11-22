// Derived from icu4c/source/common/uloc.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2): the ISO 639 and ISO 3166 code tables and the uloc_getParent, uloc_getISO3Language, uloc_getISO3Country, uloc_getISOLanguages and uloc_getISOCountries logic below.
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uloc.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libc/include/General.hpp"
#include "prx/libSceIcu/TerminateBuffer.hpp"
#include "prx/libSceIcu/DefaultLocaleName.hpp"
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
constexpr std::int32_t kFullNameCapacity = 157;

constexpr std::array<std::pair<std::string_view, std::string_view>, 10> CanonicalIds{{
    {"art__LOJBAN", "jbo"}, {"hy__AREVELA", "hy"}, {"hy__AREVMDA", "hyw"}, {"zh__GUOYU", "zh"}, {"zh__HAKKA", "hak"},
    {"zh__XIANG", "hsn"}, {"zh_GAN", "gan"}, {"zh_MIN_NAN", "nan"}, {"zh_WUU", "wuu"}, {"zh_YUE", "yue"},
}};

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

constexpr std::array<const char*, 604> IsoLanguages{{
    "aa", "ab", "ace", "ach", "ada", "ady", "ae", "aeb",
    "af", "afh", "agq", "ain", "ak", "akk", "akz", "ale",
    "aln", "alt", "am", "an", "ang", "anp", "ar", "arc",
    "arn", "aro", "arp", "arq", "ars", "arw", "ary", "arz",
    "as", "asa", "ase", "ast", "av", "avk", "awa", "ay",
    "az", "ba", "bal", "ban", "bar", "bas", "bax", "bbc",
    "bbj", "be", "bej", "bem", "bew", "bez", "bfd", "bfq",
    "bg", "bgn", "bho", "bi", "bik", "bin", "bjn", "bkm",
    "bla", "bm", "bn", "bo", "bpy", "bqi", "br", "bra",
    "brh", "brx", "bs", "bss", "bua", "bug", "bum", "byn",
    "byv", "ca", "cad", "car", "cay", "cch", "ccp", "ce",
    "ceb", "cgg", "ch", "chb", "chg", "chk", "chm", "chn",
    "cho", "chp", "chr", "chy", "ckb", "co", "cop", "cps",
    "cr", "crh", "cs", "csb", "cu", "cv", "cy", "da",
    "dak", "dar", "dav", "de", "del", "den", "dgr", "din",
    "dje", "doi", "dsb", "dtp", "dua", "dum", "dv", "dyo",
    "dyu", "dz", "dzg", "ebu", "ee", "efi", "egl", "egy",
    "eka", "el", "elx", "en", "enm", "eo", "es", "esu",
    "et", "eu", "ewo", "ext", "fa", "fan", "fat", "ff",
    "fi", "fil", "fit", "fj", "fo", "fon", "fr", "frc",
    "frm", "fro", "frp", "frr", "frs", "fur", "fy", "ga",
    "gaa", "gag", "gan", "gay", "gba", "gbz", "gd", "gez",
    "gil", "gl", "glk", "gmh", "gn", "goh", "gom", "gon",
    "gor", "got", "grb", "grc", "gsw", "gu", "guc", "gur",
    "guz", "gv", "gwi", "ha", "hai", "hak", "haw", "he",
    "hi", "hif", "hil", "hit", "hmn", "ho", "hr", "hsb",
    "hsn", "ht", "hu", "hup", "hy", "hz", "ia", "iba",
    "ibb", "id", "ie", "ig", "ii", "ik", "ilo", "inh",
    "io", "is", "it", "iu", "izh", "ja", "jam", "jbo",
    "jgo", "jmc", "jpr", "jrb", "jut", "jv", "ka", "kaa",
    "kab", "kac", "kaj", "kam", "kaw", "kbd", "kbl", "kcg",
    "kde", "kea", "ken", "kfo", "kg", "kgp", "kha", "kho",
    "khq", "khw", "ki", "kiu", "kj", "kk", "kkj", "kl",
    "kln", "km", "kmb", "kn", "ko", "koi", "kok", "kos",
    "kpe", "kr", "krc", "kri", "krj", "krl", "kru", "ks",
    "ksb", "ksf", "ksh", "ku", "kum", "kut", "kv", "kw",
    "ky", "la", "lad", "lag", "lah", "lam", "lb", "lez",
    "lfn", "lg", "li", "lij", "liv", "lkt", "lmo", "ln",
    "lo", "lol", "loz", "lrc", "lt", "ltg", "lu", "lua",
    "lui", "lun", "luo", "lus", "luy", "lv", "lzh", "lzz",
    "mad", "maf", "mag", "mai", "mak", "man", "mas", "mde",
    "mdf", "mdh", "mdr", "men", "mer", "mfe", "mg", "mga",
    "mgh", "mgo", "mh", "mi", "mic", "min", "mis", "mk",
    "ml", "mn", "mnc", "mni", "mo", "moh", "mos", "mr",
    "mrj", "ms", "mt", "mua", "mul", "mus", "mwl", "mwr",
    "mwv", "my", "mye", "myv", "mzn", "na", "nan", "nap",
    "naq", "nb", "nd", "nds", "ne", "new", "ng", "nia",
    "niu", "njo", "nl", "nmg", "nn", "nnh", "no", "nog",
    "non", "nov", "nqo", "nr", "nso", "nus", "nv", "nwc",
    "ny", "nym", "nyn", "nyo", "nzi", "oc", "oj", "om",
    "or", "os", "osa", "ota", "pa", "pag", "pal", "pam",
    "pap", "pau", "pcd", "pcm", "pdc", "pdt", "peo", "pfl",
    "phn", "pi", "pl", "pms", "pnt", "pon", "prg", "pro",
    "ps", "pt", "qu", "quc", "qug", "raj", "rap", "rar",
    "rgn", "rif", "rm", "rn", "ro", "rof", "rom", "rtm",
    "ru", "rue", "rug", "rup", "rw", "rwk", "sa", "sad",
    "sah", "sam", "saq", "sas", "sat", "saz", "sba", "sbp",
    "sc", "scn", "sco", "sd", "sdc", "sdh", "se", "see",
    "seh", "sei", "sel", "ses", "sg", "sga", "sgs", "shi",
    "shn", "shu", "si", "sid", "sk", "sl", "sli", "sly",
    "sm", "sma", "smj", "smn", "sms", "sn", "snk", "so",
    "sog", "sq", "sr", "srn", "srr", "ss", "ssy", "st",
    "stq", "su", "suk", "sus", "sux", "sv", "sw", "swb",
    "swc", "syc", "syr", "szl", "ta", "tcy", "te", "tem",
    "teo", "ter", "tet", "tg", "th", "ti", "tig", "tiv",
    "tk", "tkl", "tkr", "tl", "tlh", "tli", "tly", "tmh",
    "tn", "to", "tog", "tpi", "tr", "tru", "trv", "ts",
    "tsd", "tsi", "tt", "ttt", "tum", "tvl", "tw", "twq",
    "ty", "tyv", "tzm", "udm", "ug", "uga", "uk", "umb",
    "und", "ur", "uz", "vai", "ve", "vec", "vep", "vi",
    "vls", "vmf", "vo", "vot", "vro", "vun", "wa", "wae",
    "wal", "war", "was", "wbp", "wo", "wuu", "xal", "xh",
    "xmf", "xog", "yao", "yap", "yav", "ybb", "yi", "yo",
    "yrl", "yue", "za", "zap", "zbl", "zea", "zen", "zgh",
    "zh", "zu", "zun", "zxx", "zza", nullptr, "in", "iw",
    "ji", "jw", "sh", nullptr
}};

constexpr std::array<const char*, 604> IsoLanguages3{{
    "aar", "abk", "ace", "ach", "ada", "ady", "ave", "aeb",
    "afr", "afh", "agq", "ain", "aka", "akk", "akz", "ale",
    "aln", "alt", "amh", "arg", "ang", "anp", "ara", "arc",
    "arn", "aro", "arp", "arq", "ars", "arw", "ary", "arz",
    "asm", "asa", "ase", "ast", "ava", "avk", "awa", "aym",
    "aze", "bak", "bal", "ban", "bar", "bas", "bax", "bbc",
    "bbj", "bel", "bej", "bem", "bew", "bez", "bfd", "bfq",
    "bul", "bgn", "bho", "bis", "bik", "bin", "bjn", "bkm",
    "bla", "bam", "ben", "bod", "bpy", "bqi", "bre", "bra",
    "brh", "brx", "bos", "bss", "bua", "bug", "bum", "byn",
    "byv", "cat", "cad", "car", "cay", "cch", "ccp", "che",
    "ceb", "cgg", "cha", "chb", "chg", "chk", "chm", "chn",
    "cho", "chp", "chr", "chy", "ckb", "cos", "cop", "cps",
    "cre", "crh", "ces", "csb", "chu", "chv", "cym", "dan",
    "dak", "dar", "dav", "deu", "del", "den", "dgr", "din",
    "dje", "doi", "dsb", "dtp", "dua", "dum", "div", "dyo",
    "dyu", "dzo", "dzg", "ebu", "ewe", "efi", "egl", "egy",
    "eka", "ell", "elx", "eng", "enm", "epo", "spa", "esu",
    "est", "eus", "ewo", "ext", "fas", "fan", "fat", "ful",
    "fin", "fil", "fit", "fij", "fao", "fon", "fra", "frc",
    "frm", "fro", "frp", "frr", "frs", "fur", "fry", "gle",
    "gaa", "gag", "gan", "gay", "gba", "gbz", "gla", "gez",
    "gil", "glg", "glk", "gmh", "grn", "goh", "gom", "gon",
    "gor", "got", "grb", "grc", "gsw", "guj", "guc", "gur",
    "guz", "glv", "gwi", "hau", "hai", "hak", "haw", "heb",
    "hin", "hif", "hil", "hit", "hmn", "hmo", "hrv", "hsb",
    "hsn", "hat", "hun", "hup", "hye", "her", "ina", "iba",
    "ibb", "ind", "ile", "ibo", "iii", "ipk", "ilo", "inh",
    "ido", "isl", "ita", "iku", "izh", "jpn", "jam", "jbo",
    "jgo", "jmc", "jpr", "jrb", "jut", "jav", "kat", "kaa",
    "kab", "kac", "kaj", "kam", "kaw", "kbd", "kbl", "kcg",
    "kde", "kea", "ken", "kfo", "kon", "kgp", "kha", "kho",
    "khq", "khw", "kik", "kiu", "kua", "kaz", "kkj", "kal",
    "kln", "khm", "kmb", "kan", "kor", "koi", "kok", "kos",
    "kpe", "kau", "krc", "kri", "krj", "krl", "kru", "kas",
    "ksb", "ksf", "ksh", "kur", "kum", "kut", "kom", "cor",
    "kir", "lat", "lad", "lag", "lah", "lam", "ltz", "lez",
    "lfn", "lug", "lim", "lij", "liv", "lkt", "lmo", "lin",
    "lao", "lol", "loz", "lrc", "lit", "ltg", "lub", "lua",
    "lui", "lun", "luo", "lus", "luy", "lav", "lzh", "lzz",
    "mad", "maf", "mag", "mai", "mak", "man", "mas", "mde",
    "mdf", "mdh", "mdr", "men", "mer", "mfe", "mlg", "mga",
    "mgh", "mgo", "mah", "mri", "mic", "min", "mis", "mkd",
    "mal", "mon", "mnc", "mni", "mol", "moh", "mos", "mar",
    "mrj", "msa", "mlt", "mua", "mul", "mus", "mwl", "mwr",
    "mwv", "mya", "mye", "myv", "mzn", "nau", "nan", "nap",
    "naq", "nob", "nde", "nds", "nep", "new", "ndo", "nia",
    "niu", "njo", "nld", "nmg", "nno", "nnh", "nor", "nog",
    "non", "nov", "nqo", "nbl", "nso", "nus", "nav", "nwc",
    "nya", "nym", "nyn", "nyo", "nzi", "oci", "oji", "orm",
    "ori", "oss", "osa", "ota", "pan", "pag", "pal", "pam",
    "pap", "pau", "pcd", "pcm", "pdc", "pdt", "peo", "pfl",
    "phn", "pli", "pol", "pms", "pnt", "pon", "prg", "pro",
    "pus", "por", "que", "quc", "qug", "raj", "rap", "rar",
    "rgn", "rif", "roh", "run", "ron", "rof", "rom", "rtm",
    "rus", "rue", "rug", "rup", "kin", "rwk", "san", "sad",
    "sah", "sam", "saq", "sas", "sat", "saz", "sba", "sbp",
    "srd", "scn", "sco", "snd", "sdc", "sdh", "sme", "see",
    "seh", "sei", "sel", "ses", "sag", "sga", "sgs", "shi",
    "shn", "shu", "sin", "sid", "slk", "slv", "sli", "sly",
    "smo", "sma", "smj", "smn", "sms", "sna", "snk", "som",
    "sog", "sqi", "srp", "srn", "srr", "ssw", "ssy", "sot",
    "stq", "sun", "suk", "sus", "sux", "swe", "swa", "swb",
    "swc", "syc", "syr", "szl", "tam", "tcy", "tel", "tem",
    "teo", "ter", "tet", "tgk", "tha", "tir", "tig", "tiv",
    "tuk", "tkl", "tkr", "tgl", "tlh", "tli", "tly", "tmh",
    "tsn", "ton", "tog", "tpi", "tur", "tru", "trv", "tso",
    "tsd", "tsi", "tat", "ttt", "tum", "tvl", "twi", "twq",
    "tah", "tyv", "tzm", "udm", "uig", "uga", "ukr", "umb",
    "und", "urd", "uzb", "vai", "ven", "vec", "vep", "vie",
    "vls", "vmf", "vol", "vot", "vro", "vun", "wln", "wae",
    "wal", "war", "was", "wbp", "wol", "wuu", "xal", "xho",
    "xmf", "xog", "yao", "yap", "yav", "ybb", "yid", "yor",
    "yrl", "yue", "zha", "zap", "zbl", "zea", "zen", "zgh",
    "zho", "zul", "zun", "zxx", "zza", nullptr, "ind", "heb",
    "yid", "jaw", "srp", nullptr
}};

constexpr std::array<const char*, 261> IsoCountries{{
    "AD", "AE", "AF", "AG", "AI", "AL", "AM", "AO",
    "AQ", "AR", "AS", "AT", "AU", "AW", "AX", "AZ",
    "BA", "BB", "BD", "BE", "BF", "BG", "BH", "BI",
    "BJ", "BL", "BM", "BN", "BO", "BQ", "BR", "BS",
    "BT", "BV", "BW", "BY", "BZ", "CA", "CC", "CD",
    "CF", "CG", "CH", "CI", "CK", "CL", "CM", "CN",
    "CO", "CR", "CU", "CV", "CW", "CX", "CY", "CZ",
    "DE", "DJ", "DK", "DM", "DO", "DZ", "EC", "EE",
    "EG", "EH", "ER", "ES", "ET", "FI", "FJ", "FK",
    "FM", "FO", "FR", "GA", "GB", "GD", "GE", "GF",
    "GG", "GH", "GI", "GL", "GM", "GN", "GP", "GQ",
    "GR", "GS", "GT", "GU", "GW", "GY", "HK", "HM",
    "HN", "HR", "HT", "HU", "ID", "IE", "IL", "IM",
    "IN", "IO", "IQ", "IR", "IS", "IT", "JE", "JM",
    "JO", "JP", "KE", "KG", "KH", "KI", "KM", "KN",
    "KP", "KR", "KW", "KY", "KZ", "LA", "LB", "LC",
    "LI", "LK", "LR", "LS", "LT", "LU", "LV", "LY",
    "MA", "MC", "MD", "ME", "MF", "MG", "MH", "MK",
    "ML", "MM", "MN", "MO", "MP", "MQ", "MR", "MS",
    "MT", "MU", "MV", "MW", "MX", "MY", "MZ", "NA",
    "NC", "NE", "NF", "NG", "NI", "NL", "NO", "NP",
    "NR", "NU", "NZ", "OM", "PA", "PE", "PF", "PG",
    "PH", "PK", "PL", "PM", "PN", "PR", "PS", "PT",
    "PW", "PY", "QA", "RE", "RO", "RS", "RU", "RW",
    "SA", "SB", "SC", "SD", "SE", "SG", "SH", "SI",
    "SJ", "SK", "SL", "SM", "SN", "SO", "SR", "SS",
    "ST", "SV", "SX", "SY", "SZ", "TC", "TD", "TF",
    "TG", "TH", "TJ", "TK", "TL", "TM", "TN", "TO",
    "TR", "TT", "TV", "TW", "TZ", "UA", "UG", "UM",
    "US", "UY", "UZ", "VA", "VC", "VE", "VG", "VI",
    "VN", "VU", "WF", "WS", "YE", "YT", "ZA", "ZM",
    "ZW", nullptr, "AN", "BU", "CS", "FX", "RO", "SU",
    "TP", "YD", "YU", "ZR", nullptr
}};

constexpr std::array<const char*, 261> IsoCountries3{{
    "AND", "ARE", "AFG", "ATG", "AIA", "ALB", "ARM", "AGO",
    "ATA", "ARG", "ASM", "AUT", "AUS", "ABW", "ALA", "AZE",
    "BIH", "BRB", "BGD", "BEL", "BFA", "BGR", "BHR", "BDI",
    "BEN", "BLM", "BMU", "BRN", "BOL", "BES", "BRA", "BHS",
    "BTN", "BVT", "BWA", "BLR", "BLZ", "CAN", "CCK", "COD",
    "CAF", "COG", "CHE", "CIV", "COK", "CHL", "CMR", "CHN",
    "COL", "CRI", "CUB", "CPV", "CUW", "CXR", "CYP", "CZE",
    "DEU", "DJI", "DNK", "DMA", "DOM", "DZA", "ECU", "EST",
    "EGY", "ESH", "ERI", "ESP", "ETH", "FIN", "FJI", "FLK",
    "FSM", "FRO", "FRA", "GAB", "GBR", "GRD", "GEO", "GUF",
    "GGY", "GHA", "GIB", "GRL", "GMB", "GIN", "GLP", "GNQ",
    "GRC", "SGS", "GTM", "GUM", "GNB", "GUY", "HKG", "HMD",
    "HND", "HRV", "HTI", "HUN", "IDN", "IRL", "ISR", "IMN",
    "IND", "IOT", "IRQ", "IRN", "ISL", "ITA", "JEY", "JAM",
    "JOR", "JPN", "KEN", "KGZ", "KHM", "KIR", "COM", "KNA",
    "PRK", "KOR", "KWT", "CYM", "KAZ", "LAO", "LBN", "LCA",
    "LIE", "LKA", "LBR", "LSO", "LTU", "LUX", "LVA", "LBY",
    "MAR", "MCO", "MDA", "MNE", "MAF", "MDG", "MHL", "MKD",
    "MLI", "MMR", "MNG", "MAC", "MNP", "MTQ", "MRT", "MSR",
    "MLT", "MUS", "MDV", "MWI", "MEX", "MYS", "MOZ", "NAM",
    "NCL", "NER", "NFK", "NGA", "NIC", "NLD", "NOR", "NPL",
    "NRU", "NIU", "NZL", "OMN", "PAN", "PER", "PYF", "PNG",
    "PHL", "PAK", "POL", "SPM", "PCN", "PRI", "PSE", "PRT",
    "PLW", "PRY", "QAT", "REU", "ROU", "SRB", "RUS", "RWA",
    "SAU", "SLB", "SYC", "SDN", "SWE", "SGP", "SHN", "SVN",
    "SJM", "SVK", "SLE", "SMR", "SEN", "SOM", "SUR", "SSD",
    "STP", "SLV", "SXM", "SYR", "SWZ", "TCA", "TCD", "ATF",
    "TGO", "THA", "TJK", "TKL", "TLS", "TKM", "TUN", "TON",
    "TUR", "TTO", "TUV", "TWN", "TZA", "UKR", "UGA", "UMI",
    "USA", "URY", "UZB", "VAT", "VCT", "VEN", "VGB", "VIR",
    "VNM", "VUT", "WLF", "WSM", "YEM", "MYT", "ZAF", "ZMB",
    "ZWE", nullptr, "ANT", "BUR", "SCG", "FXX", "ROM", "SUN",
    "TMP", "YMD", "YUG", "ZAR", nullptr
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

std::string ChangeCase(std::string_view value, bool upper) {
    std::string result(value);
    for (char& part : result) part = upper ? Upper(part) : Lower(part);
    return result;
}

bool AlphaNumeric(char value) {
    return Alpha(value) || (value >= '0' && value <= '9');
}

bool KeywordName(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), AlphaNumeric);
}

bool ValueCharacter(char value) {
    return AlphaNumeric(value) || value == '_' || value == '-' || value == '+' || value == '/';
}

std::string_view Trim(std::string_view value) {
    const auto first = value.find_first_not_of(' ');
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(' ') - first + 1);
}

bool ShortestSubtagIsOne(std::string_view value) {
    std::size_t shortest = value.size();
    std::size_t current = 0;
    bool reset = true;
    for (const char part : value) {
        if (part != '_' && part != '-') {
            if (reset) current = 0;
            reset = false;
            ++current;
        } else {
            if (current != 0 && current < shortest) shortest = current;
            reset = true;
        }
    }
    return shortest == 1;
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
    return value.size() == 2 || value.size() == 3;
}

std::int32_t FindCode(const char* const* list, std::size_t count, const char* key) {
    bool second = false;
    for (std::size_t index = 0; index < count; ++index) {
        if (list[index] == nullptr) {
            if (second) break;
            second = true;
            continue;
        }
        if (std::strcmp(key, list[index]) == 0) return static_cast<std::int32_t>(index);
    }
    return -1;
}

const char* Iso3Language(const std::string& code) {
    if (code.size() == 2) {
        const auto offset = FindCode(IsoLanguages.data(), IsoLanguages.size(), code.c_str());
        return offset < 0 ? "" : IsoLanguages3[static_cast<std::size_t>(offset)];
    }
    if (code.size() == 3) {
        const auto offset = FindCode(IsoLanguages3.data(), IsoLanguages3.size(), code.c_str());
        return offset < 0 ? "" : IsoLanguages3[static_cast<std::size_t>(offset)];
    }
    return "";
}

const char* Iso3Country(const std::string& code) {
    const auto offset = FindCode(IsoCountries.data(), IsoCountries.size(), code.c_str());
    return offset < 0 ? "" : IsoCountries3[static_cast<std::size_t>(offset)];
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

    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (start <= base.size()) {
        const auto end = base.find_first_of("_-", start);
        tokens.emplace_back(base.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }

    std::size_t index = 1;
    parts.language = ChangeCase(tokens.front(), false);
    if (tokens.size() > 1 && (parts.language == "i" || parts.language == "x")) {
        parts.language += "-" + ChangeCase(tokens[1], false);
        index = 2;
    }
    if (parts.language == "und" || parts.language == "root") parts.language.clear();
    if (parts.language.size() == 3) parts.language = ShortCode(Languages, parts.language);
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
        if (index < tokens.size()) parts.variant += "_";
    }
    for (; index < tokens.size(); ++index) {
        if (tokens[index].empty()) continue;
        if (!parts.variant.empty() && parts.variant.back() != '_') parts.variant.push_back('_');
        parts.variant += ChangeCase(tokens[index], true);
    }
    return parts;
}

std::string BaseName(const Parts& parts) {
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
    if (capacity > size && destination) {
        destination[size] = '\0';
        if (*status == kStringNotTerminated) *status = 0;
    } else if (capacity == size) *status = kStringNotTerminated;
    else *status = kBufferOverflow;
    return size;
}

std::int32_t Component(const char* locale, char* result, std::int32_t capacity,
                       std::int32_t* status, int which) {
    if (!status || *status > 0) return 0;
    if (!locale) locale = Icu::DefaultLocaleName();
    Parts parts = Parse(locale);
    const std::string& value = which == 0 ? parts.language : which == 1 ? parts.country : parts.script;
    return Write(value, result, capacity, status, true);
}

std::string FromLanguageTag(const char* locale, std::int32_t* status) {
    const std::string_view input(locale);
    if (input.find('@') != std::string_view::npos || !ShortestSubtagIsOne(input)) return locale;
    std::array<char, kFullNameCapacity> buffer;
    const std::int32_t length = uloc_forLanguageTag(locale, buffer.data(), kFullNameCapacity, nullptr, status);
    if (*status == kStringNotTerminated) *status = kBufferOverflow;
    return length > 0 && *status <= 0 ? std::string(buffer.data(), static_cast<std::size_t>(length)) : std::string(locale);
}

std::int32_t WriteUnconverted(std::string_view id, char* destination, std::int32_t capacity) {
    std::size_t position = 0;
    if (ChangeCase(id, false) == "root") position = 4;
    else if (ChangeCase(id.substr(0, 3), false) == "und" && (id.size() == 3 || id[3] == '-' || id[3] == '_')) position = 3;
    if (position + 1 < id.size() && (Lower(id[position]) == 'i' || Lower(id[position]) == 'x')
        && (id[position + 1] == '-' || id[position + 1] == '_')) position += 2;
    while (position < id.size() && id[position] != '.' && id[position] != '-' && id[position] != '_') ++position;
    std::string remainder;
    if (position < id.size() && id[position] != '.') {
        for (++position; position < id.size() && id[position] != '.'; ++position)
            remainder.push_back(id[position] == '-' ? '_' : Upper(id[position]));
    }
    if (destination && capacity > 0)
        std::memcpy(destination, remainder.data(), std::min(remainder.size(), static_cast<std::size_t>(capacity)));
    return static_cast<std::int32_t>(remainder.size());
}

}

const char* Icu::DefaultLocaleName() {
    return uloc_getDefault();
}

extern "C" {

std::int32_t APS5_VABI uloc_getLanguage(const char* locale, char* result,
                                       std::int32_t capacity, std::int32_t* status) {
    return Component(locale, result, capacity, status, 0);
}

std::int32_t APS5_VABI uloc_getCountry(const char* locale, char* result,
                                      std::int32_t capacity, std::int32_t* status) {
    return Component(locale, result, capacity, status, 1);
}

std::int32_t APS5_VABI uloc_getScript(const char* locale, char* result,
                                     std::int32_t capacity, std::int32_t* status) {
    return Component(locale, result, capacity, status, 2);
}

std::int32_t APS5_VABI uloc_getBaseName(const char* locale, char* result,
                                       std::int32_t capacity, std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!locale) return Write(BaseName(Parse(Icu::DefaultLocaleName())), result, capacity, status, false);
    const std::string id = FromLanguageTag(locale, status);
    if (*status > 0) return WriteUnconverted(id, result, capacity);
    return Write(BaseName(Parse(id.c_str())), result, capacity, status, false);
}

std::int32_t APS5_VABI uloc_getKeywordValue(const char* locale, const char* keyword,
                                           char* result, std::int32_t capacity,
                                           std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!locale || !keyword || !KeywordName(keyword)) {
        *status = kIllegalArgument;
        return 0;
    }
    const std::string requested = ChangeCase(keyword, false);
    std::string_view input(locale);
    std::string converted;
    if (input.find('@') == std::string_view::npos && ShortestSubtagIsOne(input)) {
        std::int32_t conversion = 0;
        const std::int32_t length = uloc_forLanguageTag(locale, nullptr, 0, nullptr, &conversion);
        if (conversion > 0 && conversion != kBufferOverflow) {
            *status = conversion;
            return 0;
        }
        if (length > 0) {
            converted.resize(static_cast<std::size_t>(length));
            conversion = 0;
            uloc_forLanguageTag(locale, converted.data(), length, nullptr, &conversion);
            input = converted;
        }
    }
    auto start = input.find('@');
    while (start != std::string_view::npos) {
        const auto equal = input.find('=', start + 1);
        const auto name = Trim(input.substr(start + 1, equal - start - 1));
        if (equal == std::string_view::npos || !KeywordName(name)) {
            *status = kIllegalArgument;
            return 0;
        }
        start = input.find(';', equal);
        if (ChangeCase(name, false) == requested) {
            const auto value = Trim(input.substr(
                equal + 1, start == std::string_view::npos ? start : start - equal - 1));
            if (value.empty() || !std::all_of(value.begin(), value.end(), ValueCharacter)) {
                *status = kIllegalArgument;
                return 0;
            }
            return Write(value, result, capacity, status, false);
        }
    }
    return Write("", result, capacity, status, false);
}

std::int32_t APS5_VABI uloc_canonicalize(const char* locale, char* result,
                                        std::int32_t capacity, std::int32_t* status) {
    if (!status || *status > 0) return 0;
    const std::string id = locale ? FromLanguageTag(locale, status) : std::string(Icu::DefaultLocaleName());
    if (*status > 0) return WriteUnconverted(id, result, capacity);
    Parts parts = Parse(id.c_str());
    parts.encoding.clear();
    std::string canonical = BaseName(parts);
    const auto mapped = std::find_if(CanonicalIds.begin(), CanonicalIds.end(),
                                     [&](const auto& entry) { return entry.first == canonical; });
    if (mapped != CanonicalIds.end()) canonical = std::string(mapped->second);
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

std::int32_t APS5_VABI uloc_getParent(const char* locale, char* result,
                                                  std::int32_t capacity, std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!locale) locale = Icu::DefaultLocaleName();
    if (capacity < 0 || (capacity > 0 && !result)) {
        *status = kIllegalArgument;
        return 0;
    }
    const char* lastUnderscore = std::strrchr(locale, '_');
    std::int32_t length = lastUnderscore ? static_cast<std::int32_t>(lastUnderscore - locale) : 0;
    if (length > 0) {
        bool und = true;
        for (std::int32_t index = 0; index < 4; ++index)
            if (Lower(locale[index]) != "und_"[index]) {
                und = false;
                break;
            }
        if (und) {
            locale += 3;
            length -= 3;
            if (capacity > 0)
                std::memmove(result, locale, static_cast<std::size_t>(std::min(length, capacity)));
        } else if (result != locale && capacity > 0) {
            std::memcpy(result, locale, static_cast<std::size_t>(std::min(length, capacity)));
        }
    }
    return u_terminateChars_nid_postfix(result, capacity, length, status);
}

const char* APS5_VABI uloc_getISO3Language(const char* locale) {
    if (!locale) locale = Icu::DefaultLocaleName();
    return Iso3Language(Parse(locale).language);
}

const char* APS5_VABI uloc_getISO3Country(const char* locale) {
    if (!locale) locale = Icu::DefaultLocaleName();
    return Iso3Country(Parse(locale).country);
}

const char* const* APS5_VABI uloc_getISOLanguages() {
    return IsoLanguages.data();
}

const char* const* APS5_VABI uloc_getISOCountries() {
    return IsoCountries.data();
}

}
