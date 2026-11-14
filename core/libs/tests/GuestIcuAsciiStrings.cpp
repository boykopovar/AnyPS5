#include "prx/libSceIcu/include/LegacyExports.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU ASCII helpers: %s\n", message);
    std::abort();
}

void TestIsASCIILetter() {
    for (int value = 0; value < 256; ++value) {
        const bool letter = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        Require(uprv_isASCIILetter_nid_postfix(static_cast<char>(value)) == (letter ? 1 : 0), "isASCIILetter matches the ASCII letters");
    }
    for (int value = 0; value < 128; ++value)
        Require((uprv_isASCIILetter_nid_postfix(static_cast<char>(value)) != 0) == (std::isalpha(value) != 0), "isASCIILetter agrees with the C locale");
}

void TestToUpper() {
    for (int value = 0; value < 256; ++value) {
        const bool lower = value >= 'a' && value <= 'z';
        Require(uprv_toupper_nid_postfix(static_cast<char>(value)) == (lower ? static_cast<char>(value - 32) : static_cast<char>(value)), "toupper maps a-z only");
    }
    for (int value = 0; value < 128; ++value)
        Require(uprv_toupper_nid_postfix(static_cast<char>(value)) == static_cast<char>(std::toupper(value)), "toupper agrees with the C locale");
}

void TestAsciiToLower() {
    for (int value = 0; value < 256; ++value) {
        const bool upper = value >= 'A' && value <= 'Z';
        Require(uprv_asciitolower_nid_postfix(static_cast<char>(value)) == (upper ? static_cast<char>(value + 32) : static_cast<char>(value)), "asciitolower maps A-Z only");
    }
    for (int value = 0; value < 128; ++value)
        Require(uprv_asciitolower_nid_postfix(static_cast<char>(value)) == static_cast<char>(std::tolower(value)), "asciitolower agrees with the C locale");
}

void TestStricmp() {
    Require(uprv_stricmp_nid_postfix(nullptr, nullptr) == 0, "stricmp: two null strings are equal");
    Require(uprv_stricmp_nid_postfix(nullptr, "a") == -1, "stricmp: a null first string sorts first");
    Require(uprv_stricmp_nid_postfix("a", nullptr) == 1, "stricmp: a null second string sorts last");
    Require(uprv_stricmp_nid_postfix("abc", "ABC") == 0, "stricmp ignores ASCII case");
    Require(uprv_stricmp_nid_postfix("a", "B") == -1, "stricmp returns the lowercase difference");
    Require(uprv_stricmp_nid_postfix("Z", "[") == 31, "stricmp lowercases before comparing");
    Require(uprv_stricmp_nid_postfix("ab", "abc") == -1, "stricmp: a shorter prefix sorts first");
    Require(uprv_stricmp_nid_postfix("abc", "ab") == 1, "stricmp: a longer string sorts last");
    Require(uprv_stricmp_nid_postfix("\xC3", "\xE3") == -32, "stricmp compares bytes above 127 unsigned without case folding");
}

void TestStrnicmp() {
    Require(uprv_strnicmp_nid_postfix(nullptr, nullptr, 5) == 0, "strnicmp: two null strings are equal");
    Require(uprv_strnicmp_nid_postfix(nullptr, "a", 1) == -1, "strnicmp: a null first string sorts first");
    Require(uprv_strnicmp_nid_postfix("a", nullptr, 1) == 1, "strnicmp: a null second string sorts last");
    Require(uprv_strnicmp_nid_postfix("abcdef", "ABCxyz", 3) == 0, "strnicmp compares only the first n bytes");
    Require(uprv_strnicmp_nid_postfix("abcdef", "ABCxyz", 4) == -20, "strnicmp reports the first differing byte after lowercasing");
    Require(uprv_strnicmp_nid_postfix("abcdef", "xyz", 0) == 0, "strnicmp with n = 0 compares nothing");
    Require(uprv_strnicmp_nid_postfix("ab", "ab", 10) == 0, "strnicmp stops at the common terminator");
    Require(uprv_strnicmp_nid_postfix("ab", "abc", 10) == -1, "strnicmp: a shorter string sorts first");
}

int ReferenceCompare(const char* left, const char* right, std::uint32_t n) {
    for (std::uint32_t index = 0; index < n; ++index) {
        const int a = std::tolower(static_cast<unsigned char>(left[index]));
        const int b = std::tolower(static_cast<unsigned char>(right[index]));
        if (a != b) return a - b;
        if (a == 0) return 0;
    }
    return 0;
}

void TestAgainstReference() {
    const char* samples[] = {"", "a", "A", "abc", "ABC", "Abc", "abd", "ABD", "zz", "ZZ", "[", "\xC3" "x", "x\xE3"};
    for (const char* left : samples) {
        for (const char* right : samples) {
            for (std::uint32_t n = 0; n <= 4; ++n) {
                const int ours = uprv_strnicmp_nid_postfix(left, right, n);
                const int expected = ReferenceCompare(left, right, n);
                Require((ours > 0) == (expected > 0) && (ours < 0) == (expected < 0), "strnicmp agrees in sign with a C-locale reference");
            }
            const int full = uprv_stricmp_nid_postfix(left, right);
            const int expectedFull = ReferenceCompare(left, right, 0xFFFFFFFFu);
            Require((full > 0) == (expectedFull > 0) && (full < 0) == (expectedFull < 0), "stricmp agrees in sign with a C-locale reference");
        }
    }
}

}

int main() {
    TestIsASCIILetter();
    TestToUpper();
    TestAsciiToLower();
    TestStricmp();
    TestStrnicmp();
    TestAgainstReference();
    return 0;
}
