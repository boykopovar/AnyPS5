#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<decltype(&locale_getKeywordsStart_nid_postfix), const char* (APS5_VABI*)(const char*)>);
static_assert(std::is_same_v<decltype(&ulocale_openForLocaleID_nid_postfix), ULocale* (APS5_VABI*)(const char*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ulocale_close_nid_postfix), void (APS5_VABI*)(ULocale*)>);
static_assert(std::is_same_v<decltype(&ulocale_getLocaleID_nid_postfix), const char* (APS5_VABI*)(const ULocale*)>);
static_assert(std::is_same_v<decltype(&ulocale_isBogus_nid_postfix), bool (APS5_VABI*)(const ULocale*)>);
static_assert(std::is_same_v<decltype(&ulocbld_buildLocaleID_nid_postfix),
                             std::int32_t (APS5_VABI*)(ULocaleBuilder*, char*, std::int32_t, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&ulocbld_buildLanguageTag_nid_postfix),
                             std::int32_t (APS5_VABI*)(ULocaleBuilder*, char*, std::int32_t, std::int32_t*)>);

namespace {

void Check(const char* localeID, const char* expected) {
    const char* actual = locale_getKeywordsStart_nid_postfix(localeID);
    if (actual != expected) {
        std::fprintf(stderr, "locale \"%s\": expected offset %td, got %td\n", localeID, expected - localeID, actual - localeID);
        std::abort();
    }
}

void CheckNullThrows() {
    try {
        locale_getKeywordsStart_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    std::fputs("a null locale did not throw\n", stderr);
    std::abort();
}

void CheckLocaleObject() {
    std::int32_t error = 0;
    ULocale* locale = ulocale_openForLocaleID_nid_postfix("en_US_extra", 5, &error);
    if (!locale || error != 0 || std::string(ulocale_getLocaleID_nid_postfix(locale)) != "en_US" ||
        ulocale_isBogus_nid_postfix(locale)) {
        std::fputs("locale object did not preserve the bounded locale ID\n", stderr);
        std::abort();
    }
    ulocale_close_nid_postfix(locale);
    ulocale_close_nid_postfix(nullptr);
    if (ulocale_getLocaleID_nid_postfix(nullptr) != nullptr || ulocale_isBogus_nid_postfix(nullptr)) {
        std::fputs("null locale object getters changed ICU behavior\n", stderr);
        std::abort();
    }
    bool threw = false;
    try {
        ulocale_openForLocaleID_nid_postfix("en", -1, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) {
        std::fputs("a null locale object status did not throw\n", stderr);
        std::abort();
    }
}

void CheckLocaleQueries() {
    std::int32_t error = 0;
    ULocale* locale = ulocale_openForLanguageTag_nid_postfix("sr-Latn-RS-u-ca-gregory", -1, &error);
    if (!locale || error != 0 || std::strcmp(ulocale_getLocaleID_nid_postfix(locale), "sr_Latn_RS@ca=gregory") != 0 ||
        std::strcmp(ulocale_getBaseName_nid_postfix(locale), "sr_Latn_RS") != 0 ||
        std::strcmp(ulocale_getLanguage_nid_postfix(locale), "sr") != 0 ||
        std::strcmp(ulocale_getScript_nid_postfix(locale), "Latn") != 0 ||
        std::strcmp(ulocale_getRegion_nid_postfix(locale), "RS") != 0 ||
        std::strcmp(ulocale_getVariant_nid_postfix(locale), "") != 0) {
        std::fputs("locale query components differ\n", stderr);
        std::abort();
    }
    char value[32] = {};
    if (ulocale_getKeywordValue_nid_postfix(locale, "ca", -1, value, 32, &error) != 7 ||
        error != 0 || std::strcmp(value, "gregory") != 0) {
        std::fputs("locale keyword value differs\n", stderr);
        std::abort();
    }
    if (ulocale_getUnicodeKeywordValue_nid_postfix(locale, "ca", 2, value, 32, &error) != 7 ||
        error != 0 || std::strcmp(value, "gregory") != 0) {
        std::fputs("locale Unicode keyword value differs\n", stderr);
        std::abort();
    }
    UEnumeration* keywords = ulocale_getKeywords_nid_postfix(locale, &error);
    std::int32_t length = 0;
    const char* keyword = uenum_next_nid_postfix(keywords, &length, &error);
    if (!keyword || length != 2 || std::string(keyword, static_cast<std::size_t>(length)) != "ca" ||
        uenum_next_nid_postfix(keywords, &length, &error) != nullptr || error != 0) {
        std::fputs("locale keyword enumeration differs\n", stderr);
        std::abort();
    }
    uenum_close_nid_postfix(keywords);
    keywords = ulocale_getUnicodeKeywords_nid_postfix(locale, &error);
    keyword = uenum_next_nid_postfix(keywords, &length, &error);
    if (!keyword || length != 2 || std::string(keyword, static_cast<std::size_t>(length)) != "ca" ||
        uenum_next_nid_postfix(keywords, &length, &error) != nullptr || error != 0) {
        std::fputs("locale Unicode keyword enumeration differs\n", stderr);
        std::abort();
    }
    uenum_close_nid_postfix(keywords);
    ulocale_close_nid_postfix(locale);
    ULocaleBuilder* builder = ulocbld_open_nid_postfix();
    if (!builder) {
        std::fputs("locale builder allocation failed\n", stderr);
        std::abort();
    }
    ulocbld_close_nid_postfix(builder);
}

void CheckLocaleBuilder() {
    ULocaleBuilder* builder = ulocbld_open_nid_postfix();
    if (!builder) {
        std::fputs("locale builder allocation failed\n", stderr);
        std::abort();
    }
    ulocbld_setLanguage_nid_postfix(builder, "sr-extra", 2);
    ulocbld_setScript_nid_postfix(builder, "latn-extra", 4);
    ulocbld_setRegion_nid_postfix(builder, "rs-extra", 2);
    ulocbld_setVariant_nid_postfix(builder, "revised-extra", 7);
    std::int32_t error = 0;
    char locale[64] = {};
    char language[64] = {};
    if (ulocbld_buildLocaleID_nid_postfix(builder, locale, 64, &error) != 18 || error != 0 ||
        std::strcmp(locale, "sr_Latn_RS_REVISED") != 0 ||
        ulocbld_buildLanguageTag_nid_postfix(builder, language, 64, &error) != 18 || error != 0 ||
        std::strcmp(language, "sr-Latn-RS-revised") != 0) {
        std::fputs("locale builder components differ\n", stderr);
        std::abort();
    }
    ulocbld_clear_nid_postfix(builder);
    ulocbld_setLanguageTag_nid_postfix(builder, "de-DE-u-ca-gregory-extra", 18);
    ulocbld_clearExtensions_nid_postfix(builder);
    if (ulocbld_buildLocaleID_nid_postfix(builder, locale, 64, &error) != 5 || error != 0 ||
        std::strcmp(locale, "de_DE") != 0) {
        std::fputs("locale builder extension clearing differs\n", stderr);
        std::abort();
    }
    ulocbld_setLocale_nid_postfix(builder, "fr_CA_extra", 5);
    if (ulocbld_buildLanguageTag_nid_postfix(builder, language, 64, &error) != 5 || error != 0 ||
        std::strcmp(language, "fr-CA") != 0) {
        std::fputs("locale builder locale reset differs\n", stderr);
        std::abort();
    }
    bool threw = false;
    try {
        ulocbld_buildLocaleID_nid_postfix(builder, locale, 64, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) {
        std::fputs("a null locale builder status did not throw\n", stderr);
        std::abort();
    }
    ulocbld_close_nid_postfix(builder);
    ulocbld_close_nid_postfix(nullptr);
}

}

int main() {
    const char keywords[] = "de_DE@collation=phonebook;currency=EUR";
    Check(keywords, keywords + 5);
    const char twoSigns[] = "en@a=b@c=d";
    Check(twoSigns, twoSigns + 2);
    const char leading[] = "@calendar=buddhist";
    Check(leading, leading);
    const char trailing[] = "sr_Latn_RS@";
    Check(trailing, trailing + 10);
    const char highBytes[] = "\xff\x80@x=y";
    Check(highBytes, highBytes + 2);
    const char pastTerminator[] = "fr\0@x=y";
    Check(pastTerminator, nullptr);
    Check("", nullptr);
    Check("en_US_POSIX", nullptr);
    Check("ja_JP;calendar=japanese", nullptr);

    for (const std::size_t position : {0u, 1u, 4095u, 65535u}) {
        std::string locale(65536, 'a');
        locale[position] = '@';
        Check(locale.c_str(), locale.c_str() + position);
    }
    const std::string noKeywords(65536, 'a');
    Check(noKeywords.c_str(), nullptr);

    CheckNullThrows();
    CheckLocaleObject();
    CheckLocaleQueries();
    CheckLocaleBuilder();
    std::puts("ICU locale keyword tests passed");
}
