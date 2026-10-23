#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
void* APS5_VABI sceCesUcsProfileInitEucCnGb2312(void*);
void* APS5_VABI sceCesUcsProfileInitGbkCp936(void*);
void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void*);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
int APS5_VABI sceCesEucCnToUcs2(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
int APS5_VABI sceCesEucCnToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucCnToUtf16be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucCnToUtf32(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesGbToUcs2(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
int APS5_VABI sceCesGbToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesGbToUtf16(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesGbToUtf32le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUcs2ToEucCn(std::uint16_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUcs2ToGb(std::uint16_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToEucCn(const std::uint8_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToGb(const std::uint8_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16leToGb(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32ToEucCn(std::uint32_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32beToGb(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8StrToMbcsStr(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
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

    alignas(8) std::uint8_t eucSheet[64] = {};
    alignas(8) std::uint8_t gbkSheet[64] = {};
    alignas(8) std::uint8_t sjisSheet[64] = {};
    const void* euc = sceCesUcsProfileInitEucCnGb2312(eucSheet);
    const void* gbk = sceCesUcsProfileInitGbkCp936(gbkSheet);
    const void* sjis = sceCesUcsProfileInitSJis1997Cp932(sjisSheet);
    Require(euc == eucSheet && gbk == gbkSheet);
    Require(sceCesUcsProfileInitEucCnGb2312(nullptr) == nullptr && sceCesUcsProfileInitGbkCp936(nullptr) == nullptr);

    std::uint32_t consumed = 0;
    std::uint32_t produced = 0;
    std::uint16_t ucs2 = 0;
    Require(sceCesEucCnToUcs2(euc, Bytes("\xD6\xD0"), 2, &consumed, &ucs2) == 0 && consumed == 2 && ucs2 == 0x4E2D);
    Require(sceCesEucCnToUcs2(euc, Bytes("A"), 1, &consumed, &ucs2) == 0 && consumed == 1 && ucs2 == 'A');
    Require(sceCesEucCnToUcs2(euc, Bytes("\xA1\xA4"), 2, &consumed, &ucs2) == 0 && ucs2 == 0x00B7);
    Require(sceCesEucCnToUcs2(euc, Bytes("\x80"), 1, &consumed, &ucs2) == invalidEncode);
    Require(sceCesEucCnToUcs2(euc, Bytes("\x81\x40"), 2, &consumed, &ucs2) == invalidEncode);
    Require(sceCesEucCnToUcs2(euc, Bytes("\xD6\x40"), 2, &consumed, &ucs2) == invalidEncode);
    Require(sceCesEucCnToUcs2(euc, Bytes("\xD6\xD0"), 1, &consumed, &ucs2) == srcBufferEnd);
    Require(sceCesEucCnToUcs2(euc, Bytes("\xAA\xA1"), 2, &consumed, &ucs2) == unassignedCode);
    Require(sceCesEucCnToUcs2(gbk, Bytes("\xD6\xD0"), 2, &consumed, &ucs2) == invalidParameter);
    Require(sceCesEucCnToUcs2(euc, nullptr, 2, &consumed, &ucs2) == invalidSrcBuffer);
    Require(sceCesEucCnToUcs2(euc, Bytes("\xD6\xD0"), 2, &consumed, nullptr) == invalidDstBuffer);

    std::uint8_t utf8[4] = {};
    Require(sceCesEucCnToUtf8(euc, Bytes("\xCE\xC4"), 2, &consumed, utf8, sizeof(utf8), &produced) == 0);
    Require(produced == 3 && std::memcmp(utf8, "\xE6\x96\x87", 3) == 0);
    std::uint16_t utf16 = 0;
    Require(sceCesEucCnToUtf16be(euc, Bytes("\xD6\xD0"), 2, &consumed, &utf16, 1, &produced) == 0 && std::memcmp(&utf16, "\x4E\x2D", 2) == 0);
    std::uint32_t utf32 = 0;
    Require(sceCesEucCnToUtf32(euc, Bytes("\xD6\xD0"), 2, &consumed, &utf32) == 0 && utf32 == 0x4E2D);

    Require(sceCesGbToUcs2(gbk, Bytes("\x80"), 1, &consumed, &ucs2) == 0 && consumed == 1 && ucs2 == 0x20AC);
    Require(sceCesGbToUcs2(gbk, Bytes("\x81\x40"), 2, &consumed, &ucs2) == 0 && consumed == 2 && ucs2 == 0x4E02);
    Require(sceCesGbToUcs2(gbk, Bytes("\xD6\xD0"), 2, &consumed, &ucs2) == 0 && ucs2 == 0x4E2D);
    Require(sceCesGbToUcs2(gbk, Bytes("\xFF"), 1, &consumed, &ucs2) == invalidEncode);
    Require(sceCesGbToUcs2(gbk, Bytes("\x81\x7F"), 2, &consumed, &ucs2) == invalidEncode);
    Require(sceCesGbToUcs2(gbk, Bytes("\x81\x3F"), 2, &consumed, &ucs2) == invalidEncode);
    Require(sceCesGbToUcs2(euc, Bytes("\x81\x40"), 2, &consumed, &ucs2) == invalidParameter);
    Require(sceCesGbToUcs2(sjis, Bytes("A"), 1, &consumed, &ucs2) == invalidParameter);
    Require(sceCesGbToUtf8(gbk, Bytes("\x80"), 1, &consumed, utf8, sizeof(utf8), &produced) == 0);
    Require(produced == 3 && std::memcmp(utf8, "\xE2\x82\xAC", 3) == 0);
    Require(sceCesGbToUtf8(gbk, Bytes("\x80"), 1, &consumed, utf8, 2, &produced) == dstBufferEnd);
    Require(sceCesGbToUtf16(gbk, Bytes("\x81\x40"), 2, &consumed, &utf16, 1, &produced) == 0 && utf16 == 0x4E02 && produced == 1);
    Require(sceCesGbToUtf32le(gbk, Bytes("\x81\x40"), 2, &consumed, &utf32) == 0 && std::memcmp(&utf32, "\x02\x4E\x00\x00", 4) == 0);

    std::uint8_t mbcs[4] = {};
    Require(sceCesUcs2ToEucCn(0x4E2D, euc, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && std::memcmp(mbcs, "\xD6\xD0", 2) == 0);
    Require(sceCesUcs2ToEucCn(0x4E02, euc, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUcs2ToEucCn(0x20AC, euc, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUcs2ToEucCn(0x4E2D, euc, mbcs, 1, &produced) == dstBufferEnd);
    Require(sceCesUcs2ToEucCn(0xDC00, euc, mbcs, sizeof(mbcs), &produced) == invalidEncode);
    Require(sceCesUcs2ToGb(0x4E02, gbk, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && std::memcmp(mbcs, "\x81\x40", 2) == 0);
    Require(sceCesUcs2ToGb(0x20AC, gbk, mbcs, sizeof(mbcs), &produced) == 0 && produced == 1 && mbcs[0] == 0x80);
    Require(sceCesUcs2ToGb(0x3042, euc, mbcs, sizeof(mbcs), &produced) == invalidParameter);
    Require(sceCesUcs2ToGb(0x3042, gbk, nullptr, sizeof(mbcs), &produced) == invalidDstBuffer);
    Require(sceCesUtf8ToEucCn(Bytes("\xE6\x96\x87"), 3, &consumed, euc, mbcs, sizeof(mbcs), &produced) == 0);
    Require(consumed == 3 && std::memcmp(mbcs, "\xCE\xC4", 2) == 0);
    Require(sceCesUtf8ToGb(Bytes("\xF0\x9F\x98\x80"), 4, &consumed, gbk, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    std::uint16_t zhongLe = 0;
    std::memcpy(&zhongLe, "\x2D\x4E", 2);
    Require(sceCesUtf16leToGb(&zhongLe, 1, &consumed, gbk, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\xD6\xD0", 2) == 0);
    Require(sceCesUtf32ToEucCn(0x110000, euc, mbcs, sizeof(mbcs), &produced) == invalidEncode);
    std::uint32_t zhongBe = 0;
    std::memcpy(&zhongBe, "\x00\x00\x4E\x2D", 4);
    Require(sceCesUtf32beToGb(&zhongBe, gbk, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\xD6\xD0", 2) == 0);

    int gbkCount = 0;
    int eucCount = 0;
    for (int code = 1; code < 0x10000; ++code) {
        if (code >= 0xD800 && code <= 0xDFFF) continue;
        std::uint8_t encoded[4] = {};
        std::uint32_t size = 0;
        std::uint16_t back = 0;
        if (sceCesUcs2ToGb(static_cast<std::uint16_t>(code), gbk, encoded, sizeof(encoded), &size) == 0) {
            Require(sceCesGbToUcs2(gbk, encoded, size, &consumed, &back) == 0 && consumed == size && back == code);
            ++gbkCount;
        }
        if (sceCesUcs2ToEucCn(static_cast<std::uint16_t>(code), euc, encoded, sizeof(encoded), &size) == 0) {
            Require(size == 1 || (encoded[0] >= 0xA1 && encoded[1] >= 0xA1));
            Require(sceCesEucCnToUcs2(euc, encoded, size, &consumed, &back) == 0 && consumed == size && back == code);
            ++eucCount;
        }
    }
    Require(gbkCount == 127 + 1 + 21791 && eucCount > 127 + 7000 && eucCount < gbkCount);

    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesMbcsUcsContextInit(context, gbk) == 0);
    std::uint8_t text[16] = {};
    Require(sceCesUtf8StrToMbcsStr(context, Bytes("A\xE4\xB8\xAD\xE2\x82\xAC"), 16, &consumed, text, sizeof(text), &produced) == 0);
    Require(consumed == 7 && produced == 4 && std::memcmp(text, "A\xD6\xD0\x80", 4) == 0);
    std::uint8_t back[16] = {};
    Require(sceCesMbcsStrToUtf8Str(context, text, produced, &consumed, back, sizeof(back), &produced) == 0);
    Require(consumed == 4 && produced == 7 && std::memcmp(back, "A\xE4\xB8\xAD\xE2\x82\xAC", 7) == 0);
    return 0;
}
