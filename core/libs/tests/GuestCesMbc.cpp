#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

static const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

static void RequireX0213(std::uint16_t sjis, std::uint8_t men, std::uint8_t ku, std::uint8_t ten) {
    std::uint8_t m = 0;
    std::uint8_t k = 0;
    std::uint8_t t = 0;
    Require(sceCesSJisCodeToJisX0213(sjis, &m, &k, &t) == 0 && m == men && k == ku && t == ten);
    std::uint16_t back = 0;
    Require(sceCesJisX0213ToSJisCode(men, ku, ten, &back) == 0 && back == sjis);
}

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
    alignas(8) std::uint8_t gbkSheet[64] = {};
    alignas(8) std::uint8_t blankSheet[64] = {};
    const void* sjis = sceCesUcsProfileInitSJis1997Cp932(sjisSheet);
    const void* euc = sceCesUcsProfileInitEucJpCp51932(eucSheet);
    const void* gbk = sceCesUcsProfileInitGbkCp936(gbkSheet);
    const void* cp1252 = sceCesRefersUcsProfileCp1252();

    std::uint32_t consumed = 0;
    std::uint32_t produced = 0;
    std::uint16_t ucs2 = 0;
    Require(sceCesMbcToUcs2(sjis, Bytes("\x82\xA0"), 2, &consumed, &ucs2) == 0 && consumed == 2 && ucs2 == 0x3042);
    Require(sceCesMbcToUcs2(sjis, Bytes("\xB1"), 1, &consumed, &ucs2) == 0 && consumed == 1 && ucs2 == 0xFF71);
    Require(sceCesMbcToUcs2(euc, Bytes("\xA4\xA2"), 2, &consumed, &ucs2) == 0 && consumed == 2 && ucs2 == 0x3042);
    Require(sceCesMbcToUcs2(gbk, Bytes("\xD6\xD0"), 2, &consumed, &ucs2) == 0 && consumed == 2 && ucs2 == 0x4E2D);
    Require(sceCesMbcToUcs2(cp1252, Bytes("\x80"), 1, &consumed, &ucs2) == 0 && consumed == 1 && ucs2 == 0x20AC);
    Require(sceCesMbcToUcs2(nullptr, Bytes("A"), 1, &consumed, &ucs2) == invalidParameter);
    Require(sceCesMbcToUcs2(blankSheet, Bytes("A"), 1, &consumed, &ucs2) == invalidParameter);
    Require(sceCesMbcToUcs2(sjis, nullptr, 1, &consumed, &ucs2) == invalidSrcBuffer);
    Require(sceCesMbcToUcs2(sjis, Bytes("A"), 0, &consumed, &ucs2) == srcBufferEnd);
    Require(sceCesMbcToUcs2(sjis, Bytes("A"), 1, &consumed, nullptr) == invalidDstBuffer);
    Require(sceCesMbcToUcs2(sjis, Bytes("\x82"), 1, &consumed, &ucs2) == srcBufferEnd);
    Require(sceCesMbcToUcs2(sjis, Bytes("\x80"), 1, &consumed, &ucs2) == invalidEncode);
    Require(sceCesMbcToUcs2(sjis, Bytes("\xF0\x40"), 2, &consumed, &ucs2) == unassignedCode);

    std::uint8_t utf8[4] = {};
    Require(sceCesMbcToUtf8(gbk, Bytes("\xD6\xD0"), 2, &consumed, utf8, sizeof(utf8), &produced) == 0 && consumed == 2 && produced == 3);
    Require(std::memcmp(utf8, "\xE4\xB8\xAD", 3) == 0);
    Require(sceCesMbcToUtf8(gbk, Bytes("\xD6\xD0"), 2, &consumed, utf8, 2, &produced) == dstBufferEnd);

    std::uint16_t utf16 = 0;
    Require(sceCesMbcToUtf16(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 1, &produced) == 0 && produced == 1 && utf16 == 0x3042);
    Require(sceCesMbcToUtf16le(euc, Bytes("\xA4\xA2"), 2, &consumed, &utf16, 1, &produced) == 0 && std::memcmp(&utf16, "\x42\x30", 2) == 0);
    Require(sceCesMbcToUtf16be(euc, Bytes("\xA4\xA2"), 2, &consumed, &utf16, 1, &produced) == 0 && std::memcmp(&utf16, "\x30\x42", 2) == 0);
    Require(sceCesMbcToUtf16(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 0, &produced) == dstBufferEnd);

    std::uint32_t utf32 = 0;
    Require(sceCesMbcToUtf32(gbk, Bytes("A"), 1, &consumed, &utf32) == 0 && consumed == 1 && utf32 == 'A');
    Require(sceCesMbcToUtf32le(gbk, Bytes("\xD6\xD0"), 2, &consumed, &utf32) == 0 && std::memcmp(&utf32, "\x2D\x4E\x00\x00", 4) == 0);
    Require(sceCesMbcToUtf32be(gbk, Bytes("\xD6\xD0"), 2, &consumed, &utf32) == 0 && std::memcmp(&utf32, "\x00\x00\x4E\x2D", 4) == 0);

    std::uint16_t code = 0;
    std::uint32_t size = 0;
    Require(sceCesSJisGetCode(Bytes("\x82\xA0"), 2, &size, &code) == 0 && size == 2 && code == 0x82A0);
    Require(sceCesSJisGetCode(Bytes("A\x82"), 2, &size, &code) == 0 && size == 1 && code == 'A');
    Require(sceCesSJisGetCode(Bytes("\xDF"), 1, &size, &code) == 0 && size == 1 && code == 0xDF);
    Require(sceCesSJisGetCode(Bytes("\xFC\xFC"), 2, nullptr, &code) == 0 && code == 0xFCFC);
    Require(sceCesSJisGetCode(Bytes("\x82"), 1, &size, &code) == srcBufferEnd);
    Require(sceCesSJisGetCode(Bytes("\x80"), 1, &size, &code) == invalidEncode);
    Require(sceCesSJisGetCode(Bytes("\xA0"), 1, &size, &code) == invalidEncode);
    Require(sceCesSJisGetCode(Bytes("\xFD\x40"), 2, &size, &code) == invalidEncode);
    Require(sceCesSJisGetCode(Bytes("\x82\x7F"), 2, &size, &code) == invalidEncode);
    Require(sceCesSJisGetCode(nullptr, 2, &size, &code) == invalidSrcBuffer);
    Require(sceCesSJisGetCode(Bytes("A"), 0, &size, &code) == srcBufferEnd);
    Require(sceCesSJisGetCode(Bytes("A"), 1, &size, nullptr) == invalidDstBuffer);

    std::uint8_t sjisOut[2] = {};
    Require(sceCesSJisPutCode(0x82A0, sjisOut, 2, &size) == 0 && size == 2 && sjisOut[0] == 0x82 && sjisOut[1] == 0xA0);
    Require(sceCesSJisPutCode('A', sjisOut, 1, &size) == 0 && size == 1 && sjisOut[0] == 'A');
    Require(sceCesSJisPutCode(0xB1, sjisOut, 1, nullptr) == 0 && sjisOut[0] == 0xB1);
    Require(sceCesSJisPutCode(0x82A0, sjisOut, 1, &size) == dstBufferEnd);
    Require(sceCesSJisPutCode('A', sjisOut, 0, &size) == dstBufferEnd);
    Require(sceCesSJisPutCode(0x80, sjisOut, 2, &size) == invalidEncode);
    Require(sceCesSJisPutCode(0x0141, sjisOut, 2, &size) == invalidEncode);
    Require(sceCesSJisPutCode(0x827F, sjisOut, 2, &size) == invalidEncode);
    Require(sceCesSJisPutCode(0x82A0, nullptr, 2, &size) == invalidDstBuffer);

    RequireX0213(0x8140, 1, 1, 1);
    RequireX0213(0x889F, 1, 16, 1);
    RequireX0213(0xEFFC, 1, 94, 94);
    RequireX0213(0xF040, 2, 1, 1);
    RequireX0213(0xF09F, 2, 8, 1);
    RequireX0213(0xF140, 2, 3, 1);
    RequireX0213(0xF19F, 2, 4, 1);
    RequireX0213(0xF240, 2, 5, 1);
    RequireX0213(0xF29F, 2, 12, 1);
    RequireX0213(0xF340, 2, 13, 1);
    RequireX0213(0xF39F, 2, 14, 1);
    RequireX0213(0xF440, 2, 15, 1);
    RequireX0213(0xF49F, 2, 78, 1);
    RequireX0213(0xF540, 2, 79, 1);
    RequireX0213(0xFCFC, 2, 94, 94);
    std::uint8_t men = 0;
    std::uint8_t ku = 0;
    std::uint8_t ten = 0;
    Require(sceCesSJisCodeToJisX0213(0x8040, &men, &ku, &ten) == invalidEncode);
    Require(sceCesSJisCodeToJisX0213(0xFD40, &men, &ku, &ten) == invalidEncode);
    Require(sceCesSJisCodeToJisX0213(0x827F, &men, &ku, &ten) == invalidEncode);
    Require(sceCesSJisCodeToJisX0213(0x82A0, nullptr, &ku, &ten) == invalidDstBuffer);
    Require(sceCesJisX0213ToSJisCode(2, 2, 1, &code) == invalidEncode);
    Require(sceCesJisX0213ToSJisCode(2, 77, 1, &code) == invalidEncode);
    Require(sceCesJisX0213ToSJisCode(3, 1, 1, &code) == invalidEncode);
    Require(sceCesJisX0213ToSJisCode(0, 1, 1, &code) == invalidEncode);
    Require(sceCesJisX0213ToSJisCode(1, 95, 1, &code) == invalidEncode);
    Require(sceCesJisX0213ToSJisCode(1, 1, 0, &code) == invalidEncode);
    Require(sceCesJisX0213ToSJisCode(1, 1, 1, nullptr) == invalidDstBuffer);

    alignas(8) std::uint8_t context[64] = {};
    alignas(8) std::uint8_t copy[64] = {};
    Require(sceCesMbcsUcsContextInit(context, gbk) == 0);
    Require(sceCesMbcsUcsContextInitCopy(copy, context) == 0);
    Require(sceCesMbcsStrToUtf8Str(copy, Bytes("\xD6\xD0"), 2, &consumed, utf8, sizeof(utf8), &produced) == 0 && consumed == 2 && produced == 3);
    Require(std::memcmp(utf8, "\xE4\xB8\xAD", 3) == 0);
    Require(sceCesMbcsUcsContextInitCopy(nullptr, context) == invalidParameter);
    Require(sceCesMbcsUcsContextInitCopy(copy, nullptr) == invalidParameter);
}
