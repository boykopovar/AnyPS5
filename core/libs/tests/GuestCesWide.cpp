#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

static const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

using Utf16Decode = int (APS5_VABI *)(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
using Utf32Decode = int (APS5_VABI *)(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
using WideEncode = int (APS5_VABI *)(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using ScalarEncode = int (APS5_VABI *)(std::uint32_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using UnitEncode = int (APS5_VABI *)(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);

struct Codecs {
    WideEncode native, be, le;
    ScalarEncode scalar;
    UnitEncode unitBe, unitLe;
};

enum WideForm { Utf16Native, Utf16Be, Utf16Le, Utf32Be, Utf32Le, Utf32Scalar, WideFormCount };

static constexpr std::uint32_t untouched = 0xEEEEEEEE;

static void Put(std::uint8_t* target, std::uint32_t value, std::uint32_t width, bool bigEndian) {
    for (std::uint32_t i = 0; i < width; ++i) target[i] = static_cast<std::uint8_t>(value >> (8 * (bigEndian ? width - 1 - i : i)));
}

static int CallWide(int form, const std::uint32_t* units, std::uint32_t count, const void* profile, std::uint8_t* mbc, std::uint32_t mbcMax, std::uint32_t* mbcLen, std::uint32_t* sourceLen) {
    alignas(4) std::uint8_t bytes[8] = {};
    const std::uint32_t width = form >= Utf32Be ? 4 : 2;
    const bool big = form == Utf16Be || form == Utf32Be || (form == Utf16Native && std::endian::native == std::endian::big);
    for (std::uint32_t i = 0; units && i < count; ++i) Put(bytes + i * width, units[i], width, big);
    const auto* utf16 = reinterpret_cast<const std::uint16_t*>(bytes);
    const auto* utf32 = reinterpret_cast<const std::uint32_t*>(bytes);
    const std::uint32_t value = units ? units[0] : 0;
    switch (form) {
    case Utf16Native: return sceCesUtf16ToMbc(utf16, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf16Be: return sceCesUtf16beToMbc(utf16, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf16Le: return sceCesUtf16leToMbc(utf16, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf32Be: return sceCesUtf32beToMbc(utf32, profile, mbc, mbcMax, mbcLen);
    case Utf32Le: return sceCesUtf32leToMbc(utf32, profile, mbc, mbcMax, mbcLen);
    default: return sceCesUtf32ToMbc(value, profile, mbc, mbcMax, mbcLen);
    }
}

static int CallNamed(int form, const std::uint32_t* units, std::uint32_t count, const void* profile, std::uint8_t* mbc, std::uint32_t mbcMax, std::uint32_t* mbcLen, std::uint32_t* sourceLen, const Codecs& codecs) {
    alignas(4) std::uint8_t bytes[8] = {};
    const std::uint32_t width = form >= Utf32Be ? 4 : 2;
    const bool big = form == Utf16Be || form == Utf32Be || (form == Utf16Native && std::endian::native == std::endian::big);
    for (std::uint32_t i = 0; units && i < count; ++i) Put(bytes + i * width, units[i], width, big);
    const auto* utf16 = reinterpret_cast<const std::uint16_t*>(bytes);
    const auto* utf32 = reinterpret_cast<const std::uint32_t*>(bytes);
    const std::uint32_t value = units ? units[0] : 0;
    switch (form) {
    case Utf16Native: return codecs.native(utf16, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf16Be: return codecs.be(utf16, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf16Le: return codecs.le(utf16, count, sourceLen, profile, mbc, mbcMax, mbcLen);
    case Utf32Be: return codecs.unitBe(utf32, profile, mbc, mbcMax, mbcLen);
    case Utf32Le: return codecs.unitLe(utf32, profile, mbc, mbcMax, mbcLen);
    default: return codecs.scalar(value, profile, mbc, mbcMax, mbcLen);
    }
}

static void RequireWideRefused(int form, const std::uint32_t* units, std::uint32_t count, const void* profile, const Codecs& codecs, bool target, std::uint32_t mbcMax) {
    std::uint8_t mbc[4] = {0xEE, 0xEE, 0xEE, 0xEE};
    std::uint32_t mbcLen = untouched;
    std::uint32_t sourceLen = untouched;
    const int generic = CallWide(form, units, count, profile, target ? mbc : nullptr, mbcMax, &mbcLen, &sourceLen);
    std::uint8_t wide[4] = {0xEE, 0xEE, 0xEE, 0xEE};
    std::uint32_t wideLen = untouched;
    std::uint32_t wideSource = untouched;
    Require(CallNamed(form, units, count, profile, target ? wide : nullptr, mbcMax, &wideLen, &wideSource, codecs) == generic);
    Require(std::memcmp(wide, mbc, 4) == 0 && wideLen == mbcLen && wideSource == sourceLen);
}

static void RequireWideCode(const void* profile, const Codecs& codecs, std::uint32_t code, const char* bytes, std::uint32_t size) {
    std::uint32_t pair[2] = {code, code};
    if (code >= 0x10000) {
        pair[0] = 0xD800 + ((code - 0x10000) >> 10);
        pair[1] = 0xDC00 + ((code - 0x10000) & 0x3FF);
    }
    for (int form = Utf16Native; form < WideFormCount; ++form) {
        const std::uint32_t* units = form >= Utf32Be ? &code : pair;
        const std::uint32_t count = form >= Utf32Be || code < 0x10000 ? 1u : 2u;
        std::uint8_t mbc[4] = {0xEE, 0xEE, 0xEE, 0xEE};
        std::uint32_t mbcLen = untouched;
        std::uint32_t sourceLen = untouched;
        const int result = CallWide(form, units, count, profile, mbc, sizeof(mbc), &mbcLen, &sourceLen);
        std::uint8_t wide[4] = {0xEE, 0xEE, 0xEE, 0xEE};
        std::uint32_t wideLen = untouched;
        std::uint32_t wideSource = untouched;
        Require(CallNamed(form, units, count, profile, wide, sizeof(wide), &wideLen, &wideSource, codecs) == result);
        Require(wideLen == mbcLen && wideSource == sourceLen && std::memcmp(wide, mbc, 4) == 0);
        if (result != 0) continue;
        if (size != 0) Require(mbcLen == size && wideLen == size && std::memcmp(wide, bytes, size) == 0);
        if (form <= Utf16Le) Require(sourceLen == count && wideSource == count);
    }
}

static void RequireNamedRefused(int form, const std::uint32_t* units, std::uint32_t count, const void* profile, const Codecs& codecs) {
    std::uint8_t mbc[4] = {0xEE, 0xEE, 0xEE, 0xEE};
    std::uint32_t mbcLen = untouched;
    std::uint32_t sourceLen = untouched;
    Require(CallNamed(form, units, count, profile, mbc, sizeof(mbc), &mbcLen, &sourceLen, codecs) == static_cast<int>(0x805C0001));
    Require(mbcLen == untouched && sourceLen == untouched && mbc[0] == 0xEE && mbc[1] == 0xEE && mbc[2] == 0xEE && mbc[3] == 0xEE);
}

static void RequireWideEncoding(const void* profile, const Codecs& codecs, const void* foreign, std::uint32_t sampleCode, const char* bytes, std::uint32_t size) {
    const std::uint32_t loneLow[1] = {0xDC00};
    const std::uint32_t loneHigh[1] = {0xD800};
    const std::uint32_t beyond[1] = {0x110000};
    const std::uint32_t sample[1] = {sampleCode};
    for (int form = Utf16Native; form < WideFormCount; ++form) {
        RequireNamedRefused(form, sample, 1, nullptr, codecs);
        RequireNamedRefused(form, sample, 1, foreign, codecs);
        RequireWideRefused(form, sample, 1, profile, codecs, false, 4);
        RequireWideRefused(form, sample, 0, profile, codecs, false, 4);
    }
    for (int form = Utf16Native; form <= Utf16Le; ++form) {
        RequireWideRefused(form, loneLow, 1, profile, codecs, true, 4);
        RequireWideRefused(form, loneHigh, 1, profile, codecs, true, 4);
    }
    RequireWideRefused(Utf32Be, loneHigh, 1, profile, codecs, true, 4);
    RequireWideRefused(Utf32Le, loneHigh, 1, profile, codecs, true, 4);
    RequireWideRefused(Utf32Scalar, loneHigh, 1, profile, codecs, true, 4);
    RequireWideRefused(Utf32Be, beyond, 1, profile, codecs, true, 4);
    RequireWideRefused(Utf32Le, beyond, 1, profile, codecs, true, 4);
    RequireWideRefused(Utf32Scalar, beyond, 1, profile, codecs, true, 4);
    RequireWideCode(profile, codecs, 0, nullptr, 0);
    for (std::uint32_t code = 1; code < 0x110000; ++code) {
        if (code >= 0xD800 && code <= 0xDFFF) continue;
        RequireWideCode(profile, codecs, code, nullptr, 0);
    }
    RequireWideCode(profile, codecs, sampleCode, bytes, size);
    std::uint8_t mbc[4] = {0xEE, 0xEE, 0xEE, 0xEE};
    std::uint32_t mbcLen = untouched;
    std::uint32_t sourceLen = untouched;
    Require(CallNamed(Utf16Le, sample, 1, profile, mbc, 1, &mbcLen, &sourceLen, codecs) == static_cast<int>(0x805C0031));
    Require(mbcLen == untouched && sourceLen == untouched && mbc[0] == 0xEE && mbc[1] == 0xEE);
    mbcLen = untouched;
    Require(CallNamed(Utf32Scalar, sample, 1, profile, mbc, 1, &mbcLen, nullptr, codecs) == static_cast<int>(0x805C0031));
    Require(mbcLen == size && mbc[0] == 0xEE);
}

static void RequireWideDecoding(Utf16Decode utf16le, Utf32Decode utf32be, Utf32Decode utf32le, const void* profile, const void* foreign, const std::uint8_t* sample, std::uint32_t code) {
    for (std::uint32_t lead = 0; lead <= 0xFF; ++lead) {
        for (std::uint32_t trail = 0; trail <= 0xFF; ++trail) {
            const std::uint8_t cell[2] = {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint32_t used = untouched;
            std::uint32_t value = 0;
            const int result = sceCesMbcToUtf32(profile, cell, 2, &used, &value);
            std::uint32_t consumed = untouched;
            std::uint32_t wide = 0;
            Require(utf32be(profile, cell, 2, &consumed, &wide) == result && consumed == used);
            std::uint32_t genericBe = 0;
            consumed = untouched;
            Require(sceCesMbcToUtf32be(profile, cell, 2, &consumed, &genericBe) == result && consumed == used);
            Require(std::memcmp(&wide, &genericBe, 4) == 0);
            consumed = untouched;
            std::uint32_t wideLe = 0;
            Require(utf32le(profile, cell, 2, &consumed, &wideLe) == result && consumed == used);
            std::uint32_t genericLe = 0;
            consumed = untouched;
            Require(sceCesMbcToUtf32le(profile, cell, 2, &consumed, &genericLe) == result && consumed == used);
            Require(std::memcmp(&wideLe, &genericLe, 4) == 0);
            if (result != 0) continue;
            std::uint8_t expected[4];
            Put(expected, value, 4, true);
            Require(std::memcmp(&wide, expected, 4) == 0);
            Put(expected, value, 4, false);
            Require(std::memcmp(&wideLe, expected, 4) == 0);
            std::uint16_t units[2] = {0xBBBB, 0xBBBB};
            std::uint32_t produced = untouched;
            consumed = untouched;
            Require(utf16le(profile, cell, 2, &consumed, units, 2, &produced) == 0 && consumed == used && produced >= 1);
            std::uint16_t genericUnit[2] = {0xBBBB, 0xBBBB};
            std::uint32_t genericProduced = untouched;
            consumed = untouched;
            Require(sceCesMbcToUtf16le(profile, cell, 2, &consumed, genericUnit, 2, &genericProduced) == 0);
            Require(produced == genericProduced && std::memcmp(units, genericUnit, 2 * produced) == 0);
        }
    }
    std::uint32_t used = untouched;
    std::uint32_t produced = untouched;
    std::uint16_t units[2] = {0xBBBB, 0xBBBB};
    std::uint32_t wide = 0xDEADBEEF;
    Require(utf16le(foreign, sample, 2, &used, units, 2, &produced) == static_cast<int>(0x805C0001) && used == untouched && produced == untouched && units[0] == 0xBBBB);
    used = untouched;
    produced = untouched;
    Require(utf16le(nullptr, sample, 2, &used, units, 2, &produced) == static_cast<int>(0x805C0001) && used == untouched && produced == untouched);
    used = untouched;
    produced = untouched;
    Require(utf16le(profile, nullptr, 2, &used, units, 2, &produced) == static_cast<int>(0x805C0010) && used == untouched && produced == untouched);
    used = untouched;
    produced = untouched;
    Require(utf16le(profile, sample, 0, &used, units, 2, &produced) == static_cast<int>(0x805C0011) && used == untouched && produced == untouched);
    used = untouched;
    produced = untouched;
    Require(utf16le(profile, sample, 2, &used, nullptr, 2, &produced) == static_cast<int>(0x805C0030) && used == untouched && produced == untouched);
    used = untouched;
    produced = untouched;
    Require(utf16le(profile, sample, 2, &used, units, 0, &produced) == static_cast<int>(0x805C0031) && used == untouched && produced == untouched && units[0] == 0xBBBB);
    Require(utf16le(profile, sample, 2, nullptr, units, 1, nullptr) == 0);
    std::uint8_t expected[4];
    Put(expected, code, 2, false);
    Require(std::memcmp(units, expected, 2) == 0);
    used = untouched;
    wide = 0xDEADBEEF;
    Require(utf32be(foreign, sample, 2, &used, &wide) == static_cast<int>(0x805C0001) && used == untouched && wide == 0xDEADBEEF);
    used = untouched;
    Require(utf32be(profile, nullptr, 2, &used, &wide) == static_cast<int>(0x805C0010) && used == untouched);
    used = untouched;
    Require(utf32be(profile, sample, 0, &used, &wide) == static_cast<int>(0x805C0011) && used == untouched);
    used = untouched;
    Require(utf32be(profile, sample, 2, &used, nullptr) == static_cast<int>(0x805C0030) && used == untouched);
    used = untouched;
    Require(utf32be(profile, sample, 2, nullptr, &wide) == 0);
    Put(expected, code, 4, true);
    Require(std::memcmp(&wide, expected, 4) == 0);
    if (utf32le != nullptr) {
        used = untouched;
        Require(utf32le(foreign, sample, 2, &used, &wide) == static_cast<int>(0x805C0001) && used == untouched);
        used = untouched;
        Require(utf32le(profile, sample, 2, nullptr, &wide) == 0);
        Put(expected, code, 4, false);
        Require(std::memcmp(&wide, expected, 4) == 0);
    }
}

int main() {
    constexpr int invalidParameter = static_cast<int>(0x805C0001);
    constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
    constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
    constexpr int invalidEncode = static_cast<int>(0x805C0014);
    constexpr int illegalCode = static_cast<int>(0x805C0015);
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
    Require(sceCesUtf32ToSbc(0xD800, cp1252, &sbc) == illegalCode);
    Require(sceCesUtf32ToSbc(0xDFFF, sceCesRefersUcsProfileCp1254(), &sbc) == illegalCode);
    std::uint32_t yBe = 0;
    std::memcpy(&yBe, "\x00\x00\x01\x78", 4);
    Require(sceCesUtf32beToSbc(&yBe, cp1252, &sbc) == 0 && sbc == 0x9F);
    std::uint32_t yLe = 0;
    std::memcpy(&yLe, "\x78\x01\x00\x00", 4);
    Require(sceCesUtf32leToSbc(&yLe, cp1252, &sbc) == 0 && sbc == 0x9F);
    Require(sceCesUtf32leToSbc(nullptr, cp1252, &sbc) == invalidSrcBuffer);
    Require(sceCesUtf32beToSbc(nullptr, cp1252, &sbc) == invalidSrcBuffer);
    Require(sceCesUtf16beToSbc(nullptr, 1, &length, cp1252, &sbc) == invalidSrcBuffer);
    Require(sceCesUtf16leToSbc(nullptr, 1, &length, cp1252, &sbc) == invalidSrcBuffer);
    Require(sceCesUtf32ToSbc(0x0178, nullptr, &sbc) == invalidParameter);
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

    alignas(8) std::uint8_t eucJpSheet[64] = {};
    alignas(8) std::uint8_t eucCnSheet[64] = {};
    alignas(8) std::uint8_t gbkSheet[64] = {};
    alignas(8) std::uint8_t gb18030Sheet[64] = {};
    const void* eucJp = sceCesUcsProfileInitEucJpCp51932(eucJpSheet);
    const void* eucCn = sceCesUcsProfileInitEucCnGb2312(eucCnSheet);
    const void* gbk = sceCesUcsProfileInitGbkCp936(gbkSheet);
    const void* gb18030 = sceCesUcsProfileInitGb18030_2000(gb18030Sheet);
    Require(eucJp == eucJpSheet && eucCn == eucCnSheet && gbk == gbkSheet && gb18030 == gb18030Sheet);

    const Codecs toEucJp = {sceCesUtf16ToEucJp, sceCesUtf16beToEucJp, sceCesUtf16leToEucJp, sceCesUtf32ToEucJp, sceCesUtf32beToEucJp, sceCesUtf32leToEucJp};
    const Codecs toEucCn = {sceCesUtf16ToEucCn, sceCesUtf16beToEucCn, sceCesUtf16leToEucCn, sceCesUtf32ToEucCn, sceCesUtf32beToEucCn, sceCesUtf32leToEucCn};
    const Codecs toGb = {sceCesUtf16ToGb, sceCesUtf16beToGb, sceCesUtf16leToGb, sceCesUtf32ToGb, sceCesUtf32beToGb, sceCesUtf32leToGb};
    const Codecs toSjis = {sceCesUtf16ToSJis, sceCesUtf16beToSJis, sceCesUtf16leToSJis, sceCesUtf32ToSJis, sceCesUtf32beToSJis, sceCesUtf32leToSJis};

    RequireWideDecoding(sceCesEucCnToUtf16le, sceCesEucCnToUtf32be, sceCesEucCnToUtf32le, eucCn, gbk, Bytes("\xD6\xD0"), 0x4E2D);
    RequireWideDecoding(sceCesEucJpToUtf16le, sceCesEucJpToUtf32be, sceCesEucJpToUtf32le, eucJp, eucCn, Bytes("\xA4\xA2"), 0x3042);
    RequireWideDecoding(sceCesGbToUtf16le, sceCesGbToUtf32be, sceCesGbToUtf32le, gbk, eucCn, Bytes("\xD6\xD0"), 0x4E2D);
    RequireWideDecoding(sceCesGbToUtf16le, sceCesGbToUtf32be, sceCesGbToUtf32le, gb18030, eucCn, Bytes("\xD6\xD0"), 0x4E2D);

    RequireWideEncoding(eucCn, toEucCn, gbk, 0x4E2D, "\xD6\xD0", 2);
    RequireWideEncoding(eucJp, toEucJp, eucCn, 0x3042, "\xA4\xA2", 2);
    RequireWideEncoding(gbk, toGb, eucCn, 0x4E2D, "\xD6\xD0", 2);
    RequireWideEncoding(sjis, toSjis, eucCn, 0x3042, "\x82\xA0", 2);

    RequireWideCode(gb18030, toGb, 0x10000, "\x90\x30\x81\x30", 4);
    RequireWideCode(gb18030, toGb, 0x1F600, "\x94\x39\xFC\x36", 4);
    RequireWideCode(gbk, toGb, 0x10000, nullptr, 0);
    RequireWideCode(gbk, toGb, 0x1F600, nullptr, 0);
    return 0;
}
