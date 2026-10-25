#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cctype>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }
int main() {
    Require(std::setlocale(LC_CTYPE, "C") != nullptr);
    for (int category = 0; category <= 6; ++category) {
        const char* result = setlocale_nid_postfix(category, "C");
        Require(result != nullptr && std::strcmp(result, "C") == 0);
        Require(setlocale_nid_postfix(category, "POSIX") == nullptr);
        result = setlocale_nid_postfix(category, nullptr);
        Require(result != nullptr && std::strcmp(result, "C") == 0);
        result = setlocale_nid_postfix(category, "");
        Require(result != nullptr && std::strcmp(result, "C") == 0);
        Require(setlocale_nid_postfix(category, "en_US.UTF-8") == nullptr);
    }
    for (const int category : std::array<int, 4>{-1, 7, 100, 1 << 30}) {
        const char* result = setlocale_nid_postfix(category, "C");
        Require(result != nullptr && std::strcmp(result, "C") == 0);
        result = setlocale_nid_postfix(category, nullptr);
        Require(result != nullptr && std::strcmp(result, "C") == 0);
        Require(setlocale_nid_postfix(category, "POSIX") == nullptr && setlocale_nid_postfix(category, "x") == nullptr);
    }
    for (int c = EOF; c <= 255; ++c) {
        Require(bool(isupper_nid_postfix(c)) == bool(std::isupper(c)));
        Require(bool(islower_nid_postfix(c)) == bool(std::islower(c)));
        Require(bool(isalpha_nid_postfix(c)) == bool(std::isalpha(c)));
        Require(bool(isdigit_nid_postfix(c)) == bool(std::isdigit(c)));
        Require(bool(isalnum_nid_postfix(c)) == bool(std::isalnum(c)));
        Require(bool(isspace_nid_postfix(c)) == bool(std::isspace(c)));
        Require(bool(isblank_nid_postfix(c)) == bool(std::isblank(c)));
        Require(bool(iscntrl_nid_postfix(c)) == bool(std::iscntrl(c)));
        Require(bool(isprint_nid_postfix(c)) == bool(std::isprint(c)));
        Require(bool(isgraph_nid_postfix(c)) == bool(std::isgraph(c)));
        Require(bool(ispunct_nid_postfix(c)) == bool(std::ispunct(c)));
        Require(bool(isxdigit_nid_postfix(c)) == bool(std::isxdigit(c)));
        Require(bool(isascii_nid_postfix(c)) == (c >= 0 && c <= 127));
        Require(toascii_nid_postfix(c) == (c & 0x7F));
        Require(toupper_nid_postfix(c) == std::toupper(c));
        Require(tolower_nid_postfix(c) == std::tolower(c));
        Require(toupper_nid_postfix(c) == _Getptoupper_nid_postfix()[c]);
        Require(tolower_nid_postfix(c) == _Getptolower_nid_postfix()[c]);
    }
    Require(!isascii_nid_postfix(-100));
    Require(!isascii_nid_postfix(128));
    Require(!isascii_nid_postfix(256));
    Require(toascii_nid_postfix(258) == 2);
    Require(toascii_nid_postfix(-1) == 0x7F);
}
