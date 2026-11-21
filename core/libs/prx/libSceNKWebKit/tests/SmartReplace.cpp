#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <exception>
#include <string_view>

namespace {

unsigned checks = 0;
unsigned failures = 0;

void Check(int character, bool previous, bool expected) {
    ++checks;
    const bool actual = _ZN7WebCore29isCharacterSmartReplaceExemptEib(character, previous);
    if (actual != expected) {
        ++failures;
        std::fprintf(stderr, "U+%X previous=%d: expected %d, got %d\n", static_cast<unsigned>(character), previous, expected, actual);
    }
}

}  // namespace

int main() {
    try {
        const std::string_view previousSet = "([\"'#$/-`{";
        const std::string_view nextSet = ")].,;:?'!\"%*-/}";
        for (int c = 0; c < 128; ++c) {
            const bool space = c == 32 || (c >= 9 && c <= 13);
            const bool punctuation = (c >= 33 && c <= 35) || (c >= 37 && c <= 42)
                || (c >= 44 && c <= 47) || (c >= 58 && c <= 59) || (c >= 63 && c <= 64)
                || (c >= 91 && c <= 93) || c == 95 || c == 123 || c == 125;
            Check(c, true, space || previousSet.find(static_cast<char>(c)) != std::string_view::npos);
            Check(c, false, space || punctuation || nextSet.find(static_cast<char>(c)) != std::string_view::npos);
        }
        struct Case { int character; bool previous; bool next; };
        const Case cases[] = {
            {(-0x7fffffff - 1), false, false},
            {-0x1, false, false},
            {0x85, true, true},
            {0xa0, true, true},
            {0x10ff, false, false},
            {0x1100, true, true},
            {0x1101, true, true},
            {0x11ff, true, true},
            {0x1200, true, true},
            {0x1201, false, false},
            {0x1680, true, true},
            {0x2000, true, true},
            {0x2007, true, true},
            {0x200a, true, true},
            {0x200b, false, false},
            {0x2014, false, true},
            {0x2018, false, true},
            {0x2019, false, true},
            {0x2028, true, true},
            {0x2029, true, true},
            {0x202f, true, true},
            {0x203d, false, true},
            {0x205f, true, true},
            {0x2e7f, false, false},
            {0x2e80, true, true},
            {0x2e81, true, true},
            {0x2fdf, true, true},
            {0x2fe0, true, true},
            {0x2fe1, false, false},
            {0x2fef, false, false},
            {0x2ff0, true, true},
            {0x2ff1, true, true},
            {0x31bf, true, true},
            {0x31c0, true, true},
            {0x31c1, false, false},
            {0x31ff, false, false},
            {0x3200, true, true},
            {0x3201, true, true},
            {0xa4cf, true, true},
            {0xa4d0, true, true},
            {0xa4d1, false, false},
            {0xabff, false, false},
            {0xac00, true, true},
            {0xac01, true, true},
            {0xd7ae, true, true},
            {0xd7af, true, true},
            {0xd7b0, false, false},
            {0xd800, false, false},
            {0xdbff, false, false},
            {0xdc00, false, false},
            {0xdfff, false, false},
            {0xf8ff, false, false},
            {0xf900, true, true},
            {0xf901, true, true},
            {0xfa5f, true, true},
            {0xfa60, true, true},
            {0xfa61, false, false},
            {0xfe2f, false, false},
            {0xfe30, true, true},
            {0xfe31, true, true},
            {0xfe4f, true, true},
            {0xfe50, true, true},
            {0xfe51, false, true},
            {0xfeff, false, false},
            {0xff00, true, true},
            {0xff01, true, true},
            {0xffef, true, true},
            {0xfff0, true, true},
            {0xfff1, false, false},
            {0x10100, false, true},
            {0x1f600, false, false},
            {0x1ffff, false, false},
            {0x20000, true, true},
            {0x20001, true, true},
            {0x2a6d6, true, true},
            {0x2a6d7, true, true},
            {0x2a6d8, false, false},
            {0x2f7ff, false, false},
            {0x2f800, true, true},
            {0x2f801, true, true},
            {0x2fa1d, true, true},
            {0x2fa1e, true, true},
            {0x2fa1f, false, false},
            {0x110000, false, false},
            {0x7fffffff, false, false},
        };
        for (const auto& test : cases) {
            Check(test.character, true, test.previous);
            Check(test.character, false, test.next);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "smart replace exception: %s\n", error.what());
        return 1;
    }
    std::printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
