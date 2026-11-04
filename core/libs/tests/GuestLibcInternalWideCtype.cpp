#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr short Hex = 0x01;
constexpr short Upper = 0x02;
constexpr short Blank = 0x04;
constexpr short Punct = 0x08;
constexpr short Lower = 0x10;
constexpr short Digit = 0x20;
constexpr short Space = 0x40;
constexpr short Control = 0x80;
constexpr short Tab = 0x400;
constexpr short Alpha = Upper | Lower;
constexpr short AllMasks = Hex | Upper | Blank | Punct | Lower | Digit | Space | Control | Tab;

const GuestLocale::Facet facet{};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal ctype<wchar_t>: %s\n", message);
    std::abort();
}

void CheckSingleClassification() {
    const short* table = _Getpctype_nid_postfix();
    for (std::uint32_t value = 0; value <= 0xffff; ++value) {
        const auto character = static_cast<char16_t>(value);
        const short expected = value < 256 ? table[value] : 0;
        for (short bit = 1; bit <= Tab; bit = static_cast<short>(bit << 1)) {
            Require(sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, bit, character) == ((expected & bit) != 0), "do_is matches the C locale table bit by bit");
        }
        Require(!sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, 0, character), "an empty mask matches nothing");
        if (value < 128) {
            Require(sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, Alpha, character) == (isalpha_nid_postfix(static_cast<int>(value)) != 0), "do_is agrees with isalpha");
            Require(sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, Blank | Space, character) == (isspace_nid_postfix(static_cast<int>(value)) != 0), "do_is agrees with isspace");
        }
    }
    Require(sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, Alpha, u'A'), "A is alphabetic");
    Require(sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, Hex | Digit, u'7'), "7 is a digit");
    Require(!sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, Digit, u'A'), "A is no digit");
    Require(sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, Tab, u'\t'), "tab carries the blank bit");
    Require(!sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, AllMasks, u'é'), "Latin-1 letters have no class in the C locale");
    Require(!sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, AllMasks, u'Α'), "characters past 255 have no class");
    Require(!sceLibcInternalWideCtypeDoIs_nid_no_patch(&facet, AllMasks, u'￿'), "U+FFFF has no class");
}

void CheckRangeClassification() {
    const std::array<char16_t, 8> source{u'A', u'1', u' ', u'z', u'\t', u'!', u'é', u'中'};
    std::array<short, source.size() + 1> masks{};
    masks.fill(-1);
    const char16_t* end = sceLibcInternalWideCtypeDoIsRange_nid_no_patch(&facet, source.data(), source.data() + source.size(), masks.data());
    Require(end == source.data() + source.size(), "do_is returns the end of the range");
    Require(masks[0] == (Upper | Hex) && masks[1] == (Digit | Hex) && masks[2] == Blank, "letters, digits and space get their masks");
    Require(masks[3] == Lower && masks[4] == (Control | Space | Tab) && masks[5] == Punct, "lower case, tab and punctuation get their masks");
    Require(masks[6] == 0 && masks[7] == 0, "characters outside the C table get no mask");
    Require(masks[8] == -1, "do_is writes one mask per character");
    Require(sceLibcInternalWideCtypeDoIsRange_nid_no_patch(&facet, source.data() + 3, source.data() + 3, nullptr) == source.data() + 3, "an empty range writes nothing");
}

void CheckScanNot() {
    const std::array<char16_t, 7> text{u' ', u'\t', u'\n', u'a', u'b', u'1', u' '};
    const char16_t* begin = text.data();
    const char16_t* end = text.data() + text.size();
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(&facet, Blank | Space, begin, end) == begin + 3, "do_scan_not stops at the first non-space");
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(&facet, Alpha, begin + 3, end) == begin + 5, "do_scan_not stops at the first non-letter");
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(&facet, AllMasks, begin, end) == end, "a range that always matches returns its end");
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(&facet, 0, begin, end) == begin, "an empty mask stops at once");
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(&facet, Space, end, end) == end, "an empty range returns its end");
    const std::array<char16_t, 2> wide{u'Α', u'a'};
    Require(sceLibcInternalWideCtypeDoScanNot_nid_no_patch(&facet, AllMasks, wide.data(), wide.data() + wide.size()) == wide.data(), "characters past 255 match no mask");
}

void CheckToupper() {
    for (std::uint32_t value = 0; value <= 0xffff; ++value) {
        const auto character = static_cast<char16_t>(value);
        const auto expected = value >= 'a' && value <= 'z' ? static_cast<char16_t>(value - 32) : character;
        Require(sceLibcInternalWideCtypeDoToupper_nid_no_patch(&facet, character) == expected, "do_toupper changes only ASCII lower case letters");
    }
    std::array<char16_t, 7> text{u'a', u'Z', u'q', u'é', u'α', u'5', u'#'};
    char16_t* begin = text.data();
    Require(sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(&facet, begin, begin + 6) == begin + 6, "do_toupper returns the end of the range");
    Require(text[0] == u'A' && text[1] == u'Z' && text[2] == u'Q', "ASCII letters convert in place");
    Require(text[3] == u'é' && text[4] == u'α' && text[5] == u'5', "other characters are unchanged");
    Require(text[6] == u'#', "do_toupper stops at the end of the range");
    Require(sceLibcInternalWideCtypeDoToupperRange_nid_no_patch(&facet, begin + 2, begin + 2) == begin + 2, "an empty range converts nothing");
}

}

int main() {
    CheckSingleClassification();
    CheckRangeClassification();
    CheckScanNot();
    CheckToupper();
    return 0;
}
