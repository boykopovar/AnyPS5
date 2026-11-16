#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using Jsc::WtfAbi::StringView;
using Jsc::YarrAbi::OptionalFlags;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Yarr flags check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

StringView View8(const char* text) {
    return {text, static_cast<unsigned>(std::strlen(text)), true};
}

StringView View16(const std::u16string& text) {
    return {text.data(), static_cast<unsigned>(text.size()), false};
}

bool Parsed(StringView view, std::uint16_t expected) {
    const OptionalFlags result = _ZN3JSC4Yarr10parseFlagsEN3WTF10StringViewE(view);
    return result.engaged && result.value == expected;
}

bool Rejected(StringView view) {
    return !_ZN3JSC4Yarr10parseFlagsEN3WTF10StringViewE(view).engaged;
}

void TestLatin1() {
    Require(Parsed(View8(""), 0));
    Require(Parsed(View8("gimsuy"), 0x00BE));
    Require(Parsed(View8("d"), 0x01));
    Require(Parsed(View8("v"), 0x40));
    Require(Parsed(View8("ydgimsv"), 0x00DF));
    Require(Rejected(View8("gg")));
    Require(Rejected(View8("uv")));
    Require(Rejected(View8("vu")));
    Require(Rejected(View8("x")));
    Require(Rejected(View8("G")));
    Require(Rejected(View8("g ")));
    const char withNul[] = {'g', '\0', 'i'};
    Require(Rejected(StringView{withNul, 3, true}));
    Require(Parsed(StringView{"gix", 2, true}, 0x06));
}

void TestUtf16() {
    const std::u16string gi = u"gi";
    Require(Parsed(View16(gi), 0x06));
    const std::u16string wide = u"gĀ";
    Require(Rejected(View16(wide)));
    const std::u16string highByte = u"ŧ";
    Require(Rejected(View16(highByte)));
    const std::u16string all = u"dgimsuy";
    Require(Parsed(View16(all), 0x00BF));
}

void TestNullCharacters() {
    Require(Parsed(StringView{nullptr, 0, true}, 0));
    bool threw = false;
    try {
        _ZN3JSC4Yarr10parseFlagsEN3WTF10StringViewE(StringView{nullptr, 1, true});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    TestLatin1();
    TestUtf16();
    TestNullCharacters();
    return 0;
}
