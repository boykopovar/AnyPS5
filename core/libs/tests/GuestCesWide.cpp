#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1251(void);
void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void*);
void* APS5_VABI sceCesUcsProfileInitEucJpCp51932(void*);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
int APS5_VABI sceCesSbcToUcs2(const std::uint8_t*, std::uint8_t, std::uint16_t*);
int APS5_VABI sceCesSbcToUtf16(const std::uint8_t*, std::uint8_t, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf16be(const std::uint8_t*, std::uint8_t, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf16le(const std::uint8_t*, std::uint8_t, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf32(const std::uint8_t*, std::uint8_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf32be(const std::uint8_t*, std::uint8_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf32le(const std::uint8_t*, std::uint8_t, std::uint32_t*);
int APS5_VABI sceCesUcs2ToSbc(std::uint16_t, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesUtf16ToSbc(const std::uint16_t*, std::uint32_t, std::uint32_t*, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesUtf16beToSbc(const std::uint16_t*, std::uint32_t, std::uint32_t*, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesUtf16leToSbc(const std::uint16_t*, std::uint32_t, std::uint32_t*, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesUtf32ToSbc(std::uint32_t, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesUtf32beToSbc(const std::uint32_t*, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesUtf32leToSbc(const std::uint32_t*, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesUtf16StrToMbcsStr(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16StrGetMbcsLen(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUtf32StrToMbcsStr(void*, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32StrGetMbcsLen(void*, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUtf16Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUtf16Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUtf32Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUtf32Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
}

static void Require(bool value) { if (!value) std::abort(); }

static const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

int main() {
    constexpr int invalidParameter = static_cast<int>(0x805C0001);
    constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
    constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
    constexpr int invalidEncode = static_cast<int>(0x805C0014);
    constexpr int unassignedCode = static_cast<int>(0x805C0020);
    constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
    constexpr int dstBufferEnd = static_cast<int>(0x805C0031);

    const std::uint8_t* cp1252 = sceCesRefersUcsProfileCp1252();
    const std::uint8_t* cp1251 = sceCesRefersUcsProfileCp1251();
    alignas(8) std::uint8_t sjisSheet[64] = {};
    const void* sjis = sceCesUcsProfileInitSJis1997Cp932(sjisSheet);

    std::uint16_t ucs2 = 0;
    std::uint16_t utf16 = 0;
    std::uint32_t utf32 = 0;
    std::uint32_t length = 0;
    Require(sceCesSbcToUcs2(cp1252, 0x80, &ucs2) == 0 && ucs2 == 0x20AC);
    Require(sceCesSbcToUcs2(cp1251, 0xC0, &ucs2) == 0 && ucs2 == 0x0410);
    Require(sceCesSbcToUcs2(static_cast<const std::uint8_t*>(sjis), 0x41, &ucs2) == invalidParameter);
    Require(sceCesSbcToUcs2(cp1252, 0x41, nullptr) == invalidDstBuffer);
    Require(sceCesSbcToUtf16(cp1252, 0x80, &utf16, 1, &length) == 0 && utf16 == 0x20AC && length == 1);
    Require(sceCesSbcToUtf16(cp1252, 0x80, &utf16, 0, &length) == dstBufferEnd);
    Require(sceCesSbcToUtf16be(cp1252, 0x80, &utf16, 1, &length) == 0 && std::memcmp(&utf16, "\x20\xAC", 2) == 0);
    Require(sceCesSbcToUtf16le(cp1252, 0x80, &utf16, 1, &length) == 0 && std::memcmp(&utf16, "\xAC\x20", 2) == 0);
    Require(sceCesSbcToUtf32(cp1252, 0x9F, &utf32) == 0 && utf32 == 0x0178);
    Require(sceCesSbcToUtf32be(cp1252, 0x9F, &utf32) == 0 && std::memcmp(&utf32, "\x00\x00\x01\x78", 4) == 0);
    Require(sceCesSbcToUtf32le(cp1252, 0x9F, &utf32) == 0 && std::memcmp(&utf32, "\x78\x01\x00\x00", 4) == 0);

    std::uint8_t sbc = 0;
    Require(sceCesUcs2ToSbc(0x20AC, cp1252, &sbc) == 0 && sbc == 0x80);
    Require(sceCesUcs2ToSbc(0x0410, cp1251, &sbc) == 0 && sbc == 0xC0);
    Require(sceCesUcs2ToSbc(0x3042, cp1252, &sbc) == unassignedCode);
    Require(sceCesUcs2ToSbc(0xD800, cp1252, &sbc) == invalidEncode);
    Require(sceCesUcs2ToSbc(0x41, cp1252, nullptr) == invalidDstBuffer);
    Require(sceCesUcs2ToSbc(0x41, static_cast<const std::uint8_t*>(sjis), &sbc) == invalidParameter);
    const std::uint16_t euro[] = {0x20AC};
    const std::uint16_t pair[] = {0xD83D, 0xDE00};
    Require(sceCesUtf16ToSbc(euro, 1, &length, cp1252, &sbc) == 0 && sbc == 0x80 && length == 1);
    Require(sceCesUtf16ToSbc(pair, 2, &length, cp1252, &sbc) == unassignedCode);
    Require(sceCesUtf16ToSbc(pair, 1, &length, cp1252, &sbc) == srcBufferEnd);
    Require(sceCesUtf16ToSbc(euro, 0, &length, cp1252, &sbc) == srcBufferEnd);
    Require(sceCesUtf16ToSbc(nullptr, 1, &length, cp1252, &sbc) == invalidSrcBuffer);
    std::uint16_t euroBe = 0;
    std::memcpy(&euroBe, "\x20\xAC", 2);
    Require(sceCesUtf16beToSbc(&euroBe, 1, &length, cp1252, &sbc) == 0 && sbc == 0x80);
    std::uint16_t euroLe = 0;
    std::memcpy(&euroLe, "\xAC\x20", 2);
    Require(sceCesUtf16leToSbc(&euroLe, 1, &length, cp1252, &sbc) == 0 && sbc == 0x80);
    Require(sceCesUtf32ToSbc(0x0178, cp1252, &sbc) == 0 && sbc == 0x9F);
    Require(sceCesUtf32ToSbc(0x110000, cp1252, &sbc) == invalidEncode);
    std::uint32_t yBe = 0;
    std::memcpy(&yBe, "\x00\x00\x01\x78", 4);
    Require(sceCesUtf32beToSbc(&yBe, cp1252, &sbc) == 0 && sbc == 0x9F);
    std::uint32_t yLe = 0;
    std::memcpy(&yLe, "\x78\x01\x00\x00", 4);
    Require(sceCesUtf32leToSbc(&yLe, cp1252, &sbc) == 0 && sbc == 0x9F);
    Require(sceCesUtf32leToSbc(nullptr, cp1252, &sbc) == invalidSrcBuffer);
    for (int value = 0; value < 256; ++value) {
        if (value == 0x98) {
            Require(sceCesSbcToUtf32(cp1251, static_cast<std::uint8_t>(value), &utf32) == unassignedCode);
            continue;
        }
        Require(sceCesSbcToUtf32(cp1251, static_cast<std::uint8_t>(value), &utf32) == 0);
        Require(sceCesUtf32ToSbc(utf32, cp1251, &sbc) == 0 && sbc == value);
    }

    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesMbcsUcsContextInit(context, sjis) == 0);
    const std::uint16_t wide[] = {0x3042, 'A', 0xFF71, 0};
    std::uint8_t mbcs[8] = {};
    std::uint32_t mbcsLength = 0;
    Require(sceCesUtf16StrToMbcsStr(context, wide, 4, &length, mbcs, sizeof(mbcs), &mbcsLength) == 0);
    Require(length == 3 && mbcsLength == 4 && std::memcmp(mbcs, "\x82\xA0" "A\xB1", 4) == 0);
    Require(sceCesUtf16StrGetMbcsLen(context, wide, 4, &length, &mbcsLength) == 0 && length == 3 && mbcsLength == 4);
    Require(sceCesUtf16StrToMbcsStr(context, wide, 4, &length, mbcs, 3, &mbcsLength) == dstBufferEnd);
    Require(sceCesUtf16StrToMbcsStr(context, wide, 4, &length, nullptr, 3, &mbcsLength) == invalidDstBuffer);
    Require(sceCesUtf16StrToMbcsStr(context, pair, 2, &length, mbcs, sizeof(mbcs), &mbcsLength) == unassignedCode);
    const std::uint32_t wide32[] = {0x65E5, 'B', 0};
    Require(sceCesUtf32StrToMbcsStr(context, wide32, 3, &length, mbcs, sizeof(mbcs), &mbcsLength) == 0);
    Require(length == 2 && mbcsLength == 3 && std::memcmp(mbcs, "\x93\xFA" "B", 3) == 0);
    Require(sceCesUtf32StrGetMbcsLen(context, wide32, 1, &length, &mbcsLength) == 0 && length == 1 && mbcsLength == 2);
    const std::uint32_t bad32[] = {0xDC00};
    Require(sceCesUtf32StrGetMbcsLen(context, bad32, 1, &length, &mbcsLength) == invalidEncode);

    std::uint16_t utf16Out[4] = {};
    std::uint32_t outLength = 0;
    Require(sceCesMbcsStrToUtf16Str(context, Bytes("\x82\xA0" "A\xB1"), 8, &length, utf16Out, 4, &outLength) == 0);
    Require(length == 4 && outLength == 3 && utf16Out[0] == 0x3042 && utf16Out[1] == 'A' && utf16Out[2] == 0xFF71);
    Require(sceCesMbcsStrGetUtf16Len(context, Bytes("\x82\xA0" "A"), 3, &length, &outLength) == 0 && length == 3 && outLength == 2);
    Require(sceCesMbcsStrToUtf16Str(context, Bytes("\x82\xA0" "A"), 3, &length, utf16Out, 1, &outLength) == dstBufferEnd);
    Require(sceCesMbcsStrToUtf16Str(context, Bytes("\x82"), 1, &length, utf16Out, 4, &outLength) == srcBufferEnd);
    std::uint32_t utf32Out[4] = {};
    Require(sceCesMbcsStrToUtf32Str(context, Bytes("\x93\xFA" "B"), 3, &length, utf32Out, 4, &outLength) == 0);
    Require(length == 3 && outLength == 2 && utf32Out[0] == 0x65E5 && utf32Out[1] == 'B');
    Require(sceCesMbcsStrToUtf32Str(context, Bytes("A"), 1, &length, nullptr, 4, &outLength) == invalidDstBuffer);
    Require(sceCesMbcsStrGetUtf32Len(context, Bytes("AB"), 2, &length, &outLength) == 0 && outLength == 2);
    Require(sceCesMbcsStrGetUtf32Len(nullptr, Bytes("AB"), 2, &length, &outLength) == invalidParameter);

    Require(sceCesMbcsUcsContextInit(context, cp1252) == 0);
    Require(sceCesMbcsStrToUtf16Str(context, Bytes("\x80\xE9"), 2, &length, utf16Out, 4, &outLength) == 0);
    Require(outLength == 2 && utf16Out[0] == 0x20AC && utf16Out[1] == 0x00E9);
    return 0;
}
