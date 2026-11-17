#include "prx/libSceIme/include/ImeHostText.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

using namespace ImeHostText;

std::u16string clip(const std::string& utf8, bool multiline = false) {
    return ClipboardUnits(utf8.data(), utf8.size(), multiline);
}

std::u16string preedit(const std::string& utf8) {
    return PreeditUnits(utf8.data(), utf8.size());
}

void decodes_valid_utf8() {
    const std::string text = "a\xc3\xa9\xe3\x81\x82\xf0\x9f\x98\x80";
    const auto points = DecodeUtf8Lenient(text.data(), text.size(), false);
    Require(points == std::u32string(U"a\u00E9\u3042\U0001F600"), "valid UTF-8 decodes to its code points");
    Require(clip(text) == u"a\u00E9\u3042\U0001F600", "valid UTF-8 becomes UTF-16 with a surrogate pair");
    Require(clip("\xf0\x9f\x98\x80")[0] == 0xd83d && clip("\xf0\x9f\x98\x80")[1] == 0xde00, "the pair is high then low");
}

void replaces_invalid_utf8() {
    Require(clip("a\xffz") == u"a\uFFFDz", "a byte that never starts a sequence is replaced");
    Require(clip("\xc0\x80") == u"\uFFFD\uFFFD", "an overlong NUL is replaced and not decoded");
    Require(clip("\xe0\x80\xaf") == u"\uFFFD", "an overlong three-byte slash is replaced");
    Require(clip("\xf0\x80\x80\x80") == u"\uFFFD", "an overlong four-byte NUL is replaced");
    Require(clip("\xed\xa0\x80") == u"\uFFFD", "an encoded surrogate is replaced");
    Require(clip("\xf4\x90\x80\x80") == u"\uFFFD", "a code point past U+10FFFF is replaced");
    Require(clip("\xe3\x81" "A") == u"\uFFFDA", "a sequence cut by an ASCII byte is replaced once");
    Require(clip("a\xe3\x81") == u"a\uFFFD", "an incomplete tail is replaced in a clipboard");
    Require(preedit("a\xe3\x81") == u"a", "an incomplete tail is dropped in a preedit cut at 32 bytes");
    Require(preedit("\xe3\x81\x82\xe3\x81") == u"\u3042", "only the cut sequence of a preedit is dropped");
    Require(preedit("\xe3\x81" "A") == u"\uFFFDA", "a sequence broken inside a preedit is still replaced");
}

void strips_control_characters() {
    Require(clip(std::string("a\0b", 3)) == u"ab", "NUL is stripped");
    Require(clip("a\tb\x01" "c\x7f" "d") == u"abcd", "tab and other C0 controls and DEL are stripped");
    Require(clip("a\xc2\x80" "b\xc2\x9f" "c") == u"abc", "C1 controls are stripped");
    Require(clip("a\x1b[31mb") == u"a[31mb", "an escape byte is stripped and the rest stays text");
    Require(preedit("a\x01" "b") == u"ab", "a preedit loses control characters too");
    Require(clip("\xc2\xa0z") == u"\u00A0z", "a no-break space is not a control character");
}

void handles_line_breaks() {
    Require(clip("a\nb") == u"ab", "a single-line field drops LF");
    Require(clip("a\r\nb") == u"ab", "a single-line field drops CRLF as one break");
    Require(clip("a\rb") == u"ab", "a single-line field drops CR");
    Require(clip("a\xe2\x80\xa8" "b\xe2\x80\xa9" "c") == u"abc", "a single-line field drops U+2028 and U+2029");
    Require(clip("a\nb", true) == u"a\nb", "a multi-line field keeps LF");
    Require(clip("a\r\nb", true) == u"a\nb", "CRLF is one LF");
    Require(clip("a\rb", true) == u"a\nb", "CR is one LF");
    Require(clip("a\n\nb", true) == u"a\n\nb", "two LFs stay two");
    Require(clip("a\r\n\r\nb", true) == u"a\n\nb", "two CRLFs are two LFs");
    Require(clip("a\xe2\x80\xa8" "b", true) == u"a\nb", "U+2028 is a line break in a multi-line field");
    Require(clip("a\xc2\x85" "b", true) == u"a\nb", "NEL is a line break in a multi-line field");
}

void bounds_huge_clipboards() {
    Require(BoundedUtf8Size("abc", 3) == 3, "a small clipboard is not cut");
    std::string huge(MaxClipboardBytes * 4, 'x');
    Require(BoundedUtf8Size(huge.data(), huge.size()) == MaxClipboardBytes, "an ASCII clipboard is cut at the bound");
    Require(clip(huge).size() == MaxClipboardBytes, "the converted text is bounded");
    std::string wide;
    while (wide.size() < MaxClipboardBytes + 16) wide += "\xe3\x81\x82";
    const std::size_t cut = BoundedUtf8Size(wide.data(), wide.size());
    Require(cut <= MaxClipboardBytes && cut % 3 == 0, "the cut falls between code points");
    Require(clip(wide).find(u'\uFFFD') == std::u16string::npos, "a bounded clipboard has no replacement character");
    std::string pairs;
    while (pairs.size() < MaxClipboardBytes + 16) pairs += "\xf0\x9f\x98\x80";
    const std::u16string converted = clip(pairs);
    Require(converted.size() % 2 == 0 && converted.size() <= MaxClipboardBytes, "the cut never leaves half a pair");
    Require(BoundedUtf8Size(pairs.data(), pairs.size()) % 4 == 0, "a four-byte cut stays on a boundary");
}

}

int main() {
    try {
        decodes_valid_utf8();
        replaces_invalid_utf8();
        strips_control_characters();
        handles_line_breaks();
        bounds_huge_clipboards();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ime host text: %s\n", error.what());
        return 1;
    }
    return 0;
}
