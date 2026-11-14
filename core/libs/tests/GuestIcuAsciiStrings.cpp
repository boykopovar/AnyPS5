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

}

int main() {
    TestIsASCIILetter();
    TestToUpper();
    TestAsciiToLower();
    TestStricmp();
    return 0;
}
