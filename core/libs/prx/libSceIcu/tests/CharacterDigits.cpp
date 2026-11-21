#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "ICU character digit check failed at line %u\n", location.line());
    std::abort();
}

int main() {
    for (std::int32_t digit = 0; digit < 10; ++digit) {
        Require(u_charDigitValue_nid_postfix('0' + digit) == digit);
        Require(u_charDigitValue_nid_postfix(0x660 + digit) == digit);
        Require(u_charDigitValue_nid_postfix(0xff10 + digit) == digit);
        Require(u_digit_nid_postfix('0' + digit, 10) == digit);
    }
    Require(u_charDigitValue_nid_postfix(0x1d7ce) == 0);
    Require(u_charDigitValue_nid_postfix(0x1d7ff) == 9);
    Require(u_charDigitValue_nid_postfix(0x116e3) == 9);
    Require(u_charDigitValue_nid_postfix(-1) == -1);
    Require(u_charDigitValue_nid_postfix(0x110000) == -1);
    Require(u_digit_nid_postfix('A', 10) == -1);
    Require(u_digit_nid_postfix('A', 16) == 10);
    Require(u_digit_nid_postfix('z', 36) == 35);
    Require(u_digit_nid_postfix(0xff21, 10) == -1);
    Require(u_digit_nid_postfix(0xff21, 16) == 10);
    Require(u_digit_nid_postfix(0xff5a, 35) == -1);
    Require(u_digit_nid_postfix(0xff5a, 36) == 35);
    Require(u_digit_nid_postfix('0', 1) == -1);
    Require(u_digit_nid_postfix('0', 37) == -1);
}
