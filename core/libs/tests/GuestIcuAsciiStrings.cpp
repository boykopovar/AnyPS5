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

}

int main() {
    TestIsASCIILetter();
    TestToUpper();
    TestAsciiToLower();
    return 0;
}
