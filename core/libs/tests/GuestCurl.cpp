#include "prx/libcurl/libcurl.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "GuestCurl: %s\n", message);
        std::abort();
    }
}

void TestStrequal() {
    Require(curl_strequal("hello", "hello") == 1, "exact match failed");
    Require(curl_strequal("hello", "HELLO") == 1, "case difference failed");
    Require(curl_strequal("Hello, World!", "hELLO, wORLD!") == 1, "mixed case difference failed");
    Require(curl_strequal("abc", "abd") == 0, "mismatch failed");
    Require(curl_strequal("abc", "abcd") == 0, "different lengths failed");
    Require(curl_strequal("abcd", "abc") == 0, "different lengths reversed failed");
    Require(curl_strequal("", "") == 1, "empty strings failed");
    Require(curl_strequal("a", "") == 0, "empty vs non-empty failed");
    Require(curl_strequal("", "a") == 0, "non-empty vs empty failed");
    Require(curl_strequal(nullptr, nullptr) == 1, "both null failed");
    Require(curl_strequal("hello", nullptr) == 0, "second null failed");
    Require(curl_strequal(nullptr, "hello") == 0, "first null failed");
}

void TestStrnequal() {
    Require(curl_strnequal("hello", "HELLO", 5) == 1, "exact length case insensitive failed");
    Require(curl_strnequal("hello", "HELLO", 3) == 1, "prefix length case insensitive failed");
    Require(curl_strnequal("hello", "HELxx", 3) == 1, "matching prefix failed");
    Require(curl_strnequal("hello", "HELxx", 4) == 0, "differing past prefix failed");
    Require(curl_strnequal("abc", "xyz", 0) == 1, "zero length match failed");
    Require(curl_strnequal("", "", 0) == 1, "empty zero length failed");
    Require(curl_strnequal("", "", 5) == 1, "empty non-zero length failed");
    Require(curl_strnequal(nullptr, nullptr, 0) == 1, "both null zero length failed");
    Require(curl_strnequal(nullptr, nullptr, 5) == 1, "both null non-zero length failed");
    Require(curl_strnequal("hello", nullptr, 0) == 1, "second null zero length failed");
    Require(curl_strnequal("hello", nullptr, 1) == 0, "second null non-zero length failed");
    Require(curl_strnequal(nullptr, "hello", 0) == 1, "first null zero length failed");
    Require(curl_strnequal(nullptr, "hello", 1) == 0, "first null non-zero length failed");
}

}

int main() {
    TestStrequal();
    TestStrnequal();
    return 0;
}
