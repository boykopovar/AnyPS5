// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {
void Check(bool condition) { if (!condition) std::abort(); }
}

int main() {
    Check(std::strcmp(uloc_getISO3Language_nid_postfix("eng_US"), "eng") == 0);
    Check(std::strcmp(uloc_getISO3Language_nid_postfix("iw_IL"), "heb") == 0);
    Check(std::strcmp(uloc_getISO3Language_nid_postfix("root"), "") == 0);
    Check(std::strcmp(uloc_getISO3Country_nid_postfix("en_Latn_USA"), "USA") == 0);
    Check(std::strcmp(uloc_getISO3Country_nid_postfix("en_XX"), "") == 0);
    Check(std::strcmp(uloc_getISOLanguages_nid_postfix()[0], "aa") == 0);
    Check(std::strcmp(uloc_getISOCountries_nid_postfix()[0], "AD") == 0);
    std::array<char, 40> buffer{};
    std::strcpy(buffer.data(), "und_Latn_US");
    std::int32_t status = 0;
    Check(uloc_getParent_nid_postfix(buffer.data(), buffer.data(), buffer.size(), &status) == 5);
    Check(status == 0 && std::strcmp(buffer.data(), "_Latn") == 0);
    status = 0;
    Check(uloc_getParent_nid_postfix("en_US", buffer.data(), 2, &status) == 2 && status == -124);
    status = 0;
    Check(uloc_getParent_nid_postfix("en_US", nullptr, 0, &status) == 2 && status == 15);
    status = 7;
    Check(uloc_getParent_nid_postfix(nullptr, nullptr, -1, &status) == 0 && status == 7);
    bool invalid = false;
    try { uloc_getParent_nid_postfix("en_US", nullptr, 1, &status); } catch (const std::invalid_argument&) { invalid = true; }
    Check(!invalid);
    status = 0;
    try { uloc_getParent_nid_postfix("en_US", nullptr, 1, &status); } catch (const std::invalid_argument&) { invalid = true; }
    Check(invalid);
    std::strcpy(buffer.data(), "en_US_POSIX");
    invalid = false;
    try { uloc_getParent_nid_postfix(buffer.data(), buffer.data() + 1, 10, &status); } catch (const std::invalid_argument&) { invalid = true; }
    Check(invalid);
    std::puts("ICU locale ID tests passed");
}
