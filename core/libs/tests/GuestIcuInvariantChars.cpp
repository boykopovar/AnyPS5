#include "prx/libSceIcu/include/LegacyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU invariant characters: %s\n", message);
    std::abort();
}

bool ExpectedInvariant(int value) {
    if (value < 0x00 || value > 0x7f) return false;
    if (value <= 0x1f) return value != 0x0a;
    if (value <= 0x3f) return value != 0x21 && value != 0x23 && value != 0x24;
    if (value <= 0x5f) return value != 0x40 && (value < 0x5b || value > 0x5e);
    return value != 0x60 && (value < 0x7b || value > 0x7e);
}

bool IsInvariantByte(char c) {
    const char text[2] = {c, 0};
    return uprv_isInvariantString_nid_postfix(text, 1) != 0;
}

void TestSingleCharacterSetMatchesTheDocumentedRanges() {
    for (int value = 0; value < 256; ++value) {
        const char c = static_cast<char>(value);
        const bool expected = ExpectedInvariant(value);
        Require(IsInvariantByte(c) == expected, "a single byte is invariant exactly when the documented ranges say so");
    }
}

void TestByteStringSemantics() {
    Require(uprv_isInvariantString_nid_postfix("", -1) != 0, "NUL-terminated empty string is invariant");
    Require(uprv_isInvariantString_nid_postfix("Hello 42", -1) != 0, "plain ASCII text is invariant");
    Require(uprv_isInvariantString_nid_postfix("Hi!", -1) == 0, "a NUL-terminated string with '!' is variant");
    Require(uprv_isInvariantString_nid_postfix("ab\xE9", -1) == 0, "a byte above 127 makes the string variant");
    Require(uprv_isInvariantString_nid_postfix("ab\0cd!", -1) != 0, "a NUL-terminated string stops at the first NUL");
    Require(uprv_isInvariantString_nid_postfix("ab\0cd!", 6) == 0, "a length-limited string checks past a NUL and finds '!'");
    Require(uprv_isInvariantString_nid_postfix("abc", 0) != 0, "a zero length checks nothing");
}

}

int main() {
    TestSingleCharacterSetMatchesTheDocumentedRanges();
    TestByteStringSemantics();
    return 0;
}
