#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>

namespace {
using namespace NKWebKit::WtfAbi;
using WebCore::DoViOptionalParameterSet;

using RawParse = unsigned char* (APS5_VABI *)(unsigned char* result, const String&);
const auto rawParse = std::bit_cast<RawParse>(&_ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE);
unsigned checks = 0;

[[noreturn]] void Fail(const char* what, const std::string& text, bool wide) {
    std::fprintf(stderr, "DoVi failed (%s, %s): %s\n", what, wide ? "16" : "8", text.c_str());
    std::abort();
}

std::string Name(const String& string) {
    if (!string.impl || !(string.impl->hashAndFlags & 4)) return "<not 8-bit>";
    return std::string(static_cast<const char*>(string.impl->data), string.impl->length);
}

void CheckInput(const String& input, const std::string& label, bool wide, int profile, int level, const char* codecName) {
    const bool valid = profile >= 0;
    {
        const DoViOptionalParameterSet result = _ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(input);
        if (result.engaged != valid) Fail("engaged", label, wide);
        if (valid && (Name(result.value.codecName) != codecName || result.value.bitstreamProfileID != profile || result.value.bitstreamLevelID != level))
            Fail("value", label, wide);
    }
    alignas(8) std::array<unsigned char, 24> bytes;
    bytes.fill(0xcd);
    if (rawParse(bytes.data(), input) != bytes.data()) Fail("hidden result pointer", label, wide);
    if (bytes[0] != (valid ? 1 : 0)) Fail("flag byte", label, wide);
    if (!valid) {
        if (bytes[8] != 0) Fail("empty storage byte", label, wide);
        for (std::size_t index = 1; index < bytes.size(); ++index) {
            if (index != 8 && bytes[index] != 0xcd) Fail("bytes written by an empty result", label, wide);
        }
    } else {
        auto* stored = reinterpret_cast<DoViOptionalParameterSet*>(bytes.data());
        StringImpl* impl = nullptr;
        std::uint16_t storedProfile = 0;
        std::uint16_t storedLevel = 0;
        std::memcpy(&impl, bytes.data() + 8, sizeof(impl));
        std::memcpy(&storedProfile, bytes.data() + 16, sizeof(storedProfile));
        std::memcpy(&storedLevel, bytes.data() + 18, sizeof(storedLevel));
        if (impl != stored->value.codecName.impl || !impl || impl->refCount != 2) Fail("codec name storage", label, wide);
        if (Name(stored->value.codecName) != codecName || storedProfile != profile || storedLevel != level) Fail("raw value", label, wide);
        stored->~DoViOptionalParameterSet();
    }
    ++checks;
}

void Check(const std::string& text, bool wide, int profile = -1, int level = 0, const char* codecName = "") {
    std::u16string text16;
    for (const unsigned char unit : text) text16.push_back(unit);
    StringImpl impl {2, static_cast<unsigned>(text.size()), wide ? static_cast<const void*>(text16.data()) : text.data(), wide ? 0u : 4u};
    String input(&impl);
    impl.refCount += 2;
    CheckInput(input, text, wide, profile, level, codecName);
}

void CheckWide(const std::u16string& text, const char* label, int profile = -1, int level = 0, const char* codecName = "") {
    StringImpl impl {2, static_cast<unsigned>(text.size()), text.data(), 0};
    String input(&impl);
    impl.refCount += 2;
    CheckInput(input, label, true, profile, level, codecName);
}

}  // namespace

