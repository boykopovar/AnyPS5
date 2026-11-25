#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <array>
#include <cerrno>
#include <string>

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&uloc_getDefault), const char* (APS5_VABI*)()>);
static_assert(std::is_same_v<decltype(&uloc_setDefault_nid_postfix), void (APS5_VABI*)(const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&uprv_getDefaultLocaleID_nid_postfix), const char* (APS5_VABI*)()>);

void SetEnvironment(const char* name, const char* value) {
    Require(setenv_nid_postfix(name, value, 1) == 0);
}

std::string DefaultLanguage() {
    std::array<char, 32> buffer{};
    std::int32_t status = 0;
    const std::int32_t length = uloc_getLanguage(nullptr, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
    Require(status <= 0 && length >= 0 && length < static_cast<std::int32_t>(buffer.size()));
    return std::string(buffer.data(), static_cast<std::size_t>(length));
}

std::string DefaultCountry() {
    std::array<char, 32> buffer{};
    std::int32_t status = 0;
    const std::int32_t length = uloc_getCountry(nullptr, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
    Require(status <= 0 && length >= 0 && length < static_cast<std::int32_t>(buffer.size()));
    return std::string(buffer.data(), static_cast<std::size_t>(length));
}

void CheckExplicitBeforeFirstGet() {
    const std::string overlongCharset = "ja_JP." + std::string(65, 'z');
    std::int32_t status = 0;
    uloc_setDefault_nid_postfix(overlongCharset.c_str(), &status);
    Require(status == 0);

    uloc_setDefault_nid_postfix("JA-jp", &status);
    Require(status == 0);
    const char* japanese = uloc_getDefault();
    Require(std::string_view(japanese) == "ja_JP");
    Require(DefaultLanguage() == "ja" && DefaultCountry() == "JP");

    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(status == 0);
    Require(std::string_view(japanese) == "ja_JP");
}

void CheckExplicitLocaleIds(const char* name, std::string_view defaultName) {
    const auto* unreadableLocale = reinterpret_cast<const char*>(std::uintptr_t{1});
    auto* unreadableStatus = reinterpret_cast<std::int32_t*>(std::uintptr_t{1});

    std::int32_t status = 1;
    uloc_setDefault_nid_postfix(unreadableLocale, &status);
    Require(status == 1 && uloc_getDefault() == name);

    errno = 73;
    status = 0;
    uloc_setDefault_nid_postfix("EN-us", &status);
    Require(status == 0 && errno == 73);
    const char* englishUs = uloc_getDefault();
    Require(std::string_view(englishUs) == "en_US");
    Require(std::string_view(name) == defaultName);
    Require(DefaultLanguage() == "en" && DefaultCountry() == "US");

    status = -128;
    uloc_setDefault_nid_postfix("sr-Latn-RS", &status);
    Require(status == -128);
    Require(std::string_view(uloc_getDefault()) == "sr_Latn_RS");
    Require(std::string_view(englishUs) == "en_US");

    status = 0;
    uloc_setDefault_nid_postfix("de_DE@collation=phonebook;calendar=gregorian", &status);
    Require(status == 0);
    Require(std::string_view(uloc_getDefault()) == "de_DE@calendar=gregorian;collation=phonebook");

    status = 0;
    uloc_setDefault_nid_postfix("en-US-x-private", &status);
    Require(status == 0);
    Require(std::string_view(uloc_getDefault()) == "en_US@x=private");

    status = 0;
    uloc_setDefault_nid_postfix("en_US.UTF-8", &status);
    Require(status == 0);
    Require(std::string_view(uloc_getDefault()) == "en_US.UTF-8");

    const std::string longValue(300, 'a');
    const std::string longLocale = "fr_FR@x=" + longValue;
    status = 0;
    uloc_setDefault_nid_postfix(longLocale.c_str(), &status);
    Require(status == 0);
    Require(std::string_view(uloc_getDefault()) == longLocale);

    status = 0;
    uloc_setDefault_nid_postfix("ar_EG", &status);
    Require(status == 0);
    Require(std::string_view(uloc_getDefault()) == "ar_EG");
    Require(uloc_isRightToLeft_nid_postfix(nullptr) == 1);

    status = 0;
    uloc_setDefault_nid_postfix("tr_TR", &status);
    Require(status == 0);
    Require(std::string_view(uloc_getDefault()) == "tr_TR");
    Require(std::string_view(uloc_getISO3Language(nullptr)) == "tur");
    Require(uloc_isRightToLeft_nid_postfix(nullptr) == 0);

    const std::string overlongCharset = "en_US." + std::string(65, 'x');
    status = -128;
    uloc_setDefault_nid_postfix(overlongCharset.c_str(), &status);
    Require(status == -128);
    Require(std::string_view(uloc_getDefault()) == "tr_TR");

    status = 0;
    uloc_setDefault_nid_postfix("abcdefghijkl_US", &status);
    Require(status == 0);
    Require(std::string_view(uloc_getDefault()) == "tr_TR");

    status = 0;
    uloc_setDefault_nid_postfix("abcdefghijk_US", &status);
    Require(status == 0);
    Require(std::string_view(uloc_getDefault()) == "abcdefghijk_US");

    status = 0;
    uloc_setDefault_nid_postfix("en_US", &status);
    Require(status == 0);
    Require(uloc_getDefault() == englishUs);

    status = 0;
    uloc_setDefault_nid_postfix("", &status);
    Require(status == 0);
    Require(std::string_view(uloc_getDefault()).empty());

    status = 0;
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(status == 0);
    Require(uloc_getDefault() == name);

    errno = 73;
    status = 0;
    RequireThrows([&] { uloc_setDefault_nid_postfix("fr_FR", unreadableStatus); });
    RequireThrows([&] { uloc_setDefault_nid_postfix(unreadableLocale, &status); });
    Require(status == 0 && errno == 73);
    Require(uloc_getDefault() == name);
}

void Check(std::string_view systemId, std::string_view defaultName) {
    const char* id = uprv_getDefaultLocaleID_nid_postfix();
    Require(id == systemId);
    Require(uprv_getDefaultLocaleID_nid_postfix() == id);

    CheckExplicitBeforeFirstGet();

    const char* name = uloc_getDefault();
    Require(name == defaultName);
    Require(uloc_getDefault() == name);

    SetEnvironment("LC_ALL", "ja_JP");
    Require(uprv_getDefaultLocaleID_nid_postfix() == id);

    std::int32_t status = 1;
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(status == 1);
    Require(uloc_getDefault() == name);

    status = 0;
    uloc_setDefault_nid_postfix(nullptr, &status);
    Require(status == 0);
    Require(uloc_getDefault() == name);

    status = 0;
    CheckExplicitLocaleIds(name, defaultName);
    RequireThrows([] { uloc_setDefault_nid_postfix(nullptr, nullptr); });
    Require(uloc_getDefault() == name);
}

}

int main(int argc, char** argv) {
    const std::string_view scenario = argc > 1 ? argv[1] : "";
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        Require(unsetenv_nid_postfix(name) == 0);
    }
    if (scenario == "posix") {
        Check("en_US_POSIX", "en_US_POSIX");
    } else if (scenario == "messages") {
        SetEnvironment("LANG", "fr_FR");
        SetEnvironment("LC_MESSAGES", "sr_RS.UTF-8@latin");
        Check("sr_RS_latin", "sr_RS_LATIN");
    } else if (scenario == "codeset") {
        SetEnvironment("LC_MESSAGES", "de_DE");
        SetEnvironment("LC_ALL", "C.UTF-8");
        Check("en_US_POSIX", "en_US_POSIX");
    } else if (scenario == "nynorsk") {
        SetEnvironment("LANG", "no@nynorsk");
        Check("no__NY", "no__NY");
    } else if (scenario == "long") {
        SetEnvironment("LANG", "abcdefghijkl_US");
        Check("abcdefghijkl_US", "");
    } else {
        std::fprintf(stderr, "unknown default locale scenario\n");
        return 1;
    }
    return 0;
}
