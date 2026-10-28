#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU character check failed at line %u\n", location.line());
    std::abort();
}

void IsoControl() {
    for (std::int32_t value = -0x1000; value <= 0x110010; ++value) {
        const bool expected = (value >= 0 && value <= 0x1f) || (value >= 0x7f && value <= 0x9f);
        Require((u_isISOControl_nid_postfix(value) != 0) == expected);
    }
    Require(u_isISOControl_nid_postfix(0x20) == 0 && u_isISOControl_nid_postfix(0x7e) == 0 && u_isISOControl_nid_postfix(0xa0) == 0);
    Require(u_isISOControl_nid_postfix(0x85) != 0 && u_isISOControl_nid_postfix(0x7f) != 0 && u_isISOControl_nid_postfix(0) != 0);
    Require(u_isISOControl_nid_postfix(std::numeric_limits<std::int32_t>::min()) == 0);
    Require(u_isISOControl_nid_postfix(std::numeric_limits<std::int32_t>::max()) == 0);
    Require(u_isISOControl_nid_postfix(-1) == 0);
    Require(u_isISOControl_nid_postfix(0x2028) == 0);
}

void ForDigit() {
    for (int radix = -128; radix <= 127; ++radix) {
        for (std::int32_t digit = -3; digit <= 40; ++digit) {
            std::int32_t expected = 0;
            if (radix >= 2 && radix <= 36 && digit >= 0 && digit < radix) expected = digit < 10 ? '0' + digit : 'a' + (digit - 10);
            Require(u_forDigit_nid_postfix(digit, static_cast<std::int8_t>(radix)) == expected);
        }
    }
    Require(u_forDigit_nid_postfix(0, 10) == '0' && u_forDigit_nid_postfix(9, 10) == '9');
    Require(u_forDigit_nid_postfix(10, 10) == 0);
    Require(u_forDigit_nid_postfix(10, 16) == 'a' && u_forDigit_nid_postfix(15, 16) == 'f');
    Require(u_forDigit_nid_postfix(35, 36) == 'z' && u_forDigit_nid_postfix(36, 36) == 0);
    Require(u_forDigit_nid_postfix(1, 2) == '1' && u_forDigit_nid_postfix(2, 2) == 0);
    Require(u_forDigit_nid_postfix(0, 1) == 0 && u_forDigit_nid_postfix(0, 37) == 0 && u_forDigit_nid_postfix(0, 0) == 0);
    Require(u_forDigit_nid_postfix(-1, 10) == 0);
    Require(u_forDigit_nid_postfix(std::numeric_limits<std::int32_t>::min(), 36) == 0);
    Require(u_forDigit_nid_postfix(std::numeric_limits<std::int32_t>::max(), 36) == 0);
}

}

int main() {
    IsoControl();
    ForDigit();
    std::puts("ICU character tests passed");
}
