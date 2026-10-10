#include "prx/libc/include/general/VabiMacros.hpp"
#include <cctype>
#include <climits>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
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
    }
    int (APS5_VABI* const classifiers[])(int) = {
        isupper_nid_postfix, islower_nid_postfix, isalpha_nid_postfix, isdigit_nid_postfix, isalnum_nid_postfix,
        isspace_nid_postfix, isblank_nid_postfix, iscntrl_nid_postfix, isprint_nid_postfix, isgraph_nid_postfix,
        ispunct_nid_postfix, isxdigit_nid_postfix,
    };
    for (const int c : {-2, -23, -128, -129, 256, 257, 0x100, 0x1e9, 0xffff, 0x10000, INT_MAX, INT_MIN}) {
        for (const auto classify : classifiers) Require(classify(c) == 0);
        Require(toupper_nid_postfix(c) == c);
        Require(tolower_nid_postfix(c) == c);
    }
}
