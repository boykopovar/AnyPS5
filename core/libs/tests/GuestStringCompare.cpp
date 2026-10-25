#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    Require(strcmp_nid_postfix("abc", "abc") == 0);
    Require(strcmp_nid_postfix("abc", "abd") == -1);
    Require(strcmp_nid_postfix("abd", "abc") == 1);
    Require(strcmp_nid_postfix("a", "z") == -25);
    Require(strcmp_nid_postfix("abc", "abcd") == -100);
    Require(strcmp_nid_postfix("abcd", "abc") == 100);
    Require(strcmp_nid_postfix("\x80", "a") == 31);
    Require(strcmp_nid_postfix("", "") == 0);

    Require(strncmp_nid_postfix("abcdef", "abcxyz", 3) == 0);
    Require(strncmp_nid_postfix("abcdef", "abcxyz", 4) == -20);
    Require(strncmp_nid_postfix("abc", "xyz", 0) == 0);
    Require(strncmp_nid_postfix("abc\0x", "abc\0y", 6) == 0);
    Require(strncmp_nid_postfix("ab", "abc", 8) == -99);

    const unsigned char base[] = {0x01, 0x02, 0x03};
    const unsigned char higher[] = {0x01, 0x02, 0x04};
    const unsigned char top[] = {0x01, 0x02, 0xff};
    const unsigned char low[] = {0x01, 0x02, 0x01};
    Require(memcmp_nid_postfix(base, base, 3) == 0);
    Require(memcmp_nid_postfix(base, higher, 3) == -1);
    Require(memcmp_nid_postfix(top, low, 3) == 254);
    Require(memcmp_nid_postfix(top, low, 2) == 0);
    const unsigned char sign[] = {0x80};
    const unsigned char one[] = {0x01};
    Require(memcmp_nid_postfix(sign, one, 1) == 127);
    const unsigned char withNul[] = {0x00, 0x05};
    const unsigned char withNulLow[] = {0x00, 0x02};
    Require(memcmp_nid_postfix(withNul, withNulLow, 2) == 3);
}
