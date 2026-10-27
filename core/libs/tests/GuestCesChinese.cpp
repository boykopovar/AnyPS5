#include <cstdio>
#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <initializer_list>
#include <cstdlib>
#include <cstring>

static void RequireImpl(bool value, int line) { if (!value) { std::fprintf(stderr, "REQFAIL line %d\n", line); std::fflush(stderr); std::abort(); } }
#define Require(v) RequireImpl((v), __LINE__)

static const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

static constexpr int invalidParameter = static_cast<int>(0x805C0001);
static constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
static constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
static constexpr int invalidEncode = static_cast<int>(0x805C0014);
 constexpr int outOfCodeSpace = static_cast<int>(0x805C0015);
static constexpr int unassignedCode = static_cast<int>(0x805C0020);
static constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
static constexpr int dstBufferEnd = static_cast<int>(0x805C0031);

struct Gb18030Sequence {
    std::uint32_t bytes;
    std::uint32_t code2000;
    std::uint32_t code2005;
};

struct Gb18030Difference {
    std::uint32_t bytes;
    std::uint32_t code2000;
    std::uint32_t code2005;
    bool fallback;
};

static constexpr Gb18030Sequence GB18030_BOUNDARIES[88] = {
    {0x81308130, 0x80, 0x80}, {0x8130D239, 0x450, 0x450}, {0x8130D330, 0x452, 0x452}, {0x8135F436, 0x1E3E, 0x1E3E},
    {0x8135F437, 0x1E3F, 0xE7C7}, {0x8135F438, 0x1E40, 0x1E40}, {0x8136A531, 0x200F, 0x200F}, {0x8136A532, 0x2011, 0x2011},
    {0x8137A838, 0x2641, 0x2641}, {0x8137A839, 0x2643, 0x2643}, {0x8138FD38, 0x2E80, 0x2E80}, {0x8138FD39, 0x2E82, 0x2E82},
    {0x8230A632, 0x3619, 0x3619}, {0x8230A633, 0x361B, 0x361B}, {0x8230F237, 0x3917, 0x3917}, {0x8230F238, 0x3919, 0x3919},
    {0x8231D437, 0x3CDF, 0x3CDF}, {0x8231D438, 0x3CE1, 0x3CE1}, {0x8232AF32, 0x4055, 0x4055}, {0x8232AF33, 0x4057, 0x4057},
    {0x8232C936, 0x415E, 0x415E}, {0x8232C937, 0x4160, 0x4160}, {0x8232F837, 0x4336, 0x4336}, {0x8232F838, 0x4338, 0x4338},
    {0x8233A338, 0x44D5, 0x44D5}, {0x8233A339, 0x44D7, 0x44D7}, {0x8233C931, 0x464B, 0x464B}, {0x8233C932, 0x464D, 0x464D},
    {0x8233E837, 0x478C, 0x478C}, {0x8233E838, 0x478E, 0x478E}, {0x82349638, 0x4946, 0x4946}, {0x82349639, 0x4948, 0x4948},
    {0x8234A130, 0x49B5, 0x49B5}, {0x8234A131, 0x49B8, 0x49B8}, {0x8234E733, 0x4C76, 0x4C76}, {0x8234E734, 0x4C78, 0x4C78},
    {0x82358F32, 0x4DFF, 0x4DFF}, {0x82358F33, 0x9FA6, 0x9FA6}, {0x8336C738, 0xD7FF, 0xD7FF}, {0x8336C739, 0xE76C, 0xE76C},
    {0x8336CF39, 0xE863, 0xE863}, {0x8336D030, 0xE865, 0xE865}, {0x84308534, 0xF92B, 0xF92B}, {0x84308535, 0xF92D, 0xF92D},
    {0x84309C37, 0xFA26, 0xFA26}, {0x84309C38, 0xFA2A, 0xFA2A}, {0x84318537, 0xFE2F, 0xFE2F}, {0x84318538, 0xFE32, 0xFE32},
    {0x8431A233, 0xFFDF, 0xFFDF}, {0x8431A234, 0xFFE6, 0xFFE6}, {0x8431A439, 0xFFFF, 0xFFFF}, {0x8431A530, 0x0, 0x0},
    {0x8F39FE39, 0x0, 0x0}, {0x90308130, 0x0, 0x0}, {0x95328235, 0x0, 0x0}, {0x95328236, 0x0, 0x20000},
    {0x9A348431, 0x0, 0x2FFFF}, {0x9A348432, 0x0, 0x0}, {0x9F368537, 0x0, 0x0}, {0x9F368538, 0x0, 0x0},
    {0xA4388733, 0x0, 0x0}, {0xA4388734, 0x0, 0x0}, {0xAA308839, 0x0, 0x0}, {0xAA308930, 0x0, 0x0},
    {0xAF328A35, 0x0, 0x0}, {0xAF328A36, 0x0, 0x0}, {0xB4348C31, 0x0, 0x0}, {0xB4348C32, 0x0, 0x0},
    {0xB9368D37, 0x0, 0x0}, {0xB9368D38, 0x0, 0x0}, {0xBE388F33, 0x0, 0x0}, {0xBE388F34, 0x0, 0x0},
    {0xC4309039, 0x0, 0x0}, {0xC4309130, 0x0, 0x0}, {0xC9329235, 0x0, 0x0}, {0xC9329236, 0x0, 0x0},
    {0xCE349431, 0x0, 0x0}, {0xCE349432, 0x0, 0x0}, {0xD3369537, 0x0, 0x0}, {0xD3369538, 0x0, 0x0},
    {0xD8389733, 0x0, 0x0}, {0xD8389734, 0x0, 0x0}, {0xDE309839, 0x0, 0x0}, {0xDE309930, 0x0, 0x0},
    {0xE3329A35, 0x0, 0x0}, {0xE3329A36, 0x0, 0x0}, {0xFD308130, 0x0, 0x0}, {0xFE39FE39, 0x0, 0x0},
};
static constexpr Gb18030Difference GB18030_DIFFERENCES[2] = {
    {0xA8BC, 0xE7C7, 0x1E3F, false}, {0x8135F437, 0x1E3F, 0xE7C7, false},
};
static std::uint32_t Gb18030Bytes(std::uint32_t linear) {
    return (0x81 + linear / 12600) << 24 | (0x30 + linear / 1260 % 10) << 16 | (0x81 + linear / 10 % 126) << 8 | (0x30 + linear % 10);
}

