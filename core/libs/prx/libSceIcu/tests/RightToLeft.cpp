#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>

namespace {

using UErrorCode = std::int32_t;
constexpr UErrorCode U_ZERO_ERROR = 0;
constexpr UErrorCode U_ILLEGAL_ARGUMENT_ERROR = 1;

struct Case {
    const char* locale;
    bool rightToLeft;
};

struct OrientationCase {
    const char* locale;
    ULayoutType expected;
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

void ExpectOrientation(const OrientationCase& item) {
    UErrorCode status = U_ZERO_ERROR;
    const auto result = uloc_getCharacterOrientation(item.locale, &status);
    if (result == item.expected && status == U_ZERO_ERROR) return;
    std::fprintf(stderr, "uloc_getCharacterOrientation(\"%s\") returned %d, status %d, expected %d\n",
                 item.locale ? item.locale : "null", static_cast<int>(result), static_cast<int>(status), static_cast<int>(item.expected));
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
    try {
        const auto expected = uloc_isRightToLeft_nid_postfix(uloc_getDefault());
        if (uloc_isRightToLeft_nid_postfix(nullptr) != expected) {
            std::fprintf(stderr, "uloc_isRightToLeft(null) differs from the default locale\n");
            std::abort();
        }
    } catch (const std::exception&) {
        ExpectThrows(nullptr);
    }
}

void CharacterOrientation() {
    for (const OrientationCase& item : std::initializer_list<OrientationCase>{
             {"en", ULOC_LAYOUT_LTR},
             {"en_US", ULOC_LAYOUT_LTR},
             {"fr", ULOC_LAYOUT_LTR},
             {"ja", ULOC_LAYOUT_LTR},
             {"zh_Hans", ULOC_LAYOUT_LTR},
             {"hi", ULOC_LAYOUT_LTR},
             {"sv", ULOC_LAYOUT_LTR},
             {"und", ULOC_LAYOUT_LTR},
             {"ar", ULOC_LAYOUT_RTL},
             {"he", ULOC_LAYOUT_RTL},
             {"fa", ULOC_LAYOUT_RTL},
             {"ur", ULOC_LAYOUT_RTL},
             {"ps_AF", ULOC_LAYOUT_RTL},
             {"ar_EG", ULOC_LAYOUT_RTL},
             {"iw", ULOC_LAYOUT_RTL},
             {"sr_Latn", ULOC_LAYOUT_LTR},
             {"sr_Arab", ULOC_LAYOUT_RTL},
             {"az_Arab", ULOC_LAYOUT_RTL},
             {"", ULOC_LAYOUT_LTR},
             {nullptr, ULOC_LAYOUT_LTR}}) {
        ExpectOrientation(item);
    }
    if (uloc_getCharacterOrientation("en", nullptr) != ULOC_LAYOUT_UNKNOWN) {
        std::fprintf(stderr, "uloc_getCharacterOrientation with null status did not return ULOC_LAYOUT_UNKNOWN\n");
        std::abort();
    }
    if (uloc_getCharacterOrientation("ar", nullptr) != ULOC_LAYOUT_UNKNOWN) {
        std::fprintf(stderr, "uloc_getCharacterOrientation with null status and RTL locale did not return ULOC_LAYOUT_UNKNOWN\n");
        std::abort();
    }
    UErrorCode status = U_ILLEGAL_ARGUMENT_ERROR;
    const auto preset = uloc_getCharacterOrientation("en", &status);
    if (preset != ULOC_LAYOUT_UNKNOWN || status != U_ILLEGAL_ARGUMENT_ERROR) {
        std::fprintf(stderr, "uloc_getCharacterOrientation with preset failure status failed\n");
        std::abort();
    }
    status = U_ILLEGAL_ARGUMENT_ERROR;
    const auto presetRtl = uloc_getCharacterOrientation("ar", &status);
    if (presetRtl != ULOC_LAYOUT_UNKNOWN || status != U_ILLEGAL_ARGUMENT_ERROR) {
        std::fprintf(stderr, "uloc_getCharacterOrientation with preset failure status for RTL failed\n");
        std::abort();
    }
    status = U_ZERO_ERROR;
    if (uloc_getCharacterOrientation("abcdefghijkl_Arab", &status) != ULOC_LAYOUT_UNKNOWN) {
        std::fprintf(stderr, "uloc_getCharacterOrientation with invalid locale did not return ULOC_LAYOUT_UNKNOWN\n");
        std::abort();
    }
}

}

int main() {
    Scripts();
    CommonLanguages();
    LikelySubtags();
    CharacterOrientation();
    return 0;
}
