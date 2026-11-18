#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <utility>

namespace {

using Conversion = const char* (APS5_VABI*)(const char*);
using TypeConversion = const char* (APS5_VABI*)(const char*, const char*);

struct Case {
    const char* keyword;
    const char* expected;
};

void Expect(Conversion convert, const char* name, const Case& item) {
    const char* result = convert(item.keyword);
    const bool same = result == item.expected ||
                      (result && item.expected && std::strcmp(result, item.expected) == 0);
    if (same) return;
    std::fprintf(stderr, "%s(\"%s\") returned \"%s\", expected \"%s\"\n", name, item.keyword ? item.keyword : "null",
                 result ? result : "null", item.expected ? item.expected : "null");
    std::abort();
}

void ExpectSame(Conversion convert, const char* name, const char* keyword) {
    if (convert(keyword) == keyword) return;
    std::fprintf(stderr, "%s(\"%s\") did not return its argument\n", name, keyword);
    std::abort();
}

void LegacyKeys() {
    for (const Case& item : std::initializer_list<Case>{
             {"ca", "calendar"}, {"CA", "calendar"}, {"calendar", "calendar"}, {"Calendar", "calendar"},
             {"co", "collation"}, {"ka", "colalternate"}, {"kb", "colbackwards"}, {"kf", "colcasefirst"},
             {"kc", "colcaselevel"}, {"kh", "colhiraganaquaternary"}, {"kk", "colnormalization"},
             {"kn", "colnumeric"}, {"kr", "colreorder"}, {"ks", "colstrength"}, {"cu", "currency"},
             {"hc", "hours"}, {"ms", "measure"}, {"nu", "numbers"}, {"tz", "timezone"}, {"vt", "variabletop"},
             {"cf", "cf"}, {"RG", "rg"}, {"mu", "mu"}, {"x0", "x0"},
             {nullptr, nullptr}, {"", nullptr}, {"a-b", nullptr}, {"c_a", nullptr}, {"ca ", nullptr},
             {"@", nullptr}}) {
        Expect(uloc_toLegacyKey_nid_postfix, "uloc_toLegacyKey", item);
    }
    for (const char* keyword : {"zz", "Abc1", "unknownkey", "1"}) ExpectSame(uloc_toLegacyKey_nid_postfix, "uloc_toLegacyKey", keyword);
}

void UnicodeKeys() {
    for (const Case& item : std::initializer_list<Case>{
             {"calendar", "ca"}, {"CALENDAR", "ca"}, {"ca", "ca"}, {"Ca", "ca"}, {"collation", "co"},
             {"colalternate", "ka"}, {"colbackwards", "kb"}, {"colcasefirst", "kf"}, {"colcaselevel", "kc"},
             {"colhiraganaquaternary", "kh"}, {"colnormalization", "kk"}, {"colnumeric", "kn"},
             {"colreorder", "kr"}, {"colstrength", "ks"}, {"currency", "cu"}, {"hours", "hc"},
             {"measure", "ms"}, {"numbers", "nu"}, {"timezone", "tz"}, {"variabletop", "vt"},
             {"cf", "cf"}, {"SD", "sd"}, {"mu", "mu"},
             {nullptr, nullptr}, {"", nullptr}, {"a1", nullptr}, {"abc", nullptr}, {"a", nullptr},
             {"unknownkey", nullptr}, {"-a", nullptr}}) {
        Expect(uloc_toUnicodeLocaleKey_nid_postfix, "uloc_toUnicodeLocaleKey", item);
    }
    for (const char* keyword : {"zz", "1a", "Qx"}) ExpectSame(uloc_toUnicodeLocaleKey_nid_postfix, "uloc_toUnicodeLocaleKey", keyword);
}

struct TypeCase {
    const char* keyword;
    const char* value;
    const char* expected;
};

void ExpectType(TypeConversion convert, const char* name, const TypeCase& item) {
    const char* result = convert(item.keyword, item.value);
    const bool same = result == item.expected ||
                      (result && item.expected && std::strcmp(result, item.expected) == 0);
    if (same) return;
    std::fprintf(stderr, "%s(\"%s\", \"%s\") returned \"%s\", expected \"%s\"\n", name,
                 item.keyword ? item.keyword : "null", item.value ? item.value : "null", result ? result : "null",
                 item.expected ? item.expected : "null");
    std::abort();
}

void ExpectSameType(TypeConversion convert, const char* name, const char* keyword, const char* value) {
    if (convert(keyword, value) == value) return;
    std::fprintf(stderr, "%s(\"%s\", \"%s\") did not return its value\n", name, keyword, value);
    std::abort();
}

void LegacyTypes() {
    for (const TypeCase& item : std::initializer_list<TypeCase>{
             {"ca", "gregory", "gregorian"}, {"calendar", "GREGORIAN", "gregorian"}, {"CA", "Gregory", "gregorian"},
             {"ca", "islamicc", "islamic-civil"}, {"ks", "level1", "primary"}, {"colstrength", "quarternary", "quaternary"},
             {"kn", "true", "yes"}, {"cu", "USD", "usd"}, {"nu", "latn", "latn"},
             {"tz", "usnyc", "America/New_York"}, {"timezone", "america/new_york", "America/New_York"},
             {"tz", "utcw01", "Etc/GMT+1"}, {"tz", "America/Argentina/Buenos_Aires", "America/Buenos_Aires"},
             {"tz", "aqams", "Antarctica/McMurdo"}, {"tz", "Africa/Asmara", "Africa/Asmera"},
             {"sd", "subdivision_code", "SUBDIVISION_CODE"},
             {nullptr, "gregory", nullptr}, {"ca", nullptr, nullptr}, {"", "gregory", nullptr}, {"ca", "", nullptr},
             {"ca", "a__b", nullptr}, {"ca", "_ab", nullptr}, {"ca", "ab/", nullptr}, {"ca", "a+b", nullptr}}) {
        ExpectType(uloc_toLegacyType_nid_postfix, "uloc_toLegacyType", item);
    }
    for (const auto& [keyword, value] : std::initializer_list<std::pair<const char*, const char*>>{
             {"kr", "latn-grek"}, {"vt", "0020-0041"}, {"rg", "USzzzz"}, {"ca", "unknown_cal"}, {"zz", "Etc/x-1"},
             {"tz", "Mars/Olympus"}, {"x", "a"}}) {
        ExpectSameType(uloc_toLegacyType_nid_postfix, "uloc_toLegacyType", keyword, value);
    }
}

void UnicodeTypes() {
    for (const TypeCase& item : std::initializer_list<TypeCase>{
             {"calendar", "gregorian", "gregory"}, {"ca", "GREGORY", "gregory"}, {"ca", "islamicc", "islamic-civil"},
             {"colstrength", "primary", "level1"}, {"ks", "quarternary", "level4"}, {"kn", "yes", "true"},
             {"timezone", "America/New_York", "usnyc"}, {"tz", "etc/gmt+1", "utcw01"},
             {"tz", "America/Argentina/Buenos_Aires", "arbue"}, {"tz", "aqams", "aqmcm"},
             {nullptr, "gregory", nullptr}, {"ca", nullptr, nullptr}, {"", "x", nullptr}, {"ca", "", nullptr},
             {"ca", "ab", nullptr}, {"ca", "abcdefghi", nullptr}, {"ca", "abc--def", nullptr}, {"ca", "-abc", nullptr},
             {"ca", "abc_def", nullptr}, {"kr", "la-grek", nullptr}, {"rg", "us-z", nullptr}, {"zz", "Etc/x", nullptr}}) {
        ExpectType(uloc_toUnicodeLocaleType, "uloc_toUnicodeLocaleType", item);
    }
    for (const auto& [keyword, value] : std::initializer_list<std::pair<const char*, const char*>>{
             {"kr", "latn-grek"}, {"vt", "0020-0041"}, {"rg", "uszzzz"}, {"ca", "unknown"}, {"zz", "abc-defgh"},
             {"tz", "abcdefgh-123"}, {"rg", "uszz"}}) {
        ExpectSameType(uloc_toUnicodeLocaleType, "uloc_toUnicodeLocaleType", keyword, value);
    }
}

}

int main() {
    LegacyKeys();
    UnicodeKeys();
    LegacyTypes();
    UnicodeTypes();
    return 0;
}
