#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

extern "C" {
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void);
int APS5_VABI sceCesSbcToUtf8(const std::uint8_t*, std::uint8_t, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToSbc(const std::uint8_t*, std::uint32_t, std::uint32_t*, const std::uint8_t*, std::uint8_t*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int invalidParameter = static_cast<int>(0x805C0001);
constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
constexpr int invalidEncode = static_cast<int>(0x805C0014);
constexpr int unassignedCode = static_cast<int>(0x805C0020);
constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
constexpr int dstBufferEnd = static_cast<int>(0x805C0031);

struct SbcMapping {
    std::uint8_t sbc;
    const char* utf8;
};

const std::uint8_t* Profile() {
    const auto* profile = sceCesRefersUcsProfileCp1252();
    Require(profile != nullptr, "the cp1252 profile is null");
    return profile;
}

int Utf8ToSbc(const std::uint8_t* profile, const char* utf8, std::uint32_t utf8max) {
    std::uint32_t length = 0;
    std::uint8_t sbc = 0;
    return sceCesUtf8ToSbc(reinterpret_cast<const std::uint8_t*>(utf8), utf8max, &length, profile, &sbc);
}

const Case profileIdentity{"RefersUcsProfileCp1252_RepeatedCalls_ReturnSameProfile", [] {
    const auto* profile = Profile();
    Require(profile == sceCesRefersUcsProfileCp1252(), "the profile pointer changed between calls");
}};

const Case sbcToUtf8{"SbcToUtf8_Cp1252Characters_EncodeExpectedUtf8", [] {
    const auto* profile = Profile();
    for (const auto& mapping : {SbcMapping{'A', "A"}, SbcMapping{0x80, "\xE2\x82\xAC"}, SbcMapping{0x9F, "\xC5\xB8"},
                                SbcMapping{0x81, "\xC2\x81"}, SbcMapping{0xE9, "\xC3\xA9"}, SbcMapping{0xFF, "\xC3\xBF"}}) {
        const std::string context = "sbc " + std::to_string(mapping.sbc);
        std::uint8_t utf8[4] = {};
        std::uint32_t length = 0;
        RequireEqual(sceCesSbcToUtf8(profile, mapping.sbc, utf8, sizeof(utf8), &length), 0, context);
        RequireEqual(length, static_cast<std::uint32_t>(std::strlen(mapping.utf8)), context + ": length");
        Require(std::memcmp(utf8, mapping.utf8, length) == 0, context + ": encoded bytes differ");
    }
}};

const Case utf8ToSbc{"Utf8ToSbc_Cp1252Characters_DecodeExpectedSbc", [] {
    const auto* profile = Profile();
    for (const auto& mapping : {SbcMapping{'A', "A"}, SbcMapping{0x80, "\xE2\x82\xAC"}, SbcMapping{0x99, "\xE2\x84\xA2"},
                                SbcMapping{0xE9, "\xC3\xA9"}, SbcMapping{0x8D, "\xC2\x8D"}}) {
        const std::string context = "sbc " + std::to_string(mapping.sbc);
        const auto size = static_cast<std::uint32_t>(std::strlen(mapping.utf8));
        std::uint32_t length = 0;
        std::uint8_t sbc = 0;
        RequireEqual(sceCesUtf8ToSbc(reinterpret_cast<const std::uint8_t*>(mapping.utf8), size + 4, &length, profile, &sbc), 0, context);
        RequireEqual(length, size, context + ": consumed length");
        RequireEqual(sbc, mapping.sbc, context + ": decoded sbc");
    }
}};

const Case roundTrip{"SbcToUtf8ThenBack_EveryNonZeroByte_RoundTrips", [] {
    const auto* profile = Profile();
    for (int sbc = 1; sbc < 256; ++sbc) {
        const std::string context = "sbc " + std::to_string(sbc);
        std::uint8_t utf8[4] = {};
        std::uint32_t length = 0;
        std::uint8_t back = 0;
        RequireEqual(sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(sbc), utf8, sizeof(utf8), &length), 0, context + ": encode");
        RequireEqual(sceCesUtf8ToSbc(utf8, length, &length, profile, &back), 0, context + ": decode");
        RequireEqual(static_cast<int>(back), sbc, context + ": round trip");
    }
}};

const Case sbcToUtf8Errors{"SbcToUtf8_InvalidArguments_ReturnMatchingErrors", [] {
    const auto* profile = Profile();
    std::uint8_t utf8[4] = {};
    std::uint32_t length = 0;
    RequireEqual(sceCesSbcToUtf8(nullptr, 'A', utf8, sizeof(utf8), &length), invalidParameter, "null profile");
    RequireEqual(sceCesSbcToUtf8(profile, 'A', nullptr, sizeof(utf8), &length), invalidDstBuffer, "null destination");
    RequireEqual(sceCesSbcToUtf8(profile, 0x80, utf8, 2, &length), dstBufferEnd, "destination too small for the euro sign");
}};

const Case utf8ToSbcArgumentErrors{"Utf8ToSbc_InvalidArguments_ReturnMatchingErrors", [] {
    const auto* profile = Profile();
    std::uint8_t utf8[4] = {};
    std::uint32_t length = 0;
    std::uint8_t sbc = 0;
    RequireEqual(sceCesUtf8ToSbc(utf8, 1, &length, nullptr, &sbc), invalidParameter, "null profile");
    RequireEqual(sceCesUtf8ToSbc(nullptr, 1, &length, profile, &sbc), invalidSrcBuffer, "null source");
    RequireEqual(sceCesUtf8ToSbc(utf8, 1, &length, profile, nullptr), invalidDstBuffer, "null destination");
}};

const Case utf8ToSbcTruncated{"Utf8ToSbc_SourceShorterThanSequence_ReturnsSrcBufferEnd", [] {
    const auto* profile = Profile();
    RequireEqual(Utf8ToSbc(profile, "A", 0), srcBufferEnd, "empty source");
    RequireEqual(Utf8ToSbc(profile, "\xE2\x82\xAC", 2), srcBufferEnd, "truncated euro sign");
}};

const Case utf8ToSbcInvalid{"Utf8ToSbc_MalformedSequence_ReturnsInvalidEncode", [] {
    const auto* profile = Profile();
    RequireEqual(Utf8ToSbc(profile, "\x80", 1), invalidEncode, "lone continuation byte");
    RequireEqual(Utf8ToSbc(profile, "\xC0\xAF", 2), invalidEncode, "overlong encoding");
    RequireEqual(Utf8ToSbc(profile, "\xED\xA0\x80", 3), invalidEncode, "surrogate code point");
}};

const Case utf8ToSbcUnassigned{"Utf8ToSbc_CodePointOutsideCp1252_ReturnsUnassignedCode", [] {
    const auto* profile = Profile();
    RequireEqual(Utf8ToSbc(profile, "\xE3\x81\x82", 3), unassignedCode, "hiragana a");
    RequireEqual(Utf8ToSbc(profile, "\xF0\x9F\x98\x80", 4), unassignedCode, "emoji");
}};

} // namespace
