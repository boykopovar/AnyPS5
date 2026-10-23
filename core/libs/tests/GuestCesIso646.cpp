#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>

extern "C" {
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileAscii(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileAsciiWithKatakana(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileBs4730(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileGbT1988(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileGbT1988Tilde0x7e(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileJisX0201(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileJisX0201Katakana(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileJisX0201Roman(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileJisX0201RomanTilde0x7e(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileJisX0201Tilde0x7e(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileKsX1003(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileKsX1003Tilde0x7e(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_1(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void);
int APS5_VABI sceCesSbcToUtf8(const std::uint8_t*, std::uint8_t, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToSbc(const std::uint8_t*, std::uint32_t, std::uint32_t*, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesSbcToUcs2(const std::uint8_t*, std::uint8_t, std::uint16_t*);
int APS5_VABI sceCesUcs2ToSbc(std::uint16_t, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesSbcToUtf32(const std::uint8_t*, std::uint8_t, std::uint32_t*);
int APS5_VABI sceCesUtf32ToSbc(std::uint32_t, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
int APS5_VABI sceCesMbcsStrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8StrToMbcsStr(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
}

namespace {

constexpr int unassignedCode = static_cast<int>(0x805C0020);

void Require(bool value) { if (!value) std::abort(); }

void RequireByte(const std::uint8_t* profile, std::uint8_t sbc, std::uint32_t code) {
    std::uint8_t utf8[4];
    std::uint32_t length = 0;
    Require(sceCesSbcToUtf8(profile, sbc, utf8, sizeof(utf8), &length) == 0);
    std::uint32_t decoded = 0;
    if (length == 1) decoded = utf8[0];
    else if (length == 2) decoded = (utf8[0] & 0x1Fu) << 6 | (utf8[1] & 0x3Fu);
    else if (length == 3) decoded = (utf8[0] & 0x0Fu) << 12 | (utf8[1] & 0x3Fu) << 6 | (utf8[2] & 0x3Fu);
    else Require(false);
    Require(decoded == code);
    std::uint16_t ucs2 = 0;
    Require(sceCesSbcToUcs2(profile, sbc, &ucs2) == 0 && ucs2 == code);
    std::uint32_t utf32 = 0;
    Require(sceCesSbcToUtf32(profile, sbc, &utf32) == 0 && utf32 == code);
    std::uint8_t back = 0;
    Require(sceCesUtf8ToSbc(utf8, length, &length, profile, &back) == 0 && back == sbc);
    Require(sceCesUcs2ToSbc(static_cast<std::uint16_t>(code), profile, &back) == 0 && back == sbc);
    Require(sceCesUtf32ToSbc(code, profile, &back) == 0 && back == sbc);
}

void RequireByteUnassigned(const std::uint8_t* profile, std::uint8_t sbc) {
    std::uint8_t utf8[4];
    std::uint32_t length = 0;
    Require(sceCesSbcToUtf8(profile, sbc, utf8, sizeof(utf8), &length) == unassignedCode);
    std::uint16_t ucs2 = 0;
    Require(sceCesSbcToUcs2(profile, sbc, &ucs2) == unassignedCode);
    std::uint32_t utf32 = 0;
    Require(sceCesSbcToUtf32(profile, sbc, &utf32) == unassignedCode);
}

void RequireCodeUnassigned(const std::uint8_t* profile, std::uint32_t code) {
    std::uint8_t back = 0;
    Require(sceCesUtf32ToSbc(code, profile, &back) == unassignedCode);
    if (code < 0x10000) Require(sceCesUcs2ToSbc(static_cast<std::uint16_t>(code), profile, &back) == unassignedCode);
}

void CheckAsciiHalf(const std::uint8_t* profile) {
    for (std::uint32_t sbc = 0; sbc < 0x80; ++sbc) RequireByte(profile, static_cast<std::uint8_t>(sbc), sbc);
}

void CheckUpperHalfUnassigned(const std::uint8_t* profile) {
    for (std::uint32_t sbc = 0x80; sbc < 0x100; ++sbc) RequireByteUnassigned(profile, static_cast<std::uint8_t>(sbc));
    for (const std::uint32_t code : {0x00A0u, 0x00E9u, 0x00FFu, 0xFF61u, 0x30A2u}) RequireCodeUnassigned(profile, code);
}

void CheckKatakanaHalf(const std::uint8_t* profile) {
    for (std::uint32_t sbc = 0x80; sbc <= 0xA0; ++sbc) RequireByteUnassigned(profile, static_cast<std::uint8_t>(sbc));
    for (std::uint32_t sbc = 0xA1; sbc <= 0xDF; ++sbc) RequireByte(profile, static_cast<std::uint8_t>(sbc), 0xFF61 + (sbc - 0xA1));
    for (std::uint32_t sbc = 0xE0; sbc < 0x100; ++sbc) RequireByteUnassigned(profile, static_cast<std::uint8_t>(sbc));
    for (const std::uint32_t code : {0x00A0u, 0x00E9u, 0x30A2u, 0xFF60u, 0xFFA0u}) RequireCodeUnassigned(profile, code);
}

void CheckSevenBitVariant(const std::uint8_t* profile, std::uint8_t changedByte, std::uint32_t changedCode, bool overline) {
    for (std::uint32_t sbc = 0; sbc < 0x80; ++sbc) {
        if (sbc == changedByte) continue;
        if (sbc == 0x7E) continue;
        RequireByte(profile, static_cast<std::uint8_t>(sbc), sbc);
    }
    RequireByte(profile, changedByte, changedCode);
    RequireCodeUnassigned(profile, changedByte);
    RequireByte(profile, 0x7E, overline ? 0x203E : 0x007E);
    RequireCodeUnassigned(profile, overline ? 0x007E : 0x203E);
}

void CheckKatakanaOnly(const std::uint8_t* profile) {
    for (std::uint32_t sbc = 0; sbc < 0x21; ++sbc) RequireByte(profile, static_cast<std::uint8_t>(sbc), sbc);
    RequireByte(profile, 0x7F, 0x7F);
    for (std::uint32_t sbc = 0x21; sbc < 0x7F; ++sbc) {
        RequireByteUnassigned(profile, static_cast<std::uint8_t>(sbc));
        RequireCodeUnassigned(profile, sbc);
    }
    CheckKatakanaHalf(profile);
}

void CheckDistinct() {
    const std::uint8_t* profiles[] = {
        sceCesRefersUcsProfileAscii(), sceCesRefersUcsProfileAsciiWithKatakana(), sceCesRefersUcsProfileBs4730(),
        sceCesRefersUcsProfileGbT1988(), sceCesRefersUcsProfileGbT1988Tilde0x7e(), sceCesRefersUcsProfileJisX0201(),
        sceCesRefersUcsProfileJisX0201Katakana(), sceCesRefersUcsProfileJisX0201Roman(), sceCesRefersUcsProfileJisX0201RomanTilde0x7e(),
        sceCesRefersUcsProfileJisX0201Tilde0x7e(), sceCesRefersUcsProfileKsX1003(), sceCesRefersUcsProfileKsX1003Tilde0x7e(),
        sceCesRefersUcsProfileIso8859_1(), sceCesRefersUcsProfileCp1252(),
    };
    for (std::size_t i = 0; i < std::size(profiles); ++i) {
        Require(profiles[i] != nullptr);
        for (std::size_t j = 0; j < i; ++j) Require(profiles[i] != profiles[j]);
    }
    Require(sceCesRefersUcsProfileJisX0201() == sceCesRefersUcsProfileJisX0201());
}

void CheckStrings() {
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesMbcsUcsContextInit(context, sceCesRefersUcsProfileJisX0201()) == 0);
    const std::uint8_t jis[] = {0x5C, 0xB6, 0x41, 0x7E};
    std::uint8_t utf8[16] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesMbcsStrToUtf8Str(context, jis, sizeof(jis), &used, utf8, sizeof(utf8), &produced) == 0);
    Require(used == sizeof(jis) && produced == 9 && std::memcmp(utf8, "\xC2\xA5\xEF\xBD\xB6\x41\xE2\x80\xBE", 9) == 0);
    std::uint8_t back[8] = {};
    Require(sceCesUtf8StrToMbcsStr(context, utf8, produced, &used, back, sizeof(back), &produced) == 0);
    Require(used == 9 && produced == sizeof(jis) && std::memcmp(back, jis, sizeof(jis)) == 0);
    const std::uint8_t hole[] = {0x41, 0xE0};
    Require(sceCesMbcsStrToUtf8Str(context, hole, sizeof(hole), &used, utf8, sizeof(utf8), &produced) == unassignedCode);
    Require(sceCesUtf8StrToMbcsStr(context, reinterpret_cast<const std::uint8_t*>("\x5C"), 1, &used, back, sizeof(back), &produced) == unassignedCode);
    Require(sceCesMbcsUcsContextInit(context, sceCesRefersUcsProfileAscii()) == 0);
    Require(sceCesUtf8StrToMbcsStr(context, reinterpret_cast<const std::uint8_t*>("\x5C"), 1, &used, back, sizeof(back), &produced) == 0);
    Require(used == 1 && produced == 1 && back[0] == 0x5C);
}

}

int main() {
    CheckDistinct();
    CheckAsciiHalf(sceCesRefersUcsProfileAscii());
    CheckUpperHalfUnassigned(sceCesRefersUcsProfileAscii());
    for (const std::uint32_t code : {0x00A5u, 0x203Eu, 0x20A9u, 0x00A3u}) RequireCodeUnassigned(sceCesRefersUcsProfileAscii(), code);
    CheckAsciiHalf(sceCesRefersUcsProfileAsciiWithKatakana());
    CheckKatakanaHalf(sceCesRefersUcsProfileAsciiWithKatakana());
    CheckSevenBitVariant(sceCesRefersUcsProfileBs4730(), 0x23, 0x00A3, true);
    CheckUpperHalfUnassigned(sceCesRefersUcsProfileBs4730());
    CheckSevenBitVariant(sceCesRefersUcsProfileGbT1988(), 0x24, 0x00A5, true);
    CheckUpperHalfUnassigned(sceCesRefersUcsProfileGbT1988());
    CheckSevenBitVariant(sceCesRefersUcsProfileGbT1988Tilde0x7e(), 0x24, 0x00A5, false);
    CheckUpperHalfUnassigned(sceCesRefersUcsProfileGbT1988Tilde0x7e());
    CheckSevenBitVariant(sceCesRefersUcsProfileJisX0201(), 0x5C, 0x00A5, true);
    CheckKatakanaHalf(sceCesRefersUcsProfileJisX0201());
    CheckKatakanaOnly(sceCesRefersUcsProfileJisX0201Katakana());
    CheckSevenBitVariant(sceCesRefersUcsProfileJisX0201Roman(), 0x5C, 0x00A5, true);
    CheckUpperHalfUnassigned(sceCesRefersUcsProfileJisX0201Roman());
    CheckSevenBitVariant(sceCesRefersUcsProfileJisX0201RomanTilde0x7e(), 0x5C, 0x00A5, false);
    CheckUpperHalfUnassigned(sceCesRefersUcsProfileJisX0201RomanTilde0x7e());
    CheckSevenBitVariant(sceCesRefersUcsProfileJisX0201Tilde0x7e(), 0x5C, 0x00A5, false);
    CheckKatakanaHalf(sceCesRefersUcsProfileJisX0201Tilde0x7e());
    CheckSevenBitVariant(sceCesRefersUcsProfileKsX1003(), 0x5C, 0x20A9, true);
    CheckUpperHalfUnassigned(sceCesRefersUcsProfileKsX1003());
    CheckSevenBitVariant(sceCesRefersUcsProfileKsX1003Tilde0x7e(), 0x5C, 0x20A9, false);
    CheckUpperHalfUnassigned(sceCesRefersUcsProfileKsX1003Tilde0x7e());
    CheckStrings();
    RequireByte(sceCesRefersUcsProfileIso8859_1(), 0xE9, 0x00E9);
    return 0;
}
