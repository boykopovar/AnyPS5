#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void*);
void* APS5_VABI sceCesUcsProfileInitEucJpCp51932(void*);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void);
int APS5_VABI sceCesSJisToUcs2(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
int APS5_VABI sceCesSJisToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSJisToUtf16(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSJisToUtf16be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSJisToUtf16le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSJisToUtf32(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesSJisToUtf32be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesSJisToUtf32le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesEucJpToUtf16(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucJpToUtf16be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucJpToUtf32(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesEucJpToUtf32le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUcs2ToSJis(std::uint16_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUcs2ToEucJp(std::uint16_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToSJis(const std::uint8_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToEucJp(const std::uint8_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16ToSJis(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16beToEucJp(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32ToSJis(std::uint32_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32beToSJis(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32leToEucJp(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSJisCodeToJisX0208(std::uint16_t, std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesJisX0208ToSJisCode(std::uint8_t, std::uint8_t, std::uint16_t*);
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

    alignas(8) std::uint8_t sjisSheet[64] = {};
    alignas(8) std::uint8_t eucSheet[64] = {};
    const void* sjis = sceCesUcsProfileInitSJis1997Cp932(sjisSheet);
    const void* euc = sceCesUcsProfileInitEucJpCp51932(eucSheet);
    Require(sjis != nullptr && euc != nullptr);

    std::uint32_t consumed = 0;
    std::uint32_t produced = 0;
    std::uint16_t ucs2 = 0;
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\xA0"), 2, &consumed, &ucs2) == 0 && consumed == 2 && ucs2 == 0x3042);
    Require(sceCesSJisToUcs2(sjis, Bytes("\xB1"), 1, &consumed, &ucs2) == 0 && consumed == 1 && ucs2 == 0xFF71);
    Require(sceCesSJisToUcs2(sjis, Bytes("A"), 1, &consumed, &ucs2) == 0 && consumed == 1 && ucs2 == 'A');
    Require(sceCesSJisToUcs2(sjis, Bytes("\x81\x60"), 2, &consumed, &ucs2) == 0 && ucs2 == 0xFF5E);
    Require(sceCesSJisToUcs2(sjis, Bytes("\xFA\x40"), 2, &consumed, &ucs2) == 0 && ucs2 == 0x2170);
    Require(sceCesSJisToUcs2(euc, Bytes("\x82\xA0"), 2, &consumed, &ucs2) == invalidParameter);
    Require(sceCesSJisToUcs2(sjis, nullptr, 2, &consumed, &ucs2) == invalidSrcBuffer);
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\xA0"), 0, &consumed, &ucs2) == srcBufferEnd);
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\xA0"), 1, &consumed, &ucs2) == srcBufferEnd);
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\xA0"), 2, &consumed, nullptr) == invalidDstBuffer);
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\x7F"), 2, &consumed, &ucs2) == invalidEncode);
    Require(sceCesSJisToUcs2(sjis, Bytes("\xF0\x40"), 2, &consumed, &ucs2) == unassignedCode);

    std::uint8_t utf8[4] = {};
    Require(sceCesSJisToUtf8(sjis, Bytes("\x93\xFA"), 2, &consumed, utf8, sizeof(utf8), &produced) == 0);
    Require(consumed == 2 && produced == 3 && std::memcmp(utf8, "\xE6\x97\xA5", 3) == 0);
    Require(sceCesSJisToUtf8(sjis, Bytes("\x93\xFA"), 2, &consumed, utf8, 2, &produced) == dstBufferEnd);

    std::uint16_t utf16 = 0;
    Require(sceCesSJisToUtf16(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 1, &produced) == 0 && utf16 == 0x3042 && produced == 1);
    Require(sceCesSJisToUtf16(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 0, &produced) == dstBufferEnd);
    Require(sceCesSJisToUtf16be(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 1, &produced) == 0);
    Require(std::memcmp(&utf16, "\x30\x42", 2) == 0);
    Require(sceCesSJisToUtf16le(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 1, &produced) == 0);
    Require(std::memcmp(&utf16, "\x42\x30", 2) == 0);

    std::uint32_t utf32 = 0;
    Require(sceCesSJisToUtf32(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf32) == 0 && utf32 == 0x3042);
    Require(sceCesSJisToUtf32be(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf32) == 0);
    Require(std::memcmp(&utf32, "\x00\x00\x30\x42", 4) == 0);
    Require(sceCesSJisToUtf32le(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf32) == 0);
    Require(std::memcmp(&utf32, "\x42\x30\x00\x00", 4) == 0);

    Require(sceCesEucJpToUtf16(euc, Bytes("\xA4\xA2"), 2, &consumed, &utf16, 1, &produced) == 0 && consumed == 2 && utf16 == 0x3042);
    Require(sceCesEucJpToUtf16be(euc, Bytes("\x8E\xB1"), 2, &consumed, &utf16, 1, &produced) == 0);
    Require(std::memcmp(&utf16, "\xFF\x71", 2) == 0);
    Require(sceCesEucJpToUtf32(euc, Bytes("\x8F\xB0\xA1"), 3, &consumed, &utf32) == 0 && consumed == 3 && utf32 == 0x4E02);
    Require(sceCesEucJpToUtf32le(euc, Bytes("\xA4\xA2"), 2, &consumed, &utf32) == 0);
    Require(std::memcmp(&utf32, "\x42\x30\x00\x00", 4) == 0);
    Require(sceCesEucJpToUtf32(sjis, Bytes("\xA4\xA2"), 2, &consumed, &utf32) == invalidParameter);

    std::uint8_t mbcs[4] = {};
    Require(sceCesUcs2ToSJis(0x3042, sjis, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && std::memcmp(mbcs, "\x82\xA0", 2) == 0);
    Require(sceCesUcs2ToSJis(0xFF71, sjis, mbcs, sizeof(mbcs), &produced) == 0 && produced == 1 && mbcs[0] == 0xB1);
    Require(sceCesUcs2ToSJis(0x3042, sjis, mbcs, 1, &produced) == dstBufferEnd);
    Require(sceCesUcs2ToSJis(0xD800, sjis, mbcs, sizeof(mbcs), &produced) == invalidEncode);
    Require(sceCesUcs2ToSJis(0x4E02, sjis, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUcs2ToSJis(0x3042, euc, mbcs, sizeof(mbcs), &produced) == invalidParameter);
    Require(sceCesUcs2ToSJis(0x3042, sceCesRefersUcsProfileCp1252(), mbcs, sizeof(mbcs), &produced) == invalidParameter);
    Require(sceCesUcs2ToSJis(0x3042, sjis, nullptr, sizeof(mbcs), &produced) == invalidDstBuffer);
    Require(sceCesUcs2ToEucJp(0x4E02, euc, mbcs, sizeof(mbcs), &produced) == 0 && produced == 3 && std::memcmp(mbcs, "\x8F\xB0\xA1", 3) == 0);
    Require(sceCesUcs2ToEucJp(0xFF71, euc, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && std::memcmp(mbcs, "\x8E\xB1", 2) == 0);

    Require(sceCesUtf8ToSJis(Bytes("\xE6\x97\xA5"), 3, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == 0);
    Require(consumed == 3 && produced == 2 && std::memcmp(mbcs, "\x93\xFA", 2) == 0);
    Require(sceCesUtf8ToSJis(Bytes("\xE6\x97\xA5"), 2, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == srcBufferEnd);
    Require(sceCesUtf8ToSJis(Bytes("\xF0\x9F\x98\x80"), 4, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUtf8ToSJis(nullptr, 3, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == invalidSrcBuffer);
    Require(sceCesUtf8ToEucJp(Bytes("\xE6\x97\xA5"), 3, &consumed, euc, mbcs, sizeof(mbcs), &produced) == 0);
    Require(produced == 2 && std::memcmp(mbcs, "\xC6\xFC", 2) == 0);

    const std::uint16_t hiragana[] = {0x3042};
    const std::uint16_t pair[] = {0xD83D, 0xDE00};
    const std::uint16_t lone[] = {0xDC00};
    Require(sceCesUtf16ToSJis(hiragana, 1, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == 0 && consumed == 1 && std::memcmp(mbcs, "\x82\xA0", 2) == 0);
    Require(sceCesUtf16ToSJis(pair, 2, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUtf16ToSJis(pair, 1, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == srcBufferEnd);
    Require(sceCesUtf16ToSJis(lone, 1, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == invalidEncode);
    std::uint16_t hiraganaBe = 0;
    std::memcpy(&hiraganaBe, "\x30\x42", 2);
    Require(sceCesUtf16beToEucJp(&hiraganaBe, 1, &consumed, euc, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\xA4\xA2", 2) == 0);

    Require(sceCesUtf32ToSJis(0x65E5, sjis, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\x93\xFA", 2) == 0);
    Require(sceCesUtf32ToSJis(0x110000, sjis, mbcs, sizeof(mbcs), &produced) == invalidEncode);
    std::uint32_t utf32Be = 0;
    std::memcpy(&utf32Be, "\x00\x00\x65\xE5", 4);
    Require(sceCesUtf32beToSJis(&utf32Be, sjis, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\x93\xFA", 2) == 0);
    Require(sceCesUtf32beToSJis(nullptr, sjis, mbcs, sizeof(mbcs), &produced) == invalidSrcBuffer);
    std::uint32_t utf32Le = 0;
    std::memcpy(&utf32Le, "\xE5\x65\x00\x00", 4);
    Require(sceCesUtf32leToEucJp(&utf32Le, euc, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\xC6\xFC", 2) == 0);

    for (int code = 1; code < 0x10000; ++code) {
        if (code >= 0xD800 && code <= 0xDFFF) continue;
        std::uint8_t encoded[4] = {};
        std::uint32_t size = 0;
        if (sceCesUcs2ToSJis(static_cast<std::uint16_t>(code), sjis, encoded, sizeof(encoded), &size) == 0) {
            std::uint16_t back = 0;
            Require(sceCesSJisToUcs2(sjis, encoded, size, &consumed, &back) == 0 && consumed == size && back == code);
        }
    }

    std::uint8_t ku = 0;
    std::uint8_t ten = 0;
    std::uint16_t sjisCode = 0;
    Require(sceCesSJisCodeToJisX0208(0x82A0, &ku, &ten) == 0 && ku == 4 && ten == 2);
    Require(sceCesSJisCodeToJisX0208(0x93FA, &ku, &ten) == 0 && ku == 38 && ten == 92);
    Require(sceCesSJisCodeToJisX0208(0xEFFC, &ku, &ten) == 0 && ku == 94 && ten == 94);
    Require(sceCesSJisCodeToJisX0208(0xF040, &ku, &ten) == invalidEncode);
    Require(sceCesSJisCodeToJisX0208(0x0041, &ku, &ten) == invalidEncode);
    Require(sceCesSJisCodeToJisX0208(0x827F, &ku, &ten) == invalidEncode);
    Require(sceCesSJisCodeToJisX0208(0x82A0, nullptr, &ten) == invalidDstBuffer);
    Require(sceCesJisX0208ToSJisCode(4, 2, &sjisCode) == 0 && sjisCode == 0x82A0);
    Require(sceCesJisX0208ToSJisCode(0, 2, &sjisCode) == invalidEncode);
    Require(sceCesJisX0208ToSJisCode(4, 95, &sjisCode) == invalidEncode);
    Require(sceCesJisX0208ToSJisCode(4, 2, nullptr) == invalidDstBuffer);
    for (int row = 1; row <= 94; ++row) {
        for (int cell = 1; cell <= 94; ++cell) {
            Require(sceCesJisX0208ToSJisCode(static_cast<std::uint8_t>(row), static_cast<std::uint8_t>(cell), &sjisCode) == 0);
            Require(sceCesSJisCodeToJisX0208(sjisCode, &ku, &ten) == 0 && ku == row && ten == cell);
        }
    }
    return 0;
}
