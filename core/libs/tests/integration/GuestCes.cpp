#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void);
int APS5_VABI sceCesSbcToUtf8(const std::uint8_t*, std::uint8_t, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToSbc(const std::uint8_t*, std::uint32_t, std::uint32_t*, const std::uint8_t*, std::uint8_t*);
}

static void Require(bool value) { if (!value) std::abort(); }

static void RequireSbcToUtf8(const std::uint8_t* profile, std::uint8_t sbc, const char* expected) {
    std::uint8_t utf8[4] = {};
    std::uint32_t length = 0;
    Require(sceCesSbcToUtf8(profile, sbc, utf8, sizeof(utf8), &length) == 0);
    Require(length == std::strlen(expected) && std::memcmp(utf8, expected, length) == 0);
}

static void RequireUtf8ToSbc(const std::uint8_t* profile, const char* utf8, std::uint8_t expected) {
    const auto size = static_cast<std::uint32_t>(std::strlen(utf8));
    std::uint32_t length = 0;
    std::uint8_t sbc = 0;
    Require(sceCesUtf8ToSbc(reinterpret_cast<const std::uint8_t*>(utf8), size + 4, &length, profile, &sbc) == 0);
    Require(length == size && sbc == expected);
}

static int Utf8ToSbc(const std::uint8_t* profile, const char* utf8, std::uint32_t utf8max) {
    std::uint32_t length = 0;
    std::uint8_t sbc = 0;
    return sceCesUtf8ToSbc(reinterpret_cast<const std::uint8_t*>(utf8), utf8max, &length, profile, &sbc);
}

int main() {
    constexpr int invalidParameter = static_cast<int>(0x805C0001);
    constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
    constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
    constexpr int invalidEncode = static_cast<int>(0x805C0014);
    constexpr int unassignedCode = static_cast<int>(0x805C0020);
    constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
    constexpr int dstBufferEnd = static_cast<int>(0x805C0031);

    const std::uint8_t* profile = sceCesRefersUcsProfileCp1252();
    Require(profile != nullptr && profile == sceCesRefersUcsProfileCp1252());

    RequireSbcToUtf8(profile, 'A', "A");
    RequireSbcToUtf8(profile, 0x80, "\xE2\x82\xAC");
    RequireSbcToUtf8(profile, 0x9F, "\xC5\xB8");
    RequireSbcToUtf8(profile, 0x81, "\xC2\x81");
    RequireSbcToUtf8(profile, 0xE9, "\xC3\xA9");
    RequireSbcToUtf8(profile, 0xFF, "\xC3\xBF");

    RequireUtf8ToSbc(profile, "A", 'A');
    RequireUtf8ToSbc(profile, "\xE2\x82\xAC", 0x80);
    RequireUtf8ToSbc(profile, "\xE2\x84\xA2", 0x99);
    RequireUtf8ToSbc(profile, "\xC3\xA9", 0xE9);
    RequireUtf8ToSbc(profile, "\xC2\x8D", 0x8D);
    for (int sbc = 1; sbc < 256; ++sbc) {
        std::uint8_t utf8[4] = {};
        std::uint32_t length = 0;
        std::uint8_t back = 0;
        Require(sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(sbc), utf8, sizeof(utf8), &length) == 0);
        Require(sceCesUtf8ToSbc(utf8, length, &length, profile, &back) == 0 && back == sbc);
    }

    std::uint8_t utf8[4] = {};
    std::uint32_t length = 0;
    std::uint8_t sbc = 0;
    Require(sceCesSbcToUtf8(nullptr, 'A', utf8, sizeof(utf8), &length) == invalidParameter);
    Require(sceCesSbcToUtf8(profile, 'A', nullptr, sizeof(utf8), &length) == invalidDstBuffer);
    Require(sceCesSbcToUtf8(profile, 0x80, utf8, 2, &length) == dstBufferEnd);
    Require(sceCesUtf8ToSbc(utf8, 1, &length, nullptr, &sbc) == invalidParameter);
    Require(sceCesUtf8ToSbc(nullptr, 1, &length, profile, &sbc) == invalidSrcBuffer);
    Require(sceCesUtf8ToSbc(utf8, 1, &length, profile, nullptr) == invalidDstBuffer);
    Require(Utf8ToSbc(profile, "A", 0) == srcBufferEnd);
    Require(Utf8ToSbc(profile, "\xE2\x82\xAC", 2) == srcBufferEnd);
    Require(Utf8ToSbc(profile, "\x80", 1) == invalidEncode);
    Require(Utf8ToSbc(profile, "\xC0\xAF", 2) == invalidEncode);
    Require(Utf8ToSbc(profile, "\xED\xA0\x80", 3) == invalidEncode);
    Require(Utf8ToSbc(profile, "\xE3\x81\x82", 3) == unassignedCode);
    Require(Utf8ToSbc(profile, "\xF0\x9F\x98\x80", 4) == unassignedCode);
}
