#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using WebCore::HEVCOptionalParameterSet;

using RawParse = unsigned char* (APS5_VABI *)(unsigned char* result, const String&);
const auto rawParse = std::bit_cast<RawParse>(&_ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE);

struct Case {
    std::u16string_view text;
    bool valid = false;
    std::u16string_view codec = u"hvc1";
    unsigned space = 0;
    unsigned profile = 1;
    uint32_t compatibility = 1;
    bool tier = false;
    unsigned level = 0;
    std::array<unsigned short, 6> flags {};
};

void Require(bool condition, std::u16string_view text, const char* what) {
    if (condition) return;
    std::string narrow;
    for (const char16_t unit : text) narrow.push_back(unit < 0x80 ? static_cast<char>(unit) : '?');
    std::fprintf(stderr, "HEVC mismatch (%s): %s\n", what, narrow.c_str());
    std::abort();
}

std::u16string Name(const String& string) {
    std::u16string name;
    if (!string.impl) return name;
    for (unsigned index = 0; index < string.impl->length; ++index)
        name.push_back(string.impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(string.impl->data)[index] : static_cast<const char16_t*>(string.impl->data)[index]);
    return name;
}

void Check(const Case& test, bool wide) {
    std::string narrow;
    for (const char16_t unit : test.text) narrow.push_back(static_cast<char>(unit));
    StringImpl impl { 2, static_cast<unsigned>(test.text.size()), wide ? static_cast<const void*>(test.text.data()) : narrow.data(), wide ? 0u : 4u };
    String input(&impl);
    impl.refCount += 2;
    {
        const HEVCOptionalParameterSet result = _ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(input);
        Require(result.engaged == test.valid, test.text, "engaged");
        if (test.valid) {
            const auto& value = result.value;
            Require(Name(value.codecName) == test.codec, test.text, "codec name");
            Require(value.codecName.impl->data != impl.data && !!(value.codecName.impl->hashAndFlags & 4) == !wide, test.text, "codec name storage");
            Require(value.generalProfileSpace == test.space && value.generalProfileIDC == test.profile, test.text, "profile");
            Require(value.generalProfileCompatibilityFlags == test.compatibility, test.text, "compatibility flags");
            Require(value.generalTierFlag == test.tier && value.generalLevelIDC == test.level, test.text, "tier and level");
            Require(value.constraintFlags.buffer && value.constraintFlags.capacity == 6 && value.constraintFlags.size == 6, test.text, "constraint vector");
            Require(std::equal(test.flags.begin(), test.flags.end(), value.constraintFlags.buffer), test.text, "constraint flags");
        }
    }
    alignas(8) std::array<unsigned char, 48> bytes;
    bytes.fill(0xcd);
    Require(rawParse(bytes.data(), input) == bytes.data(), test.text, "hidden result pointer");
    Require(bytes[0] == (test.valid ? 1 : 0), test.text, "flag byte");
    if (!test.valid) {
        Require(bytes[8] == 0, test.text, "empty storage byte");
        for (std::size_t index = 1; index < bytes.size(); ++index)
            Require(index == 8 || bytes[index] == 0xcd, test.text, "bytes written by an empty result");
        return;
    }
    auto* stored = reinterpret_cast<HEVCOptionalParameterSet*>(bytes.data());
    StringImpl* name = nullptr;
    std::uint16_t space = 0;
    std::uint16_t profile = 0;
    std::uint32_t compatibility = 0;
    std::uint16_t level = 0;
    unsigned short* flags = nullptr;
    std::uint32_t capacity = 0;
    std::uint32_t size = 0;
    std::memcpy(&name, bytes.data() + 8, sizeof(name));
    std::memcpy(&space, bytes.data() + 16, sizeof(space));
    std::memcpy(&profile, bytes.data() + 18, sizeof(profile));
    std::memcpy(&compatibility, bytes.data() + 20, sizeof(compatibility));
    std::memcpy(&level, bytes.data() + 26, sizeof(level));
    std::memcpy(&flags, bytes.data() + 32, sizeof(flags));
    std::memcpy(&capacity, bytes.data() + 40, sizeof(capacity));
    std::memcpy(&size, bytes.data() + 44, sizeof(size));
    Require(name && name == stored->value.codecName.impl && name->refCount == 2, test.text, "raw codec name");
    Require(space == test.space && profile == test.profile && compatibility == test.compatibility, test.text, "raw profile");
    Require(bytes[24] == (test.tier ? 1 : 0) && level == test.level, test.text, "raw tier and level");
    Require(flags && capacity == 6 && size == 6 && std::equal(test.flags.begin(), test.flags.end(), flags), test.text, "raw constraint flags");
    Require(_ZN3WTF14fastMallocSizeEPKv(flags) == 12, test.text, "constraint buffer size");
    stored->~HEVCOptionalParameterSet();
}

}