static void Fnv(std::uint32_t& hash, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        hash ^= (value >> (8 * i)) & 0xFF;
        hash *= 0x01000193;
    }
}

static std::uint32_t Gb18030Decode(const void* gb, std::uint32_t packed) {
    std::uint8_t bytes[4] = {};
    const std::uint32_t size = packed > 0xFFFF ? 4 : 2;
    for (std::uint32_t i = 0; i < size; ++i) bytes[i] = static_cast<std::uint8_t>(packed >> (8 * (size - 1 - i)));
    std::uint32_t consumed = 0x55;
    std::uint32_t code = 0xDEADBEEF;
    const int result = sceCesGbToUtf32(gb, bytes, size, &consumed, &code);
    if (result != 0) {
        Require(result == unassignedCode && consumed == size && code == 0);
        return 0;
    }
    Require(consumed == size);
    return code;
}

static std::uint32_t Gb18030Encode(const void* gb, std::uint32_t code) {
    std::uint8_t bytes[4] = {};
    std::uint32_t size = 0;
    const int result = sceCesUtf32ToGb(code, gb, bytes, sizeof(bytes), &size);
    if (result == unassignedCode) return 0;
    Require(result == 0 && (size == 2 || size == 4));
    std::uint32_t packed = 0;
    for (std::uint32_t i = 0; i < size; ++i) packed = packed << 8 | bytes[i];
    return packed;
}

static void RequireGb18030Error(const void* gb, const char* bytes, std::uint32_t size, int error, std::uint32_t length) {
    std::uint32_t consumed = 0x55;
    std::uint32_t utf32 = 0xDEADBEEF;
    Require(sceCesGbToUtf32(gb, Bytes(bytes), size, &consumed, &utf32) == error && consumed == length && utf32 == 0);
    std::uint16_t ucs2 = 0xBEEF;
    consumed = 0x55;
    Require(sceCesMbcToUcs2(gb, Bytes(bytes), size, &consumed, &ucs2) == error && consumed == length && ucs2 == 0);
}

