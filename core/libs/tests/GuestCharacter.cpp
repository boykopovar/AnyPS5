#include "prx/libc/include/general/VabiMacros.hpp"
#include <cctype>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
extern "C" {
int APS5_VABI isupper_nid_postfix(int);
int APS5_VABI islower_nid_postfix(int);
int APS5_VABI isalpha_nid_postfix(int);
int APS5_VABI isdigit_nid_postfix(int);
int APS5_VABI isalnum_nid_postfix(int);
int APS5_VABI isspace_nid_postfix(int);
int APS5_VABI isblank_nid_postfix(int);
int APS5_VABI iscntrl_nid_postfix(int);
int APS5_VABI isprint_nid_postfix(int);
int APS5_VABI isgraph_nid_postfix(int);
int APS5_VABI ispunct_nid_postfix(int);
int APS5_VABI isxdigit_nid_postfix(int);
int APS5_VABI toupper_nid_postfix(int);
int APS5_VABI tolower_nid_postfix(int);
const short* APS5_VABI _Getptolower_nid_postfix();
const short* APS5_VABI _Getptoupper_nid_postfix();
int APS5_VABI _Iswctype_nid_postfix(std::uint32_t, std::uint64_t);
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    Require(std::setlocale(LC_CTYPE, "C") != nullptr);
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
        Require(toupper_nid_postfix(c) == std::toupper(c));
        Require(tolower_nid_postfix(c) == std::tolower(c));
        Require(toupper_nid_postfix(c) == _Getptoupper_nid_postfix()[c]);
        Require(tolower_nid_postfix(c) == _Getptolower_nid_postfix()[c]);
        const auto wide = static_cast<std::uint32_t>(c);
        Require(_Iswctype_nid_postfix(wide, 0) == 0);
        Require(bool(_Iswctype_nid_postfix(wide, 1)) == bool(std::isalnum(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 2)) == bool(std::isalpha(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 3)) == bool(std::iscntrl(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 4)) == bool(std::isdigit(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 5)) == bool(std::isgraph(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 6)) == bool(std::islower(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 7)) == bool(std::isprint(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 8)) == bool(std::ispunct(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 9)) == bool(std::isspace(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 10)) == bool(std::isupper(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 11)) == bool(std::isxdigit(c)));
        Require(bool(_Iswctype_nid_postfix(wide, 12)) == bool(std::isblank(c)));
    }
    Require(_Iswctype_nid_postfix(u' ', 9) == 1 && _Iswctype_nid_postfix(u'\t', 12) == 1);
    Require(_Iswctype_nid_postfix(u'7', 4) == 1 && _Iswctype_nid_postfix(u'f', 11) == 1);
    for (std::uint32_t c = 256; c <= 0xffff; ++c)
        for (std::uint64_t desc = 0; desc <= 12; ++desc) Require(_Iswctype_nid_postfix(c, desc) == 0);
    Require(_Iswctype_nid_postfix(0x3000, 9) == 0 && _Iswctype_nid_postfix(0x10000, 2) == 0);
    bool threw = false;
    try {
        _Iswctype_nid_postfix(u'a', 13);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}
