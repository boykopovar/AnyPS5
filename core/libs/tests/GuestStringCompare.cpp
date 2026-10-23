#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>

extern "C" {
int APS5_VABI memcmp_nid_postfix(const void*, const void*, std::size_t);
int APS5_VABI strcmp_nid_postfix(const char*, const char*);
int APS5_VABI strncmp_nid_postfix(const char*, const char*, std::size_t);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(memcmp_nid_postfix("\x80", "\x01", 1) == 127);
    Require(memcmp_nid_postfix("\x01", "\x80", 1) == -127);
    Require(memcmp_nid_postfix("abc", "abz", 3) == -23);
    Require(memcmp_nid_postfix("abc", "abc", 3) == 0);
    Require(memcmp_nid_postfix("ab\0x", "ab\0y", 4) == -1);
    Require(strcmp_nid_postfix("\x80", "\x01") == 127);
    Require(strcmp_nid_postfix("ab", "abc") == -99);
    Require(strcmp_nid_postfix("abz", "abc") == 23);
    Require(strcmp_nid_postfix("abc", "abc") == 0);
    Require(strncmp_nid_postfix("abX", "abY", 2) == 0);
    Require(strncmp_nid_postfix("abX", "abY", 3) == -1);
    Require(strncmp_nid_postfix("\x80", "\x01", 1) == 127);
    Require(strncmp_nid_postfix("ab", "abc", 5) == -99);
    Require(strncmp_nid_postfix("ab\0x", "ab\0y", 4) == 0);
    return 0;
}