int main() {
    {
        String null;
        const DoViOptionalParameterSet result = _ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(null);
        if (result.engaged) std::abort();
        CheckInput(null, "<null>", false, -1, 0, "");
    }
    struct Name { const char* name; const char* codec; bool avc; };
    const Name names[] {{"dva1", "avc1", true}, {"dvav", "avc3", true}, {"dvhe", "hev1", false}, {"dvh1", "hvc1", false}};
    struct Profile { int id; int maximum; };
    const Profile profiles[] {{4, 9}, {5, 13}, {7, 9}, {8, 13}, {9, 5}};
    for (bool wide : {false, true}) {
        for (auto name : names) {
            for (int profile = 0; profile <= 10; ++profile) {
                int maximum = -1;
                for (auto p : profiles) if (p.id == profile) maximum = p.maximum;
                const bool codecValid = profile == 9 ? name.avc : !name.avc;
                for (int level = 0; level <= 14; ++level) {
                    auto text = std::string(name.name) + ".0" + std::to_string(profile) + "." + std::to_string(level);
                    Check(text, wide, codecValid && level <= maximum ? profile : -1, level, name.codec);
                }
            }
        }
        struct Alpha { const char* text; int profile; int maximum; const char* codec; };
        const Alpha alpha[] {{"dvav.se", 9, 5, "avc3"}, {"dvhe.dtb", 7, 9, "hev1"}, {"dvhe.dtr", 4, 9, "hev1"}, {"dvhe.st", 8, 13, "hev1"}, {"dvhe.stn", 5, 13, "hev1"}};
        for (auto a : alpha) {
            for (int level : {0, a.maximum, a.maximum + 1}) Check(std::string(a.text) + "." + std::to_string(level), wide, level <= a.maximum ? a.profile : -1, level, a.codec);
            Check(std::string(a.text) + "..+1.extra", wide, a.profile, 1, a.codec);
        }
        for (auto text : {"", ".", "...", "dvhe", "dvhe.04", "dvhe...", "dvhe.04.", "hvc1.04.1", "hev1.04.1", "dva1.se.1", "dvh1.st.1", "dvh1.dtr.1", "dvav.dtb.1",
                 "dvhe.4.1", "dvhe.+04.1", "dvhe. 04.1", "dvhe.04.-1", "dvhe.04.256", "dvhe.04.1x", "dvhe.04.0x1", "dvhe.04.+", "dvhe.04. ", "dvhe.0x4.1", "dvhe.04.1 2",
                 "dvhe.0256.1", "dvhe.04.99999999999", "dvhe.se.1", "dvav.st.1", "dvhe.dt.1", "dvhe.stnn.1"})
            Check(text, wide);
        for (auto text : {"DVHE.04.1", "DvH1.08.13", "DVA1.09.5", "dvAV.09.1", "Dvhe.dtb.6", "dvhe.DTB.6", "dvhe.StN.13", "dvav.SE.1", "dvh\x05.04.1", "dva\x11.09.1", "dvav.s\x05.1"})
            Check(text, wide);
        Check(".dvhe.04.1", wide, 4, 1, "hev1");
        Check(".dvhe.dtr.1", wide);
        Check("..dvhe...04...1..", wide, 4, 1, "hev1");
        Check("dvhe.04.1.extra", wide, 4, 1, "hev1");
        Check("dvhe.04.1.", wide, 4, 1, "hev1");
        Check("dvhe.04..1", wide, 4, 1, "hev1");
        Check("dvhe.0004.01", wide, 4, 1, "hev1");
        Check("dvhe.04 \t. +1\r\n", wide, 4, 1, "hev1");
        Check("dvhe.04.\v\f+1\t", wide, 4, 1, "hev1");
        Check("dvhe.04.00000000000000000000000000001", wide, 4, 1, "hev1");
        Check("dvhe.04.0", wide, 4, 0, "hev1");
        Check("dvh1.08.13", wide, 8, 13, "hvc1");
        Check("dva1.09.5", wide, 9, 5, "avc1");
        Check("dvav.se.1.extra", wide, 9, 1, "avc3");
        Check(std::string("dvhe.04.1\0junk", 14), wide);
        Check("dvhe.04.\xa0" "1", wide);
        Check("dvhe.04.\x85" "1", wide);
    }
    CheckWide(u"dvhe.04.\u20031", "em space before the level", 4, 1, "hev1");
    CheckWide(u"dvhe.04.1\u3000", "ideographic space after the level", 4, 1, "hev1");
    CheckWide(u"dvhe.04\u1680.\u2028\u205f+9\u200a", "white-space-neutral characters", 4, 9, "hev1");
    CheckWide(u"dvhe.04.\u20291", "paragraph separator");
    CheckWide(u"dvhe.04.\u180e1", "mongolian vowel separator");
    CheckWide(u"dvhe.04.\u200b1", "zero width space");
    CheckWide(u"dvhe.0\u0134.1", "non-digit in the profile");
    CheckWide(u"dvhe.04.\uff11", "fullwidth digit");
    CheckWide(u"dvh\u0165.04.1", "non-Latin-1 letter in the codec");
    StringImpl broken {1, 3, nullptr, 4};
    String input(&broken);
    try {
        _ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(input);
        std::abort();
    } catch (const std::invalid_argument&) {
    }
    input.impl = nullptr;
    std::printf("%u DoVi cases passed, each through the returned object and through the hidden result pointer\n", checks);
}
