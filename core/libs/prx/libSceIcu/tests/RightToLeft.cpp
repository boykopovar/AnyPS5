#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>

namespace {

struct Case {
    const char* locale;
    bool rightToLeft;
};

void Expect(const Case& item) {
    const auto result = uloc_isRightToLeft_nid_postfix(item.locale);
    if (result == (item.rightToLeft ? 1 : 0)) return;
    std::fprintf(stderr, "uloc_isRightToLeft(\"%s\") returned %d, expected %d\n", item.locale, result, item.rightToLeft ? 1 : 0);
    std::abort();
}

void ExpectThrows(const char* locale) {
    try {
        uloc_isRightToLeft_nid_postfix(locale);
    } catch (const std::exception&) {
        return;
    }
    std::fprintf(stderr, "uloc_isRightToLeft(\"%s\") did not throw\n", locale ? locale : "null");
    std::abort();
}

void Scripts() {
    for (const Case& item : std::initializer_list<Case>{
             {"ar_Arab", true}, {"en_Arab", true}, {"en-Arab", true}, {"en_arab", true}, {"en_ARAB_US", true},
             {"en_Hebr", true}, {"en_Thaa", true}, {"en_Syrc", true}, {"en_Adlm", true}, {"en_Nkoo", true},
             {"en_Rohg", true}, {"en_Aran", false}, {"en_Ougr", true}, {"en_Gara", true}, {"en_Sidt", true},
             {"ar_Latn", false}, {"he_Latn_IL", false}, {"fa_Cyrl", false}, {"ur_Hans", false},
             {"ar_Abcd", false}, {"he_Zzzz", false}, {"fa_Zyyy", false}, {"ar_Qaaa", false},
             {"en_Arab1", true}, {"en_Arab.UTF-8", true}, {"en_Arab@calendar=islamic", true}, {"en_Arab_IR", true},
             {"_Arab", true}, {"-Hebr", true}, {"und_Arab", true}, {"und-Hebr", true}, {"UND_HEBR_IL", true},
             {"x-foo_Arab", true}, {"abcdefghijk_Arab", true}, {"root-Arab", true}, {"root_Latn", false}}) {
        Expect(item);
    }
}

void CommonLanguages() {
    for (const Case& item : std::initializer_list<Case>{
             {"ar", true}, {"he", true}, {"fa", true}, {"AR", true}, {"He_IL", true}, {"fa-IR", true},
             {"ar.UTF-8", true}, {"ar@calendar=islamic", true}, {"ar-u-nu-arab", true}, {"ar_", true},
             {"en", false}, {"es", false}, {"pt", false}, {"zh", false}, {"ja", false}, {"ko", false},
             {"de", false}, {"fr", false}, {"it", false}, {"ru", false}, {"nl", false}, {"pl", false},
             {"th", false}, {"tr", false}, {"en-u-nu-arab", false}, {"ru_Arabic", false},
             {"ara", true}, {"HEB", true}, {"fas_IR", true}, {"eng", false}, {"zho_CN", false}, {"deu", false},
             {"ar_Arabx", true}, {"ar_Ara", true}, {"ar__Arab", true}, {"ar_EG_Arab", true}, {"en_US_Arab", false},
             {"root_US", false}, {"t", false}, {"ot", false}, {"n", false}, {"h", false}}) {
        Expect(item);
    }
}

void LikelySubtags() {
    for (const char* locale : {"", "ur", "ps_AF", "yi", "dv", "und", "und_", "und@x", "und.x", "root", "ROOT",
                               "r", "e", "oo", "iw", "per", "x-foo", "i_klingon", "abcdefgh", "abcdefghijk_",
                               "abcdefghijkl_Arab", "ur_Arabx", "@x", ".x", "_US"}) {
        ExpectThrows(locale);
    }
    ExpectThrows(nullptr);
}

}

int main() {
    Scripts();
    CommonLanguages();
    LikelySubtags();
    return 0;
}
