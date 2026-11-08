#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libcurl/libcurl.hpp"
#include <cstdio>
#include <cstdlib>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "curl string check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

static void CheckEqual() {
    Check(curl_strequal("", "") == 1);
    Check(curl_strequal("Content-Type", "content-TYPE") == 1);
    Check(curl_strequal("azAZ", "AZaz") == 1);
    Check(curl_strequal("a1-_", "A1-_") == 1);
    Check(curl_strequal("abc", "abd") == 0);
    Check(curl_strequal("abc", "ab") == 0);
    Check(curl_strequal("ab", "abc") == 0);
    Check(curl_strequal("", "a") == 0);
    Check(curl_strequal("a", "") == 0);
    Check(curl_strequal("@", "`") == 0);
    Check(curl_strequal("[", "{") == 0);
    Check(curl_strequal("\xE9", "\xC9") == 0);
    Check(curl_strequal("\xE9", "\xE9") == 1);
}

static void CheckBounded() {
    Check(curl_strnequal("abc", "xyz", 0) == 1);
    Check(curl_strnequal("", "a", 0) == 1);
    Check(curl_strnequal("abcX", "ABCy", 3) == 1);
    Check(curl_strnequal("abcX", "ABCy", 4) == 0);
    Check(curl_strnequal("abc", "ab", 2) == 1);
    Check(curl_strnequal("abc", "ab", 3) == 0);
    Check(curl_strnequal("ab", "abc", 3) == 0);
    Check(curl_strnequal("ab", "AB", 2) == 1);
    Check(curl_strnequal("ab", "AB", 100) == 1);
    Check(curl_strnequal("ab\0x", "ab\0y", 4) == 1);
    Check(curl_strnequal("", "", 5) == 1);
    Check(curl_strnequal("\xE9", "\xC9", 1) == 0);
}

int main() {
    CheckEqual();
    CheckBounded();
    return 0;
}
