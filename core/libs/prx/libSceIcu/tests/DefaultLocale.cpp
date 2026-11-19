#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&uloc_getDefault), const char* (APS5_VABI*)()>);
static_assert(std::is_same_v<decltype(&uloc_setDefault_nid_postfix), void (APS5_VABI*)(const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&uprv_getDefaultLocaleID_nid_postfix), const char* (APS5_VABI*)()>);

void SetEnvironment(const char* name, const char* value) {
    Require(setenv_nid_postfix(name, value, 1) == 0);
}

void Check(std::string_view systemId, std::string_view defaultName) {
    const char* id = uprv_getDefaultLocaleID_nid_postfix();
    Require(id == systemId);
    Require(uprv_getDefaultLocaleID_nid_postfix() == id);

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
    RequireThrows([&] { uloc_setDefault_nid_postfix("fr_FR", &status); });
    RequireThrows([] { uloc_setDefault_nid_postfix(nullptr, nullptr); });
    Require(uloc_getDefault() == name);
}

void CheckName(std::int32_t (APS5_VABI* function)(const char*, char*, std::int32_t, std::int32_t*),
               const char* locale, std::string_view expected) {
    std::array<char, 64> buffer{};
    std::int32_t status = 0;
    const auto length = function(locale, buffer.data(), static_cast<std::int32_t>(buffer.size()), &status);
    Require(status == 0);
    Require(std::string_view(buffer.data(), static_cast<std::size_t>(length)) == expected);
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
    } else if (scenario == "i-default") {
        SetEnvironment("LANG", "fr_FR");
        Check("fr_FR", "fr_FR");
        CheckName(uloc_getBaseName, "i-default", "fr_FR");
        CheckName(uloc_getBaseName, "i-default.utf8", "fr_FR.utf8");
        CheckName(uloc_canonicalize, "i-default", "fr_FR");
        CheckName(uloc_canonicalize, "i-default@currency=EUR", "fr_FR@currency=EUR");
    } else if (scenario == "i-default-system") {
        SetEnvironment("LANG", "i-default");
        Check("i-default", "i-default");
        CheckName(uloc_getBaseName, "i-default", "i-default");
        CheckName(uloc_getBaseName, "i-default.utf8", "i-default.utf8");
        CheckName(uloc_canonicalize, "i-default", "i-default");
    } else {
        std::fprintf(stderr, "unknown default locale scenario\n");
        return 1;
    }
    return 0;
}
