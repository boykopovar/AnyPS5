#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libcurl/libcurl.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <cstdlib>

static void CheckEqual() {
    const char* same = "MixedCaseHeader";
    Require(curl_strequal(same, same) == 1);
    Require(curl_strequal("", "") == 1);
    Require(curl_strequal("Content-Type", "content-TYPE") == 1);
    Require(curl_strequal("azAZ", "AZaz") == 1);
    Require(curl_strequal("a1-_", "A1-_") == 1);
    Require(curl_strequal("abc", "abd") == 0);
    Require(curl_strequal("abc", "ab") == 0);
    Require(curl_strequal("ab", "abc") == 0);
    Require(curl_strequal("", "a") == 0);
    Require(curl_strequal("a", "") == 0);
    Require(curl_strequal("@", "`") == 0);
    Require(curl_strequal("[", "{") == 0);
    Require(curl_strequal("\xE9", "\xC9") == 0);
    Require(curl_strequal("\xE9", "\xE9") == 1);
}

static void CheckBounded() {
    Require(curl_strnequal("abc", "xyz", 0) == 1);
    Require(curl_strnequal("", "a", 0) == 1);
    Require(curl_strnequal("ab", "ax", 1) == 1);
    Require(curl_strnequal("abcX", "ABCy", 3) == 1);
    Require(curl_strnequal("abcX", "ABCy", 4) == 0);
    Require(curl_strnequal("abc", "ab", 2) == 1);
    Require(curl_strnequal("abc", "ab", 3) == 0);
    Require(curl_strnequal("ab", "abc", 3) == 0);
    Require(curl_strnequal("ab", "AB", 2) == 1);
    Require(curl_strnequal("ab", "AB", 100) == 1);
    Require(curl_strnequal("a\0x", "a\0y", 3) == 1);
    Require(curl_strnequal("ab\0x", "ab\0y", 4) == 1);
    Require(curl_strnequal("", "", 5) == 1);
    Require(curl_strnequal("\xE9", "\xC9", 1) == 0);
}

int main() {
    CheckEqual();
    CheckBounded();
    return 0;
}
