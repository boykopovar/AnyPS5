#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include "prx/libc/include/Api.hpp"
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
