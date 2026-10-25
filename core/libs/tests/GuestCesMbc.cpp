#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>

constexpr int invalidParameter = static_cast<int>(0x805C0001);
constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
constexpr int invalidEncode = static_cast<int>(0x805C0014);
constexpr int outOfCodeSpace = static_cast<int>(0x805C0015);
constexpr int unassignedCode = static_cast<int>(0x805C0020);
constexpr int unsupported = static_cast<int>(0x805C0024);
constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
constexpr int dstBufferEnd = static_cast<int>(0x805C0031);
constexpr std::uint32_t untouched = 0xEEEEEEEE;

enum Form { Ucs2, Utf8, Utf16, Utf16be, Utf16le, Utf32, Utf32be, Utf32le, FormCount };

struct Sequence {
    std::uint32_t count;
    std::uint32_t units[4];
    int expected;
};

static void Require(bool value) { if (!value) std::abort(); }

static const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

static bool IsBuffer(int form) { return form == Utf8 || form == Utf16 || form == Utf16be || form == Utf16le; }

static std::uint32_t Units(int form, std::uint32_t code, std::uint32_t* units) {
    constexpr std::uint32_t leads[5] = {0, 0, 0xC0, 0xE0, 0xF0};
    if (form == Utf8 && code >= 0x80) {
        const std::uint32_t count = code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
        for (std::uint32_t i = count - 1; i > 0; --i, code >>= 6) units[i] = 0x80 | (code & 0x3F);
        units[0] = leads[count] | code;
        return count;
    }
    if (IsBuffer(form) && code >= 0x10000) {
        units[0] = 0xD800 + ((code - 0x10000) >> 10);
        units[1] = 0xDC00 + ((code - 0x10000) & 0x3FF);
        return 2;
    }
    units[0] = code;
    return 1;
}