int main() {
    const Case cases[] {
        { u"hvc1.1.1.L0", true },
        { u"hev1.A31.12345678.H255", true, u"hev1", 1, 31, 0x12345678, true, 255 },
        { u"hvc1.B0.0.L1", true, u"hvc1", 2, 0, 0, false, 1 },
        { u"hev1.C255.FFFFFFFF.H0", true, u"hev1", 3, 255, 0xffffffff, true },
        { u"hvc1.2.4.L120.B0", true, u"hvc1", 0, 2, 4, false, 120, {0xb0} },
        { u"hvc1.1.6.L93.12.34", true, u"hvc1", 0, 1, 6, false, 93, {0x12, 0x34} },
        { u"hvc1.1.1.L0.12.34.56", true, u"hvc1", 0, 1, 1, false, 0, {0x12, 0x34, 0x56} },
        { u"hvc1.1.1.L0.12.34.56.78", true, u"hvc1", 0, 1, 1, false, 0, {0x12, 0x34, 0x56, 0x78} },
        { u"hvc1.1.1.L0.12.34.56.78.9a", true, u"hvc1", 0, 1, 1, false, 0, {0x12, 0x34, 0x56, 0x78, 0x9a} },
        { u"hvc1.1.1.L0.12.34.56.78.9a.BC", true, u"hvc1", 0, 1, 1, false, 0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc} },
        { u"hvc1.1.1.L0.12.34.56.78.9a.BC.bad.100", true, u"hvc1", 0, 1, 1, false, 0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc} },
        { u"..hvc1..1...1..L0...", true },
        { u"hvc1.\t\n\v\f\r +1 \t. +1\n.L +0 \r. +fF \v", true, u"hvc1", 0, 1, 1, false, 0, {0xff} },
        { u"hvc1.A +1 .1.L0", true, u"hvc1", 1 },
        { u"hvc1.32.1.L254", true, u"hvc1", 0, 32, 1, false, 254 },
        { u"hvc1.0001.0000000001.L000", true },
        { u"hvc1.1.80000000.H0", true, u"hvc1", 0, 1, 0x80000000, true },
        { u"" }, { u"..." }, { u"hvc1" }, { u"hvc1.1" }, { u"hvc1.1.1" },
        { u"HVC1.1.1.L0" }, { u"Hev1.1.1.L0" }, { u"avc1.1.1.L0" }, { u"hvc.1.1.L0" }, { u"hvc11.1.1.L0" },
        { u"hvc1.a1.1.L0" }, { u"hvc1.D1.1.L0" }, { u"hvc1.A.1.L0" },
        { u"hvc1.256.1.L0" }, { u"hvc1.-1.1.L0" }, { u"hvc1.1x.1.L0" },
        { u"hvc1.1.100000000.L0" }, { u"hvc1.1.0x1.L0" }, { u"hvc1.1.-1.L0" },
        { u"hvc1.1.G.L0" }, { u"hvc1.1.1.l0" }, { u"hvc1.1.1.h0" },
        { u"hvc1.1.1.L" }, { u"hvc1.1.1.L256" }, { u"hvc1.1.1.L-1" }, { u"hvc1.1.1.L1x" },
        { u"hvc1.1.1.L0.100" }, { u"hvc1.1.1.L0.G" }, { u"hvc1.1.1.L0.ffx" },
        { u"hvc1.1.1.L0. " }, { u"hvc1.1.1.L0.-1" }, { u"hvc1.1.1.L0.+" },
        { u"hvc1.1.1.L0.bad" }, { u"hvc1.1 1.1.L0" }, { u"hvc1.1.1. L0" },
    };
    for (const auto& test : cases) {
        Check(test, true);
        Check(test, false);
    }
    const Case wideCases[] {
        { u"hvc1.\u20031.1.L0", true },
        { u"hvc1.1\u1680.\u20281\u205f.L\u30000\u200a.\u2000fF\u2009", true, u"hvc1", 0, 1, 1, false, 0, {0xff} },
        { u"hvc1.\u00a01.1.L0" }, { u"hvc1.\u00851.1.L0" }, { u"hvc1.\u20291.1.L0" }, { u"hvc1.\u200b1.1.L0" },
        { u"hvc1.\uff11.1.L0" }, { u"hv\u01631.1.1.L0" }, { u"hvc1.1.1.L0.1\u0100" }, { u"hvc1.1.1.\u013c0" },
    };
    for (const auto& test : wideCases) Check(test, true);
    String null;
    {
        const HEVCOptionalParameterSet result = _ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(null);
        Require(!result.engaged, u"null String", "engaged");
    }
    StringImpl missingStorage { 1, 1, nullptr, 4 };
    String malformed(&missingStorage);
    bool threw = false;
    try {
        _ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(malformed);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, u"null character storage", "throws");
    malformed.impl = nullptr;
    std::printf("HEVC codec parameter cases passed, each through the returned object and through the hidden result pointer\n");
}
