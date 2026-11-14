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

}

int main() {
    TestIsASCIILetter();
    return 0;
}
