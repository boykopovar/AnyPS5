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
using WebCore::DoViParameters;
using Codec = DoViParameters::Codec;

struct RegisterResult {
    uint64_t fields;
    uint32_t engaged;
};
static_assert(sizeof(RegisterResult) == 16);
using RawParse = RegisterResult (APS5_VABI *)(const String&);
const auto rawParse = std::bit_cast<RawParse>(&_ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE);
unsigned checks = 0;

void Check(const std::string& text, bool wide, int profile = -1, int level = 0, Codec codec = Codec::HVC1) {
    std::u16string text16(text.begin(), text.end());
    StringImpl impl {1, static_cast<unsigned>(text.size()), wide ? static_cast<const void*>(text16.data()) : text.data(), wide ? 0u : 4u};
    String input(&impl);
    auto result = _ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(input);
    auto raw = rawParse(input);
    std::array<unsigned char, 12> bytes;
    std::memcpy(bytes.data(), &result, bytes.size());
    bool valid = profile >= 0;
    if (result.has_value() != valid || bytes[offsetof(WebCore::DoViOptionalAbi, engaged)] != valid || (raw.engaged & 255) != valid ||
        (valid && (result->codec != codec || result->bitstreamProfileID != profile || result->bitstreamLevelID != level ||
            raw.fields != (static_cast<uint64_t>(codec) | (static_cast<uint64_t>(profile) << 32) | (static_cast<uint64_t>(level) << 48))))) {
        std::fprintf(stderr, "DoVi failed (%s): %s expected profile %d level %d\n", wide ? "16" : "8", text.c_str(), profile, level);
        std::abort();
    }
    ++checks;
}

}  // namespace

int main() {
    String null;
    if (_ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(null) || (rawParse(null).engaged & 255)) std::abort();
    struct Name { const char* name; Codec codec; };
    const Name names[] {{"dva1", Codec::AVC1}, {"dvav", Codec::AVC3}, {"dvhe", Codec::HEV1}, {"dvh1", Codec::HVC1}};
    struct Profile { int id; int maximum; };
    const Profile profiles[] {{4, 9}, {5, 13}, {7, 9}, {8, 13}, {9, 5}};
    for (bool wide : {false, true}) {
        for (auto name : names) {
            for (int profile = 0; profile <= 10; ++profile) {
                int maximum = -1;
                for (auto p : profiles) if (p.id == profile) maximum = p.maximum;
                bool codecValid = profile == 9 ? name.codec == Codec::AVC1 || name.codec == Codec::AVC3 : name.codec == Codec::HEV1 || name.codec == Codec::HVC1;
                for (int level = 0; level <= 14; ++level) {
                    auto text = std::string(name.name) + ".0" + std::to_string(profile) + "." + std::to_string(level);
                    Check(text, wide, codecValid && level <= maximum ? profile : -1, level, name.codec);
                }
            }
        }
        struct Alpha { const char* text; int profile; int maximum; Codec codec; };
        const Alpha alpha[] {{"dvav.se", 9, 5, Codec::AVC3}, {"dvhe.dtb", 7, 9, Codec::HEV1}, {"dvhe.dtr", 4, 9, Codec::HEV1}, {"dvhe.st", 8, 13, Codec::HEV1}, {"dvhe.stn", 5, 13, Codec::HEV1}};
        for (auto a : alpha) {
            for (int level : {0, a.maximum, a.maximum + 1}) Check(std::string(a.text) + "." + std::to_string(level), wide, level <= a.maximum ? a.profile : -1, level, a.codec);
            Check(std::string(a.text) + "..+1.extra", wide, a.profile, 1, a.codec);
        }
        for (auto text : {"", ".", "dvhe", "dvhe.04", "dvhe...", "dvh\x05.04.1", "dvav.s\x05.1", "hvc1.04.1", "dva1.se.1", "dvh1.st.1", "dvhe.SE.1", "dvhe.4.1", "dvhe.+04.1", "dvhe. 04.1", "dvhe.0256.1", "dvhe.09999999999999999.1", "dvhe.04.256", "dvhe.04.999999999999999999999", "dvhe.04.-1", "dvhe.04.+", "dvhe.04.1x", "dvhe.04.1e0", "dvhe.04.0x1", "dvhe.04.\xC2\xA0" "1", "dvhe.0+4.1", "dvhe.0 4.1", "dvhe..st.1", ".dvhe.st.1", "dvhe.04.\v", "dvhe.04.1 2"}) Check(text, wide);
        for (auto text : {"dvhe.04.1.extra", "dvhe.04.1.", "dvhe.04..1", "..dvhe...04...1..", "dvhe.0004.01", "dvhe.04 \t. +1\r\n", "dvhe.04.\v\f+1\t", "dvhe.04.00000000000000000000000000001"}) Check(text, wide, 4, 1, Codec::HEV1);
        Check("dvhe.04.0", wide, 4, 0, Codec::HEV1);
        Check("DVHE.04.1", wide, 4, 1, Codec::HEV1);
        Check("DvH1.08.13", wide, 8, 13, Codec::HVC1);
        Check("DVA1.09.5", wide, 9, 5, Codec::AVC1);
        Check("dvAV.SE.1", wide, 9, 1, Codec::AVC3);
        Check("DVHE.DTB.6", wide, 7, 6, Codec::HEV1);
        Check("dvhe.StN.13", wide, 5, 13, Codec::HEV1);
        Check("dva\x11.09.1", wide, 9, 1, Codec::AVC1);
        Check(std::string("dvhe.04.1\0junk", 14), wide);
        Check("dvav.se.1.extra", wide, 9, 1, Codec::AVC3);
    }
    for (const auto& text : {std::u16string(u"dvhe.04.\u20031"), std::u16string(u"dvhe.0\u0134.1"), std::u16string(u"dvhe.04.\uff11")}) {
        StringImpl impl {1, static_cast<unsigned>(text.size()), text.data(), 0};
        String input(&impl);
        if (_ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(input)) std::abort();
    }
    StringImpl broken {1, 3, nullptr, 4};
    String input(&broken);
    try {
        _ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(input);
        std::abort();
    } catch (const std::invalid_argument&) {
    }
    std::printf("%u DoVi cases passed, plus null String and register ABI checks\n", checks);
}
