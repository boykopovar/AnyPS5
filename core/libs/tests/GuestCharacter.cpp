#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cctype>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
int APS5_VABI isascii_nid_postfix(int);
int APS5_VABI toascii_nid_postfix(int);
int APS5_VABI toupper_nid_postfix(int);
int APS5_VABI tolower_nid_postfix(int);
const short* APS5_VABI _Getptolower_nid_postfix();
const short* APS5_VABI _Getptoupper_nid_postfix();
const char* APS5_VABI setlocale_nid_postfix(int, const char*);
}
static void Require(bool value) { if (!value) std::abort(); }
static int ConsoleClass(int c) {
    if (c < 0 || c > 255) return 0;
    int bits = 0;
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) bits |= 0x01;
    if (c >= 'A' && c <= 'Z') bits |= 0x02;
    if (c == ' ') bits |= 0x04;
    if (c >= 33 && c <= 126 && !(c >= '0' && c <= '9') && !(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z')) bits |= 0x08;
    if (c >= 'a' && c <= 'z') bits |= 0x10;
    if (c >= '0' && c <= '9') bits |= 0x20;
    if (c >= '\t' && c <= '\r') bits |= 0x40;
    if (c < 32 || c == 127) bits |= 0x80;
    if (c == '\t') bits |= 0x400;
    return bits;
}
static void CheckConsoleClassValues() {
    Require(isspace_nid_postfix(' ') == 0x04 && isspace_nid_postfix('\t') == 0x40 && isspace_nid_postfix('\n') == 0x40);
    Require(isblank_nid_postfix(' ') == 0x04 && isblank_nid_postfix('\t') == 0x400 && iscntrl_nid_postfix('\t') == 0x80);
    Require(isdigit_nid_postfix('0') == 0x20 && isalnum_nid_postfix('0') == 0x20 && isxdigit_nid_postfix('0') == 0x01);
    Require(isupper_nid_postfix('A') == 0x02 && isalpha_nid_postfix('A') == 0x02 && isxdigit_nid_postfix('F') == 0x01 && isxdigit_nid_postfix('G') == 0);
    Require(islower_nid_postfix('a') == 0x10 && isalpha_nid_postfix('a') == 0x10 && isprint_nid_postfix(' ') == 0x04 && isgraph_nid_postfix(' ') == 0);
    Require(ispunct_nid_postfix('_') == 0x08 && isgraph_nid_postfix('_') == 0x08 && isprint_nid_postfix('_') == 0x08 && iscntrl_nid_postfix(127) == 0x80);
    for (int c = EOF; c <= 255; ++c) {
        const int bits = ConsoleClass(c);
        Require(isupper_nid_postfix(c) == (bits & 0x02) && islower_nid_postfix(c) == (bits & 0x10) && isalpha_nid_postfix(c) == (bits & 0x12));
        Require(isdigit_nid_postfix(c) == (bits & 0x20) && isalnum_nid_postfix(c) == (bits & 0x32) && isxdigit_nid_postfix(c) == (bits & 0x01));
        Require(isspace_nid_postfix(c) == (bits & 0x44) && isblank_nid_postfix(c) == (bits & 0x404) && iscntrl_nid_postfix(c) == (bits & 0x80));
        Require(isprint_nid_postfix(c) == (bits & 0x3e) && isgraph_nid_postfix(c) == (bits & 0x3a) && ispunct_nid_postfix(c) == (bits & 0x08));
    }
}
static void CheckSignedCharValues() {
    for (int c = -128; c <= -2; ++c) {
        Require(isupper_nid_postfix(c) == 0 && islower_nid_postfix(c) == 0 && isalpha_nid_postfix(c) == 0 && isdigit_nid_postfix(c) == 0);
        Require(isalnum_nid_postfix(c) == 0 && isspace_nid_postfix(c) == 0 && isblank_nid_postfix(c) == 0 && iscntrl_nid_postfix(c) == 0);
        Require(isprint_nid_postfix(c) == 0 && isgraph_nid_postfix(c) == 0 && ispunct_nid_postfix(c) == 0 && isxdigit_nid_postfix(c) == 0);
        Require(toupper_nid_postfix(c) == c && tolower_nid_postfix(c) == c);
    }
}

int main() {
    CheckConsoleClassValues();
    CheckSignedCharValues();
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
