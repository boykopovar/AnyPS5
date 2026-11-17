#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace {

using IcuRecoveredTest::Require;
using LocaleFunction = std::int32_t (APS5_VABI*)(const char*, char*, std::int32_t, std::int32_t*);
using CaseFunction = std::int32_t (APS5_VABI*)(char16_t*, std::int32_t, const char16_t*, std::int32_t, const char*, std::int32_t*);

std::string Call(LocaleFunction function, const char* locale) {
    std::array<char, 64> buffer{};
    buffer.fill('Q');
    std::int32_t status = 0;
    const std::int32_t length = function(locale, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
    Require(status <= 0 && length >= 0 && length < static_cast<std::int32_t>(buffer.size()) && buffer[length] == '\0');
    return std::string(buffer.data(), static_cast<std::size_t>(length));
}

std::u16string Map(CaseFunction function, const char16_t* text, const char* locale) {
    std::array<char16_t, 16> buffer{};
    std::int32_t status = 0;
    const std::int32_t length = function(buffer.data(), static_cast<std::int32_t>(buffer.size()), text, -1, locale, &status);
    Require(status <= 0 && length >= 0 && length < static_cast<std::int32_t>(buffer.size()));
    return std::u16string(buffer.data(), static_cast<std::size_t>(length));
}

std::string Component(LocaleFunction function) {
    const std::string fromNull = Call(function, nullptr);
    Require(fromNull == Call(function, uloc_getDefault()));
    return fromNull;
}

std::u16string Mapped(CaseFunction function, const char16_t* text) {
    const std::u16string fromNull = Map(function, text, nullptr);
    Require(fromNull == Map(function, text, uloc_getDefault()));
    return fromNull;
}

std::string_view Iso3Language() {
    const std::string_view fromNull = uloc_getISO3Language(nullptr);
    Require(fromNull == uloc_getISO3Language(uloc_getDefault()));
    return fromNull;
}

std::string_view Iso3Country() {
    const std::string_view fromNull = uloc_getISO3Country(nullptr);
    Require(fromNull == uloc_getISO3Country(uloc_getDefault()));
    return fromNull;
}

std::int8_t RightToLeft() {
    const std::int8_t fromNull = uloc_isRightToLeft_nid_postfix(nullptr);
    Require(fromNull == uloc_isRightToLeft_nid_postfix(uloc_getDefault()));
    return fromNull;
}

void FailedStatusLeavesNullUnread() {
    std::array<char, 8> buffer{};
    buffer.fill('Q');
    for (const LocaleFunction function : {uloc_getLanguage, uloc_getCountry, uloc_getScript, uloc_getBaseName, uloc_canonicalize,
                                          uloc_getParent, uloc_getVariant_nid_postfix}) {
        std::int32_t status = 1;
        Require(function(nullptr, buffer.data(), 8, &status) == 0 && status == 1 && buffer[0] == 'Q');
    }
}

void SetEnvironment(const char* name, const char* value) {
    Require(setenv_nid_postfix(name, value, 1) == 0);
}

}

int main(int argc, char** argv) {
    const std::string_view scenario = argc > 1 ? argv[1] : "";
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) Require(unsetenv_nid_postfix(name) == 0);
    if (scenario == "turkish") {
        SetEnvironment("LANG", "tr_TR.UTF-8@traditional");
        Require(std::string_view(uloc_getDefault()) == "tr_TR_TRADITIONAL");
        Require(Component(uloc_getLanguage) == "tr");
        Require(Component(uloc_getCountry) == "TR");
        Require(Component(uloc_getScript).empty());
        Require(Component(uloc_getVariant_nid_postfix) == "TRADITIONAL");
        Require(Component(uloc_getBaseName) == "tr_TR_TRADITIONAL");
        Require(Component(uloc_canonicalize) == "tr_TR_TRADITIONAL");
        Require(Component(uloc_getParent) == "tr_TR");
        Require(Iso3Language() == "tur" && Iso3Country() == "TUR");
        Require(RightToLeft() == 0);
        Require(Mapped(u_strToUpper_nid_postfix, u"di") == u"Dİ");
        Require(Mapped(u_strToLower_nid_postfix, u"DI") == u"dı");
        Require(Map(u_strToUpper_nid_postfix, u"di", "") == u"DI");
    } else if (scenario == "arabic") {
        SetEnvironment("LANG", "fr_FR");
        SetEnvironment("LC_ALL", "ar_Arab_EG.UTF-8");
        Require(std::string_view(uloc_getDefault()) == "ar_Arab_EG");
        Require(Component(uloc_getLanguage) == "ar");
        Require(Component(uloc_getScript) == "Arab");
        Require(Component(uloc_getCountry) == "EG");
        Require(Component(uloc_getVariant_nid_postfix).empty());
        Require(Component(uloc_getBaseName) == "ar_Arab_EG");
        Require(Component(uloc_getParent) == "ar_Arab");
        Require(Iso3Language() == "ara" && Iso3Country() == "EGY");
        Require(RightToLeft() == 1);
        Require(Mapped(u_strToUpper_nid_postfix, u"di") == u"DI");
    } else if (scenario == "long") {
        SetEnvironment("LANG", "abcdefghijkl_US");
        Require(std::string_view(uloc_getDefault()).empty());
        for (const LocaleFunction function : {uloc_getLanguage, uloc_getCountry, uloc_getScript, uloc_getVariant_nid_postfix,
                                              uloc_getBaseName, uloc_canonicalize, uloc_getParent})
            Require(Component(function).empty());
        Require(Iso3Language().empty() && Iso3Country().empty());
        Require(Mapped(u_strToLower_nid_postfix, u"DI") == u"di");
    } else {
        std::fprintf(stderr, "unknown null locale scenario\n");
        return 1;
    }
    FailedStatusLeavesNullUnread();
    std::puts("ICU null locale tests passed");
    return 0;
}