static void CheckGb18030(const void* gb, bool revision2005, std::uint32_t twoByteHash, std::uint32_t fourByteHash, std::uint32_t encodeHash, int fourByteOneWay, int codeOneWay) {
    std::uint32_t hash = 0x811C9DC5;
    for (std::uint32_t cell = 0; cell < 126 * 190; ++cell) {
        const std::uint32_t column = cell % 190;
        const std::uint32_t bytes = (0x81 + cell / 190) << 8 | (column < 0x3F ? 0x40 + column : 0x41 + column);
        const std::uint32_t code = Gb18030Decode(gb, bytes);
        Require(code != 0 && Gb18030Encode(gb, code) == bytes);
        Fnv(hash, code);
    }
    Require(hash == twoByteHash);

    hash = 0x811C9DC5;
    int oneWay = 0;
    for (std::uint32_t linear = 0; linear < 39420; ++linear) {
        const std::uint32_t bytes = Gb18030Bytes(linear);
        const std::uint32_t code = Gb18030Decode(gb, bytes);
        Require(code >= 0x80 && code <= 0xFFFF);
        const std::uint32_t back = Gb18030Encode(gb, code);
        if (back != bytes) {
            Require(back <= 0xFFFF && Gb18030Decode(gb, back) == code);
            ++oneWay;
        }
        Fnv(hash, code);
    }
    Require(hash == fourByteHash && oneWay == fourByteOneWay);

    hash = 0x811C9DC5;
    oneWay = 0;
    for (std::uint32_t code = 0x80; code < 0x10000; ++code) {
        if (code >= 0xD800 && code <= 0xDFFF) continue;
        const std::uint32_t bytes = Gb18030Encode(gb, code);
        if (Gb18030Decode(gb, bytes) != code) ++oneWay;
        Fnv(hash, bytes);
    }
    Require(hash == encodeHash && oneWay == codeOneWay);

    hash = 0x811C9DC5;
    int samples = 0;
    for (std::uint32_t linear = 189000; linear <= 189000 + 0xFFFFF; linear += 997) {
        Fnv(hash, Gb18030Decode(gb, Gb18030Bytes(linear)));
        ++samples;
    }
    Require(hash == (revision2005 ? 0x0F212E49u : 0x9832CC85u) && samples == 1052);
    hash = 0x811C9DC5;
    for (std::uint32_t code = 0x10000; code < 0x110000; code += 997) Fnv(hash, Gb18030Encode(gb, code));
    Require(hash == (revision2005 ? 0x2EB90A7Bu : 0x9832CC85u));

    for (const Gb18030Sequence& entry : GB18030_BOUNDARIES) {
        const std::uint32_t code = revision2005 ? entry.code2005 : entry.code2000;
        Require(Gb18030Decode(gb, entry.bytes) == code);
        if (code != 0) Require(Gb18030Encode(gb, code) == entry.bytes);
    }
    for (const Gb18030Difference& entry : GB18030_DIFFERENCES) {
        const std::uint32_t code = revision2005 ? entry.code2005 : entry.code2000;
        Require(Gb18030Decode(gb, entry.bytes) == code && Gb18030Encode(gb, code) == entry.bytes);
        if (revision2005 && entry.fallback) Require(Gb18030Encode(gb, entry.code2000) == entry.bytes);
    }

    RequireGb18030Error(gb, "\x80", 1, invalidEncode, 1);
    RequireGb18030Error(gb, "\xFF", 1, invalidEncode, 1);
    RequireGb18030Error(gb, "\x81", 1, srcBufferEnd, 2);
    RequireGb18030Error(gb, "\x81\x30", 2, srcBufferEnd, 4);
    RequireGb18030Error(gb, "\x81\x30\x81", 3, srcBufferEnd, 4);
    RequireGb18030Error(gb, "\x81\x00", 2, invalidEncode, 1);
    RequireGb18030Error(gb, "\x81\x2F", 2, invalidEncode, 2);
    RequireGb18030Error(gb, "\x81\x3A", 2, invalidEncode, 2);
    RequireGb18030Error(gb, "\x81\x7F", 2, invalidEncode, 2);
    RequireGb18030Error(gb, "\x81\xFF", 2, invalidEncode, 2);
    RequireGb18030Error(gb, "\x81\x30\x80\x30", 4, invalidEncode, 4);
    RequireGb18030Error(gb, "\x81\x30\xFF\x30", 4, invalidEncode, 4);
    RequireGb18030Error(gb, "\x81\x30\x30\x30", 4, invalidEncode, 4);
    RequireGb18030Error(gb, "\x81\x30\x81\x2F", 4, invalidEncode, 4);
    RequireGb18030Error(gb, "\x81\x30\x81\x3A", 4, invalidEncode, 4);
    RequireGb18030Error(gb, "\x81\x30\x81\x81", 4, invalidEncode, 4);
    RequireGb18030Error(gb, "\x84\x31\xA5\x30", 4, unassignedCode, 4);
    RequireGb18030Error(gb, "\xE3\x32\x9A\x36", 4, unassignedCode, 4);
    RequireGb18030Error(gb, "\xFE\x39\xFE\x39", 4, unassignedCode, 4);

    std::uint32_t consumed = 0x55;
    std::uint32_t produced = 0x55;
    std::uint16_t ucs2 = 0xBEEF;
    std::uint8_t utf8[4] = {1, 2, 3, 4};
    std::uint16_t utf16[2] = {0xBEEF, 0xBEEF};
    for (const char* refused : {"\x90\x30\x81\x30", "\x94\x39\xFC\x36", "\xE3\x32\x9A\x35"}) {
        Require(sceCesGbToUtf8(gb, Bytes(refused), 4, &consumed, utf8, 4, &produced) == unassignedCode && consumed == 4 && produced == 0);
    }
    consumed = 0x55;
    ucs2 = 0xBEEF;
    Require(sceCesGbToUcs2(gb, Bytes("\x95\x32\x82\x36"), 4, &consumed, &ucs2) == unassignedCode);
    if (revision2005) {
        Require(consumed == 0x55 && ucs2 == 0xBEEF);
        consumed = 0x55;
        produced = 0x55;
        Require(sceCesGbToUtf8(gb, Bytes("\x95\x32\x82\x36"), 4, &consumed, utf8, 3, &produced) == dstBufferEnd && consumed == 0x55 && produced == 0x55);
        Require(sceCesGbToUtf8(gb, Bytes("\x95\x32\x82\x36"), 4, &consumed, utf8, 4, &produced) == 0 && consumed == 4 && produced == 4);
        Require(std::memcmp(utf8, "\xF0\xA0\x80\x80", 4) == 0);
        produced = 0x55;
        Require(sceCesGbToUtf16(gb, Bytes("\x9A\x34\x84\x31"), 4, &consumed, utf16, 1, &produced) == dstBufferEnd && produced == 0x55 && utf16[0] == 0xBEEF);
        Require(sceCesGbToUtf16(gb, Bytes("\x9A\x34\x84\x31"), 4, &consumed, utf16, 2, &produced) == 0 && produced == 2 && utf16[0] == 0xD87F && utf16[1] == 0xDFFF);
        Require(sceCesGbToUtf16be(gb, Bytes("\x95\x32\x82\x36"), 4, &consumed, utf16, 2, &produced) == 0 && std::memcmp(utf16, "\xD8\x40\xDC\x00", 4) == 0);
    } else {
        Require(sceCesGbToUtf8(gb, Bytes("\x95\x32\x82\x36"), 4, &consumed, utf8, 4, &produced) == unassignedCode && consumed == 4 && produced == 0);
    }
    Require(sceCesGbToUcs2(gb, Bytes("\xA2\xE3"), 2, &consumed, &ucs2) == 0 && consumed == 2 && ucs2 == 0x20AC);

    std::uint8_t mbcs[4] = {1, 2, 3, 4};
    produced = 0x55;
    Require(sceCesUtf32ToGb(0x10000, gb, mbcs, sizeof(mbcs), &produced) == unassignedCode && produced == 0);
    produced = 0x55;
    Require(sceCesUtf32ToGb(0x20000, gb, mbcs, 3, &produced) == (revision2005 ? dstBufferEnd : unassignedCode) && produced == (revision2005 ? 4u : 0u) && std::memcmp(mbcs, "\x01\x02\x03\x04", 4) == 0);
    Require(sceCesUtf32ToGb(0xD800, gb, mbcs, sizeof(mbcs), &produced) == outOfCodeSpace);
    Require(sceCesUtf32ToGb(0x110000, gb, mbcs, sizeof(mbcs), &produced) == invalidEncode);
    Require(sceCesUcs2ToGb(0x80, gb, mbcs, sizeof(mbcs), &produced) == 0 && produced == 4 && std::memcmp(mbcs, "\x81\x30\x81\x30", 4) == 0);
    Require(sceCesUcs2ToGb(0x20AC, gb, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && std::memcmp(mbcs, "\xA2\xE3", 2) == 0);
    Require(sceCesUtf8ToGb(Bytes("\xF0\x9F\x98\x80"), 4, &consumed, gb, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    const std::uint16_t pair[2] = {0xD840, 0xDC00};
    if (revision2005) Require(sceCesUtf16ToGb(pair, 2, &consumed, gb, mbcs, sizeof(mbcs), &produced) == 0 && consumed == 2 && produced == 4 && std::memcmp(mbcs, "\x95\x32\x82\x36", 4) == 0);
    else Require(sceCesUtf16ToGb(pair, 2, &consumed, gb, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUtf16ToGb(pair, 1, &consumed, gb, mbcs, sizeof(mbcs), &produced) == srcBufferEnd);

    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesMbcsUcsContextInit(context, gb) == 0);
    const char* utf8Text = revision2005 ? "A\xE4\xB8\xAD\xC2\x80\xF0\xA0\x80\x80\xEF\xB8\x90" : "A\xE4\xB8\xAD\xC2\x80\xEF\xB8\x90";
    const std::uint32_t utf8Size = revision2005 ? 13 : 9;
    const char* gbText = revision2005 ? "A\xD6\xD0\x81\x30\x81\x30\x95\x32\x82\x36\x84\x31\x82\x36" : "A\xD6\xD0\x81\x30\x81\x30\x84\x31\x82\x36";
    const std::uint32_t gbSize = revision2005 ? 15 : 11;
    const std::uint32_t units = revision2005 ? 6 : 4;
    std::uint8_t text[16] = {};
    Require(sceCesUtf8StrToMbcsStr(context, Bytes(utf8Text), utf8Size, &consumed, text, sizeof(text), &produced) == 0 && consumed == utf8Size && produced == gbSize);
    Require(std::memcmp(text, gbText, gbSize) == 0);
    std::uint8_t backUtf8[16] = {};
    Require(sceCesMbcsStrToUtf8Str(context, text, gbSize, &consumed, backUtf8, sizeof(backUtf8), &produced) == 0 && consumed == gbSize && produced == utf8Size);
    Require(std::memcmp(backUtf8, utf8Text, utf8Size) == 0);
    std::uint16_t backUtf16[8] = {};
    Require(sceCesMbcsStrToUtf16Str(context, text, gbSize, &consumed, backUtf16, 8, &produced) == 0 && consumed == gbSize && produced == units);
    Require(backUtf16[0] == 'A' && backUtf16[1] == 0x4E2D && backUtf16[2] == 0x80 && backUtf16[units - 1] == 0xFE10);
    if (revision2005) Require(backUtf16[3] == 0xD840 && backUtf16[4] == 0xDC00);
    std::uint16_t shortUtf16[8] = {};
    Require(sceCesMbcsStrToUtf16Str(context, text, gbSize, &consumed, shortUtf16, units - 2, &produced) == dstBufferEnd);
    Require(sceCesMbcsStrGetUtf16Len(context, text, gbSize, &consumed, &produced) == 0 && consumed == gbSize && produced == units);
    std::uint8_t again[16] = {};
    Require(sceCesUtf16StrToMbcsStr(context, backUtf16, units, &consumed, again, sizeof(again), &produced) == 0 && consumed == units && produced == gbSize);
    Require(std::memcmp(again, gbText, gbSize) == 0);
    std::uint16_t ucs2Text[8] = {};
    if (revision2005) Require(sceCesMbcsStrToUcs2Str(context, text, gbSize, &consumed, ucs2Text, 8, &produced) == unassignedCode);
    Require(sceCesUtf8StrToMbcsStr(context, Bytes("\xC2\x80"), 2, &consumed, text, 3, &produced) == dstBufferEnd);
}

int main() {
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
    Require(sceCesUcs2ToEucCn(0xDC00, euc, mbcs, sizeof(mbcs), &produced) == outOfCodeSpace);
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

    alignas(8) std::uint8_t gb2000Sheet[64] = {};
    alignas(8) std::uint8_t gb2005Sheet[64] = {};
    const void* gb2000 = sceCesUcsProfileInitGb18030_2000(gb2000Sheet);
    const void* gb2005 = sceCesUcsProfileInitGb18030_2005(gb2005Sheet);
    Require(gb2000 == gb2000Sheet && gb2005 == gb2005Sheet);
    Require(sceCesUcsProfileInitGb18030_2000(nullptr) == nullptr && sceCesUcsProfileInitGb18030_2005(nullptr) == nullptr);
    Require(sceCesEucCnToUcs2(gb2000, Bytes("\xD6\xD0"), 2, &consumed, &ucs2) == invalidParameter);
    Require(sceCesGbToUcs2(gb2005, Bytes("\xD6\xD0"), 2, &consumed, &ucs2) == 0 && consumed == 2 && ucs2 == 0x4E2D);
    CheckGb18030(gb2000, false, 0xE61D3BD3, 0x83476043, 0x469CFCE5, 0, 0);
    CheckGb18030(gb2005, true, 0xE4CF78F0, 0x8D188DFC, 0xD675EFA1, 0, 0);
    return 0;
}
