#include "prx/libSceIme/include/ImeClipboard.hpp"

#include <cstdio>

namespace {

bool correct = true;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "IMECLIPBOARD: %s\n", message);
        correct = false;
    }
}

}  // namespace

int main() {
    Check(ImeClipboard::Utf8(u"") == "", "an empty text is not empty in UTF-8");
    Check(ImeClipboard::Utf8(u"Hola") == "Hola", "ASCII text changes");
    Check(ImeClipboard::Utf8(u"Ñ") == "\xC3\x91", "a two-byte character is not encoded");
    Check(ImeClipboard::Utf8(u"€") == "\xE2\x82\xAC", "a three-byte character is not encoded");
    Check(ImeClipboard::Utf8(u"\xD83D\xDE00") == "\xF0\x9F\x98\x80", "a surrogate pair is not one four-byte character");
    Check(ImeClipboard::Utf8(u"\xD83D") == "\xEF\xBF\xBD", "a lone surrogate is not replaced");
    return correct ? 0 : 1;
}
