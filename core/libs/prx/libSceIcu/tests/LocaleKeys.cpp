#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

using Conversion = const char* (APS5_VABI*)(const char*);

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

}

int main() {
    LegacyKeys();
    UnicodeKeys();
    return 0;
}
