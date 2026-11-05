#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU character check failed at line %u\n", location.line());
    std::abort();
}

} // namespace

int main() {
    for (std::int32_t character = 0; character < 0x80; ++character) {
        const bool decimal = character >= 0x30 && character <= 0x39;
        Require(u_isdigit_nid_postfix(character) == (decimal ? 1 : 0));
        Require(u_charDigitValue_nid_postfix(character) == (decimal ? character - 0x30 : -1));
    }
    Require(u_charDigitValue_nid_postfix('0') == 0 && u_charDigitValue_nid_postfix('9') == 9);
    Require(u_charDigitValue_nid_postfix('A') == -1 && u_charDigitValue_nid_postfix(0) == -1);
    Require(u_isdigit_nid_postfix(0x660) == 1 && u_charDigitValue_nid_postfix(0x660) == 0);
    Require(u_isdigit_nid_postfix(0x669) == 1 && u_charDigitValue_nid_postfix(0x669) == 9);
    Require(u_isdigit_nid_postfix(0x6F0) == 1 && u_charDigitValue_nid_postfix(0x6F0) == 0);
    Require(u_isdigit_nid_postfix(0xFF19) == 1 && u_charDigitValue_nid_postfix(0xFF19) == 9);
    Require(u_isdigit_nid_postfix(0x104A9) == 1 && u_charDigitValue_nid_postfix(0x104A9) == 9);
    Require(u_isdigit_nid_postfix(0x1D7FF) == 1 && u_charDigitValue_nid_postfix(0x1D7FF) == 9);
    Require(u_isdigit_nid_postfix(-1) == 0 && u_charDigitValue_nid_postfix(-1) == -1);
    Require(u_isdigit_nid_postfix(0x110000) == 0 && u_charDigitValue_nid_postfix(0x110000) == -1);
    Require(u_isxdigit_nid_postfix('0') == 1 && u_isxdigit_nid_postfix('9') == 1);
    Require(u_isxdigit_nid_postfix('A') == 1 && u_isxdigit_nid_postfix('F') == 1 && u_isxdigit_nid_postfix('G') == 0);
    Require(u_isxdigit_nid_postfix('a') == 1 && u_isxdigit_nid_postfix('f') == 1 && u_isxdigit_nid_postfix('g') == 0);
    Require(u_isxdigit_nid_postfix(0xFF21) == 1 && u_isxdigit_nid_postfix(0xFF26) == 1 && u_isxdigit_nid_postfix(0xFF27) == 0);
    Require(u_isxdigit_nid_postfix(0xFF41) == 1 && u_isxdigit_nid_postfix(0xFF46) == 1 && u_isxdigit_nid_postfix(0xFF47) == 0);
    Require(u_isxdigit_nid_postfix(0x669) == 1 && u_isxdigit_nid_postfix(0x660) == 1);
    Require(u_isxdigit_nid_postfix(0x110000) == 0 && u_isxdigit_nid_postfix(-1) == 0);
    Require(u_digit_nid_postfix('A', 16) == 10);
    Require(u_digit_nid_postfix('z', 36) == 35);
    Require(u_digit_nid_postfix('Z', 36) == 35);
    Require(u_digit_nid_postfix(0xFF41, 16) == 10);
    Require(u_digit_nid_postfix(0xFF5A, 36) == 35);
    Require(u_digit_nid_postfix(0x0669, 10) == 9);
    Require(u_digit_nid_postfix('9', 10) == 9);
    Require(u_digit_nid_postfix('9', 9) == -1);
    Require(u_digit_nid_postfix('A', 10) == -1);
    Require(u_digit_nid_postfix('A', 1) == -1);
    Require(u_digit_nid_postfix('A', 37) == -1);
    Require(u_digit_nid_postfix(-1, 10) == -1);
    return 0;
}
