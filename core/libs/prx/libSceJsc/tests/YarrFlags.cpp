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

std::uint32_t Raw(StringView view) {
    const OptionalFlags result = _ZN3JSC4Yarr10parseFlagsEN3WTF10StringViewE(view);
    std::uint32_t raw = 0;
    static_assert(sizeof(raw) == sizeof(result), "OptionalFlags is returned as 4 bytes");
    std::memcpy(&raw, &result, sizeof(raw));
    return raw;
}

std::uint32_t Result(const char* text) {
    return Raw(View8(text));
}

void TestMeasuredFlags() {
    Require(Result("d") == 0x0101);
    Require(Result("g") == 0x0102);
    Require(Result("i") == 0x0104);
    Require(Result("m") == 0x0108);
    Require(Result("s") == 0x0110);
    Require(Result("u") == 0x0120);
    Require(Result("y") == 0x0140);
    Require(Result("") == 0x0100);
}

void TestMeasuredCombinations() {
    Require(Result("dgimsuy") == 0x017F);
    Require(Result("gi") == 0x0106);
    Require(Result("ig") == 0x0106);
    Require(Result("gimsuy") == 0x017E);
    Require(Result("dgy") == 0x0143);
    Require(Result("yg") == 0x0142);
}

void TestMeasuredRejections() {
    Require(Result("G") == 0);
    Require(Result("dd") == 0);
    Require(Result("yy") == 0);
    Require(Result("dgimsuyy") == 0);
    Require(Result("v") == 0);
    Require(Result("uv") == 0);
    Require(Result("gg") == 0);
    Require(Result("x") == 0);
    Require(Result("dgimsuvy") == 0);
}

void TestSixteenBitFollowsTable() {
    const std::u16string gi = u"gi";
    Require(Raw(View16(gi)) == 0x0106);
    const std::u16string all = u"dgimsuy";
    Require(Raw(View16(all)) == 0x017F);
    const std::u16string wide = u"gĀ";
    Require(Raw(View16(wide)) == 0);
    const std::u16string highByte = u"ŧ";
    Require(Raw(View16(highByte)) == 0);
    const std::u16string unicodeSets = u"v";
    Require(Raw(View16(unicodeSets)) == 0);
}

void TestLengthIsRespected() {
    Require(Result("g ") == 0);
    const char withNul[] = {'g', '\0', 'i'};
    Require(Raw(StringView{withNul, 3, true}) == 0);
    Require(Raw(StringView{"gix", 2, true}) == 0x0106);
}

void TestNullCharacters() {
    Require(Raw(StringView{nullptr, 0, true}) == 0x0100);
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
    TestMeasuredFlags();
    TestMeasuredCombinations();
    TestMeasuredRejections();
    TestSixteenBitFollowsTable();
    TestLengthIsRespected();
    TestNullCharacters();
    return 0;
}