static int Call(int form, const std::uint32_t* units, std::uint32_t count, const void* profile, std::uint8_t* mbc, std::uint32_t mbcMax, std::uint32_t* mbcLen, std::uint32_t* sourceLen) {
    const bool bigEndian = form == Utf16be || form == Utf32be || ((form == Utf16 || form == Utf32) && std::endian::native == std::endian::big);
    const std::uint32_t width = form == Utf8 ? 1 : form >= Utf32 ? 4 : 2;
    alignas(4) std::uint8_t bytes[16] = {};
    for (std::uint32_t i = 0; units && i < count; ++i) {
        for (std::uint32_t b = 0; b < width; ++b) bytes[i * width + b] = static_cast<std::uint8_t>(units[i] >> (8 * (bigEndian ? width - 1 - b : b)));
    }
    const std::uint8_t* utf8 = units ? bytes : nullptr;
    const auto* utf16 = reinterpret_cast<const std::uint16_t*>(utf8);
    const auto* utf32 = reinterpret_cast<const std::uint32_t*>(utf8);
    const std::uint32_t value = units ? units[0] : 0;
    switch (form) {
    case Ucs2: return sceCesUcs2ToMbc(static_cast<std::uint16_t>(value), profile, mbc, mbcMax, mbcLen);
    case Utf8: return sceCesUtf8ToMbc(utf8, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf16: return sceCesUtf16ToMbc(utf16, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf16be: return sceCesUtf16beToMbc(utf16, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf16le: return sceCesUtf16leToMbc(utf16, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf32: return sceCesUtf32ToMbc(value, profile, mbc, mbcMax, mbcLen);
    case Utf32be: return sceCesUtf32beToMbc(utf32, profile, mbc, mbcMax, mbcLen);
    default: return sceCesUtf32leToMbc(utf32, profile, mbc, mbcMax, mbcLen);
    }
}

static void RequireFailure(int form, const std::uint32_t* units, std::uint32_t count, const void* profile, bool target, std::uint32_t mbcMax, int expected) {
    std::uint8_t mbc[4] = {0xEE, 0xEE, 0xEE, 0xEE};
    std::uint32_t mbcLen = untouched;
    std::uint32_t sourceLen = untouched;
    Require(Call(form, units, count, profile, target ? mbc : nullptr, mbcMax, &mbcLen, &sourceLen) == expected);
    const bool refused = !IsBuffer(form) && expected != invalidParameter && expected != invalidSrcBuffer && expected != invalidDstBuffer;
    Require(mbc[0] == 0xEE && mbc[1] == 0xEE && mbc[2] == 0xEE && mbc[3] == 0xEE && mbcLen == (refused ? 0 : untouched) && sourceLen == untouched);
}

static void RequireAll(std::uint32_t code, const void* profile, int expected, const std::uint8_t* bytes, std::uint32_t size) {
    for (int form = code > 0xFFFF ? Utf8 : Ucs2; form < FormCount; ++form) {
        std::uint32_t units[4] = {};
        const std::uint32_t count = Units(form, code, units);
        if (expected != 0) {
            RequireFailure(form, units, count, profile, true, 4, expected);
            continue;
        }
        std::uint8_t mbc[4] = {0xEE, 0xEE, 0xEE, 0xEE};
        std::uint32_t mbcLen = untouched;
        std::uint32_t sourceLen = untouched;
        Require(Call(form, units, count, profile, mbc, sizeof(mbc), &mbcLen, &sourceLen) == 0);
        Require(mbcLen == size && std::memcmp(mbc, bytes, size) == 0 && sourceLen == (IsBuffer(form) ? count : untouched));
    }
}

static std::uint32_t SweepCodes(const void* profile) {
    constexpr std::uint32_t supplementary[] = {0x10000, 0x1F600, 0x10FFFF};
    std::uint32_t mapped = 0;
    for (std::uint32_t code = 0; code < 0x10000; ++code) {
        if (code >= 0xD800 && code <= 0xDFFF) continue;
        std::uint8_t bytes[4] = {};
        std::uint32_t size = 0;
        const int result = sceCesUtf32ToMbc(code, profile, bytes, sizeof(bytes), &size);
        Require(result == 0 || result == unassignedCode || result == unsupported);
        if (result == 0 && size > 0) {
            std::uint32_t consumed = 0;
            std::uint32_t back = 0;
            Require(sceCesMbcToUtf32(profile, bytes, size, &consumed, &back) == 0 && consumed == size);
            if (back == code) ++mapped;
        }
        RequireAll(code, profile, result, bytes, size);
    }
    for (const std::uint32_t code : supplementary) RequireAll(code, profile, unassignedCode, nullptr, 0);
    return mapped;
}

static std::uint32_t RoundTrip(const void* profile, const std::uint8_t* source, std::uint32_t size) {
    std::uint32_t consumed = 0;
    std::uint32_t produced = 0;
    std::uint32_t code = 0;
    std::uint16_t ucs2 = 0;
    std::uint8_t utf8[4] = {};
    std::uint32_t utf8Size = 0;
    std::uint16_t utf16[3] = {};
    std::uint32_t utf32[2] = {};
    Require(sceCesMbcToUtf32(profile, source, size, &consumed, &code) == 0 && consumed == size);
    Require(sceCesMbcToUcs2(profile, source, size, &consumed, &ucs2) == 0 && consumed == size);
    Require(sceCesMbcToUtf8(profile, source, size, &consumed, utf8, sizeof(utf8), &utf8Size) == 0 && consumed == size);
    Require(sceCesMbcToUtf16(profile, source, size, &consumed, &utf16[0], 1, &produced) == 0 && consumed == size);
    Require(sceCesMbcToUtf16be(profile, source, size, &consumed, &utf16[1], 1, &produced) == 0 && consumed == size);
    Require(sceCesMbcToUtf16le(profile, source, size, &consumed, &utf16[2], 1, &produced) == 0 && consumed == size);
    Require(sceCesMbcToUtf32be(profile, source, size, &consumed, &utf32[0]) == 0 && consumed == size);
    Require(sceCesMbcToUtf32le(profile, source, size, &consumed, &utf32[1]) == 0 && consumed == size);
    std::uint8_t mbc[8][4] = {};
    std::uint32_t mbcLen[8] = {};
    std::uint32_t sourceLen[4] = {};
    Require(sceCesUtf32ToMbc(code, profile, mbc[0], 4, &mbcLen[0]) == 0);
    Require(sceCesUcs2ToMbc(ucs2, profile, mbc[1], 4, &mbcLen[1]) == 0);
    Require(sceCesUtf8ToMbc(utf8, utf8Size, &sourceLen[0], profile, mbc[2], 4, &mbcLen[2]) == 0 && sourceLen[0] == utf8Size);
    Require(sceCesUtf16ToMbc(&utf16[0], 1, &sourceLen[1], profile, mbc[3], 4, &mbcLen[3]) == 0 && sourceLen[1] == 1);
    Require(sceCesUtf16beToMbc(&utf16[1], 1, &sourceLen[2], profile, mbc[4], 4, &mbcLen[4]) == 0 && sourceLen[2] == 1);
    Require(sceCesUtf16leToMbc(&utf16[2], 1, &sourceLen[3], profile, mbc[5], 4, &mbcLen[5]) == 0 && sourceLen[3] == 1);
    Require(sceCesUtf32beToMbc(&utf32[0], profile, mbc[6], 4, &mbcLen[6]) == 0);
    Require(sceCesUtf32leToMbc(&utf32[1], profile, mbc[7], 4, &mbcLen[7]) == 0);
    for (std::uint32_t i = 1; i < 8; ++i) Require(mbcLen[i] == mbcLen[0] && std::memcmp(mbc[i], mbc[0], mbcLen[0]) == 0);
    return mbcLen[0] == size && std::memcmp(mbc[0], source, size) == 0 ? 1 : 0;
}

static std::uint32_t SweepBytes(const void* profile) {
    std::uint32_t identical = 0;
    std::uint8_t source[3] = {};
    std::uint32_t consumed = 0;
    std::uint32_t code = 0;
    for (std::uint32_t lead = 0; lead < 256; ++lead) {
        source[0] = static_cast<std::uint8_t>(lead);
        const int single = sceCesMbcToUtf32(profile, source, 1, &consumed, &code);
        if (single == 0) identical += RoundTrip(profile, source, 1);
        if (single != srcBufferEnd) continue;
        for (std::uint32_t trail = 0; trail < 256; ++trail) {
            source[1] = static_cast<std::uint8_t>(trail);
            const int pair = sceCesMbcToUtf32(profile, source, 2, &consumed, &code);
            if (pair == 0) identical += RoundTrip(profile, source, 2);
            for (std::uint32_t third = 0; pair == srcBufferEnd && third < 256; ++third) {
                source[2] = static_cast<std::uint8_t>(third);
                if (sceCesMbcToUtf32(profile, source, 3, &consumed, &code) == 0) identical += RoundTrip(profile, source, 3);
            }
        }
    }
    return identical;
}

static void RequireInvalid(const void* profile) {
    constexpr int scalarForms[] = {Ucs2, Utf32, Utf32be, Utf32le};
    const Sequence utf8[] = {
        {2, {0xC0, 0x80}, outOfCodeSpace}, {2, {0xC1, 0xBF}, outOfCodeSpace}, {3, {0xE0, 0x80, 0x80}, outOfCodeSpace},
        {3, {0xE0, 0x9F, 0xBF}, outOfCodeSpace}, {4, {0xF0, 0x80, 0x80, 0x80}, outOfCodeSpace}, {4, {0xF0, 0x8F, 0xBF, 0xBF}, outOfCodeSpace},
        {3, {0xED, 0xA0, 0x80}, outOfCodeSpace}, {3, {0xED, 0xBF, 0xBF}, outOfCodeSpace}, {4, {0xF4, 0x90, 0x80, 0x80}, unassignedCode},
        {4, {0xF7, 0xBF, 0xBF, 0xBF}, unassignedCode}, {1, {0x80}, invalidEncode}, {1, {0xFF}, invalidEncode},
        {3, {0xE3, 0x41, 0x42}, invalidEncode}, {2, {0xE3, 0x81}, srcBufferEnd}, {3, {0xF0, 0x9F, 0x98}, srcBufferEnd},
    };
    const Sequence utf16[] = {
        {1, {0xDC00}, invalidEncode}, {1, {0xDFFF}, invalidEncode}, {2, {0xD800, 0x0041}, invalidEncode},
        {2, {0xDBFF, 0xDBFF}, invalidEncode}, {2, {0xDC00, 0xD800}, invalidEncode}, {1, {0xD800}, srcBufferEnd},
    };
    const Sequence utf32[] = {{1, {0xD800}, outOfCodeSpace}, {1, {0xDFFF}, outOfCodeSpace}, {1, {0x110000}, invalidEncode}, {1, {0xFFFFFFFF}, invalidEncode}};
    for (const Sequence& sequence : utf8) RequireFailure(Utf8, sequence.units, sequence.count, profile, true, 4, sequence.expected);
    for (const Sequence& sequence : utf16) {
        for (int form = Utf16; form <= Utf16le; ++form) RequireFailure(form, sequence.units, sequence.count, profile, true, 4, sequence.expected);
    }
    for (const Sequence& sequence : utf32) {
        for (const int form : scalarForms) {
            if (form != Ucs2 || sequence.units[0] <= 0xFFFF) RequireFailure(form, sequence.units, sequence.count, profile, true, 4, sequence.expected);
        }
    }
}

static void RequireX0213(std::uint16_t sjis, std::uint8_t men, std::uint8_t ku, std::uint8_t ten) {
    std::uint8_t m = 0;
    std::uint8_t k = 0;
    std::uint8_t t = 0;
    Require(sceCesSJisCodeToJisX0213(sjis, &m, &k, &t) == 0 && m == men && k == ku && t == ten);
    std::uint16_t back = 0;
    Require(sceCesJisX0213ToSJisCode(men, ku, ten, &back) == 0 && back == sjis);
}

int main() {
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

    alignas(8) std::uint8_t eucCnSheet[64] = {};
    alignas(8) std::uint8_t eucKrSheet[64] = {};
    alignas(8) std::uint8_t uhcSheet[64] = {};
    alignas(8) std::uint8_t big5Sheet[64] = {};
    const void* eucCn = sceCesUcsProfileInitEucCnGb2312(eucCnSheet);
    const void* eucKr = sceCesUcsProfileInitEucKr(eucKrSheet);
    const void* uhc = sceCesUcsProfileInitUhc(uhcSheet);
    const void* big5 = sceCesUcsProfileInitBig5Cp950(big5Sheet);
    const void* cp1251 = sceCesRefersUcsProfileCp1251();
    const void* cp1250 = sceCesRefersUcsProfileCp1250();

    RequireAll(0x20AC, cp1252, 0, Bytes("\x80"), 1);
    RequireAll(0x00E9, cp1252, 0, Bytes("\xE9"), 1);
    RequireAll(0x0100, cp1252, unassignedCode, nullptr, 0);
    RequireAll(0x0416, cp1251, 0, Bytes("\xC6"), 1);
    RequireAll(0x0141, cp1250, 0, Bytes("\xA3"), 1);
    RequireAll(0x3042, sjis, 0, Bytes("\x82\xA0"), 2);
    RequireAll(0xFF71, sjis, 0, Bytes("\xB1"), 1);
    RequireAll(0x4E02, sjis, unassignedCode, nullptr, 0);
    RequireAll(0x3042, euc, 0, Bytes("\xA4\xA2"), 2);
    RequireAll(0xFF71, euc, 0, Bytes("\x8E\xB1"), 2);
    RequireAll(0x4E02, euc, unassignedCode, nullptr, 0);
    RequireAll(0x4E2D, gbk, 0, Bytes("\xD6\xD0"), 2);
    RequireAll(0x4E02, gbk, 0, Bytes("\x81\x40"), 2);
    RequireAll(0x20AC, gbk, 0, Bytes("\x80"), 1);
    RequireAll(0x4E2D, eucCn, 0, Bytes("\xD6\xD0"), 2);
    RequireAll(0x4E02, eucCn, unassignedCode, nullptr, 0);
    RequireAll(0xAC00, eucKr, 0, Bytes("\xB0\xA1"), 2);
    RequireAll(0xAC02, eucKr, unassignedCode, nullptr, 0);
    RequireAll(0xAC02, uhc, 0, Bytes("\x81\x41"), 2);
    RequireAll(0x4E00, big5, 0, Bytes("\xA4\x40"), 2);

    std::uint32_t units[4] = {};
    for (int form = Ucs2; form < FormCount; ++form) {
        std::uint32_t count = Units(form, 0xDC00, units);
        RequireFailure(form, nullptr, 0, nullptr, false, 0, invalidParameter);
        RequireFailure(form, units, count, blankSheet, true, 4, invalidParameter);
        if (form != Ucs2 && form != Utf32) RequireFailure(form, nullptr, 0, sjis, false, 0, invalidSrcBuffer);
        if (IsBuffer(form)) RequireFailure(form, units, 0, sjis, false, 0, srcBufferEnd);
        RequireFailure(form, units, count, sjis, false, 0, invalidDstBuffer);
        RequireFailure(form, units, count, sjis, true, 0, IsBuffer(form) && form != Utf8 ? invalidEncode : outOfCodeSpace);
        count = Units(form, 0x4E02, units);
        RequireFailure(form, units, count, sjis, true, 0, unassignedCode);
        count = Units(form, 0x3042, units);
        std::uint8_t full[4] = {0xEE, 0xEE, 0xEE, 0xEE};
        std::uint32_t needed = untouched;
        std::uint32_t sourceLen = untouched;
        Require(Call(form, units, count, sjis, full, 1, &needed, &sourceLen) == dstBufferEnd && needed == (IsBuffer(form) ? untouched : 2) && full[0] == 0xEE && sourceLen == untouched);
        std::uint8_t mbc[4] = {};
        Require(Call(form, units, count, sjis, mbc, sizeof(mbc), nullptr, nullptr) == 0 && std::memcmp(mbc, "\x82\xA0", 2) == 0);
    }

    std::uint8_t mbc[4] = {};
    std::uint32_t wide = 0;
    std::memcpy(&wide, "\x00\x00\x30\x42", 4);
    Require(sceCesUtf32beToMbc(&wide, sjis, mbc, sizeof(mbc), &produced) == 0 && produced == 2 && std::memcmp(mbc, "\x82\xA0", 2) == 0);
    Require(sceCesUtf32leToMbc(&wide, sjis, mbc, sizeof(mbc), &produced) == invalidEncode);
    std::uint16_t narrow = 0;
    std::memcpy(&narrow, "\xD8\x00", 2);
    Require(sceCesUtf16leToMbc(&narrow, 1, &consumed, cp1252, mbc, sizeof(mbc), &produced) == 0 && consumed == 1 && produced == 1 && mbc[0] == 0xD8);
    Require(sceCesUtf16beToMbc(&narrow, 1, &consumed, cp1252, mbc, sizeof(mbc), &produced) == srcBufferEnd);
    Require(sceCesUtf8ToMbc(Bytes("A\xE4\xB8\xAD"), 4, &consumed, gbk, mbc, sizeof(mbc), &produced) == 0 && consumed == 1 && produced == 1 && mbc[0] == 'A');

    const void* singleByte[] = {cp1252, cp1251, cp1250};
    constexpr std::uint32_t singleByteMapped[] = {251, 255, 251};
    const void* multiByte[] = {sjis, euc, eucCn, gbk, eucKr, uhc, big5};
    for (std::size_t index = 0; index < 3; ++index) {
        Require(SweepCodes(singleByte[index]) == singleByteMapped[index] && SweepBytes(singleByte[index]) == singleByteMapped[index]);
        RequireInvalid(singleByte[index]);
    }
    for (const void* profile : multiByte) {
        const std::uint32_t mapped = SweepCodes(profile);
        Require(mapped > 256 && SweepBytes(profile) == mapped);
        RequireInvalid(profile);
    }
}
