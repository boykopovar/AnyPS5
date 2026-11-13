#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::HEVCAbi::HEVCParameters;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

struct Case {
    std::u16string_view text;
    bool valid = false;
    HEVCParameters::Codec codec = HEVCParameters::Codec::Hvc1;
    unsigned space = 0;
    unsigned profile = 1;
    uint32_t compatibility = 0x80000000;
    unsigned tier = 0;
    unsigned level = 0;
    std::array<unsigned char, 6> flags {};
};

void Require(bool condition, std::u16string_view text) {
    if (condition) return;
    std::string narrow(text.begin(), text.end());
    std::fprintf(stderr, "HEVC mismatch: %s\n", narrow.c_str());
    std::abort();
}

void Check(const Case& test, bool wide) {
    std::string narrow(test.text.begin(), test.text.end());
    StringImpl impl { 1, static_cast<unsigned>(test.text.size()), wide ? static_cast<const void*>(test.text.data()) : narrow.data(), wide ? 0u : 4u };
    String input(&impl);
    auto result = _ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(input);
    Require(result.has_value() == test.valid, test.text);
    if (!result) return;
    Require(reinterpret_cast<const unsigned char*>(&result)[48] == 1, test.text);
    Require(result->codec == test.codec && result->generalProfileSpace == test.space && result->generalProfileIDC == test.profile
        && result->generalProfileCompatibilityFlags == test.compatibility && result->generalTierFlag == test.tier
        && result->generalLevelIDC == test.level, test.text);
    auto& flags = result->generalConstraintIndicatorFlags;
    Require(flags.buffer == flags.inlineStorage && flags.capacity == 6 && flags.size == 6, test.text);
    Require(std::equal(test.flags.begin(), test.flags.end(), flags.buffer), test.text);
    auto copy = result;
    Require(copy->generalConstraintIndicatorFlags.buffer == copy->generalConstraintIndicatorFlags.inlineStorage, test.text);
    Require(copy->generalConstraintIndicatorFlags.buffer != flags.buffer, test.text);
    auto moved = std::move(copy);
    Require(moved->generalConstraintIndicatorFlags.buffer == moved->generalConstraintIndicatorFlags.inlineStorage, test.text);
    Require(std::equal(test.flags.begin(), test.flags.end(), moved->generalConstraintIndicatorFlags.buffer), test.text);
}

}

int main() {
    using Codec = HEVCParameters::Codec;
    const Case cases[] {
        { u"hvc1.1.1.L0", true },
        { u"hev1.A31.12345678.H255", true, Codec::Hev1, 1, 31, 0x1e6a2c48, 1, 255 },
        { u"hvc1.B0.0.L1", true, Codec::Hvc1, 2, 0, 0, 0, 1 },
        { u"hev1.C255.FFFFFFFF.H0", true, Codec::Hev1, 3, 255, 0xffffffff, 1 },
        { u"hvc1.1.1.L0.12", true, Codec::Hvc1, 0, 1, 0x80000000, 0, 0, {0x12} },
        { u"hvc1.1.1.L0.12.34", true, Codec::Hvc1, 0, 1, 0x80000000, 0, 0, {0x12, 0x34} },
        { u"hvc1.1.1.L0.12.34.56", true, Codec::Hvc1, 0, 1, 0x80000000, 0, 0, {0x12, 0x34, 0x56} },
        { u"hvc1.1.1.L0.12.34.56.78", true, Codec::Hvc1, 0, 1, 0x80000000, 0, 0, {0x12, 0x34, 0x56, 0x78} },
        { u"hvc1.1.1.L0.12.34.56.78.9a", true, Codec::Hvc1, 0, 1, 0x80000000, 0, 0, {0x12, 0x34, 0x56, 0x78, 0x9a} },
        { u"hvc1.1.1.L0.12.34.56.78.9a.BC", true, Codec::Hvc1, 0, 1, 0x80000000, 0, 0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc} },
        { u"hvc1.1.1.L0.12.34.56.78.9a.BC.bad.100", true, Codec::Hvc1, 0, 1, 0x80000000, 0, 0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc} },
        { u"..hvc1..1...1..L0...", true },
        { u"hvc1.\t\n\v\f\r +1 \t. +1\n.L +0 \r. +fF \v", true, Codec::Hvc1, 0, 1, 0x80000000, 0, 0, {0xff} },
        { u"hvc1.A +1 .1.L0", true, Codec::Hvc1, 1 },
        { u"hvc1.32.1.L254", true, Codec::Hvc1, 0, 32, 0x80000000, 0, 254 },
        { u"hvc1.0001.0000000001.L000", true },
        { u"" }, { u"..." }, { u"hvc1" }, { u"hvc1.1" }, { u"hvc1.1.1" },
        { u"HVC1.1.1.L0" }, { u"Hev1.1.1.L0" }, { u"avc1.1.1.L0" },
        { u"hvc1.a1.1.L0" }, { u"hvc1.D1.1.L0" }, { u"hvc1.A.1.L0" },
        { u"hvc1.256.1.L0" }, { u"hvc1.-1.1.L0" }, { u"hvc1.1x.1.L0" },
        { u"hvc1.1.100000000.L0" }, { u"hvc1.1.0x1.L0" }, { u"hvc1.1.-1.L0" },
        { u"hvc1.1.G.L0" }, { u"hvc1.1.1.l0" }, { u"hvc1.1.1.h0" },
        { u"hvc1.1.1.L" }, { u"hvc1.1.1.L256" }, { u"hvc1.1.1.L-1" }, { u"hvc1.1.1.L1x" },
        { u"hvc1.1.1.L0.100" }, { u"hvc1.1.1.L0.G" }, { u"hvc1.1.1.L0.ffx" },
        { u"hvc1.1.1.L0. " }, { u"hvc1.1.1.L0.-1" }, { u"hvc1.1.1.L0.+" },
        { u"hvc1.1.1.L0.bad" }, { u"hvc1.1 1.1.L0" }, { u"hvc1.1.1. L0" },
        { u"hvc1.\u00a01.1.L0" }, { u"hvc1.\uff11.1.L0" }, { u"hv\u01631.1.1.L0" },
        { u"hvc1.1.1.L0.1\u0100" },
    };
    for (const auto& test : cases) {
        Check(test, true);
        if (test.text.find_first_of(u"\u00a0\uff11\u0163\u0100") == test.text.npos) Check(test, false);
    }
    String null;
    Require(!_ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(null), u"null String");
    StringImpl missingStorage { 1, 1, nullptr, 4 };
    String malformed(&missingStorage);
    bool threw = false;
    try {
        _ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(malformed);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, u"null character storage");
    std::printf("HEVC codec parameter cases passed\n");
}
