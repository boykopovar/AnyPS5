#include "prx/libSceIcu/libSceIcu.hpp"
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
    std::puts("ICU locale keyword tests passed");
}
