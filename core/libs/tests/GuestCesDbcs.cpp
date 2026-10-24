#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
void* APS5_VABI sceCesUcsProfileInitEucKr(void*);
void* APS5_VABI sceCesUcsProfileInitUhc(void*);
void* APS5_VABI sceCesUcsProfileInitBig5Cp950(void*);
void* APS5_VABI sceCesUcsProfileInitEucJpCp51932(void*);
int APS5_VABI sceCesEucKrToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUhcToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesBig5ToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
int APS5_VABI sceCesUtf8StrToMbcsStr(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUcs2StrToMbcsStr(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUcs2Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucKrToUcs2(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
int APS5_VABI sceCesEucKrToUtf16(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucKrToUtf16be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucKrToUtf16le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucKrToUtf32(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesEucKrToUtf32be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesEucKrToUtf32le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUhcToUcs2(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
int APS5_VABI sceCesUhcToUtf16(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUhcToUtf16be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUhcToUtf16le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUhcToUtf32(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUhcToUtf32be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUhcToUtf32le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesBig5ToUcs2(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
int APS5_VABI sceCesBig5ToUtf16(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesBig5ToUtf16be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesBig5ToUtf16le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesBig5ToUtf32(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesBig5ToUtf32be(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesBig5ToUtf32le(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUcs2ToEucKr(std::uint16_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToEucKr(const std::uint8_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16ToEucKr(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16beToEucKr(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16leToEucKr(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32ToEucKr(std::uint32_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32beToEucKr(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32leToEucKr(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUcs2ToUhc(std::uint16_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToUhc(const std::uint8_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16ToUhc(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16beToUhc(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16leToUhc(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32ToUhc(std::uint32_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32beToUhc(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32leToUhc(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUcs2ToBig5(std::uint16_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToBig5(const std::uint8_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16ToBig5(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16beToBig5(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16leToBig5(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32ToBig5(std::uint32_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32beToBig5(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32leToBig5(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
}

using ToUtf8 = int (APS5_VABI*)(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using ToUcs2 = int (APS5_VABI*)(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
using ToUtf16 = int (APS5_VABI*)(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
using ToUtf32 = int (APS5_VABI*)(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
using FromUcs2 = int (APS5_VABI*)(std::uint16_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using FromUtf8 = int (APS5_VABI*)(const std::uint8_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using FromUtf16 = int (APS5_VABI*)(const std::uint16_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using FromUtf32 = int (APS5_VABI*)(std::uint32_t, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
using FromUtf32Unit = int (APS5_VABI*)(const std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);

struct Decoders {
    ToUtf8 utf8;
    ToUcs2 ucs2;
    ToUtf16 utf16[3];
    ToUtf32 utf32[3];
};

struct Encoders {
    FromUcs2 ucs2;
    FromUtf8 utf8;
    FromUtf16 utf16[3];
    FromUtf32 utf32;
    FromUtf32Unit utf32Unit[3];
};

constexpr Decoders eucKrDecoders = {sceCesEucKrToUtf8, sceCesEucKrToUcs2, {sceCesEucKrToUtf16, sceCesEucKrToUtf16be, sceCesEucKrToUtf16le}, {sceCesEucKrToUtf32, sceCesEucKrToUtf32be, sceCesEucKrToUtf32le}};
constexpr Decoders uhcDecoders = {sceCesUhcToUtf8, sceCesUhcToUcs2, {sceCesUhcToUtf16, sceCesUhcToUtf16be, sceCesUhcToUtf16le}, {sceCesUhcToUtf32, sceCesUhcToUtf32be, sceCesUhcToUtf32le}};
constexpr Decoders big5Decoders = {sceCesBig5ToUtf8, sceCesBig5ToUcs2, {sceCesBig5ToUtf16, sceCesBig5ToUtf16be, sceCesBig5ToUtf16le}, {sceCesBig5ToUtf32, sceCesBig5ToUtf32be, sceCesBig5ToUtf32le}};
constexpr Encoders eucKrEncoders = {sceCesUcs2ToEucKr, sceCesUtf8ToEucKr, {sceCesUtf16ToEucKr, sceCesUtf16beToEucKr, sceCesUtf16leToEucKr}, sceCesUtf32ToEucKr, {nullptr, sceCesUtf32beToEucKr, sceCesUtf32leToEucKr}};
constexpr Encoders uhcEncoders = {sceCesUcs2ToUhc, sceCesUtf8ToUhc, {sceCesUtf16ToUhc, sceCesUtf16beToUhc, sceCesUtf16leToUhc}, sceCesUtf32ToUhc, {nullptr, sceCesUtf32beToUhc, sceCesUtf32leToUhc}};
constexpr Encoders big5Encoders = {sceCesUcs2ToBig5, sceCesUtf8ToBig5, {sceCesUtf16ToBig5, sceCesUtf16beToBig5, sceCesUtf16leToBig5}, sceCesUtf32ToBig5, {nullptr, sceCesUtf32beToBig5, sceCesUtf32leToBig5}};

constexpr int invalidParameter = static_cast<int>(0x805C0001);
constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
constexpr int invalidEncode = static_cast<int>(0x805C0014);
constexpr int illegalCode = static_cast<int>(0x805C0015);
constexpr int unassignedCode = static_cast<int>(0x805C0020);
constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
constexpr int dstBufferEnd = static_cast<int>(0x805C0031);

static void Require(bool value) { if (!value) std::abort(); }

static const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

static std::uint32_t Size(const char* text) { return static_cast<std::uint32_t>(std::strlen(text)); }

static std::uint64_t Fnv(std::uint64_t hash, std::uint8_t byte) { return (hash ^ byte) * 0x100000001B3ull; }

static void RequireTable(ToUtf8 convert, const void* profile, std::uint64_t expectedHash, std::uint32_t expectedCells) {
    std::uint64_t hash = 0xCBF29CE484222325ull;
    std::uint32_t cells = 0;
    for (std::uint32_t lead = 0; lead < 256; ++lead) {
        for (std::uint32_t trail = 0; trail < 256; ++trail) {
            const std::uint8_t source[2] = {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint8_t utf8[4] = {};
            std::uint32_t consumed = 0;
            std::uint32_t produced = 0;
            const int result = convert(profile, source, sizeof(source), &consumed, utf8, sizeof(utf8), &produced);
            hash = Fnv(hash, static_cast<std::uint8_t>(result & 0xFF));
            if (result != 0) continue;
            hash = Fnv(hash, static_cast<std::uint8_t>(consumed));
            for (std::uint32_t i = 0; i < produced; ++i) hash = Fnv(hash, utf8[i]);
            if (consumed == 2) ++cells;
        }
    }
    Require(hash == expectedHash && cells == expectedCells);
}

static void RequireScalar(ToUtf8 convert, const void* profile, const char* source, const char* expected) {
    std::uint8_t utf8[4] = {};
    std::uint32_t consumed = 0;
    std::uint32_t produced = 0;
    Require(convert(profile, Bytes(source), Size(source), &consumed, utf8, sizeof(utf8), &produced) == 0);
    Require(consumed == Size(source) && produced == Size(expected) && std::memcmp(utf8, expected, produced) == 0);
}

static int Scalar(ToUtf8 convert, const void* profile, const char* source, std::uint32_t sourceMax, std::uint32_t utf8Max) {
    std::uint8_t utf8[4] = {0x55, 0x55, 0x55, 0x55};
    std::uint32_t consumed = 77;
    std::uint32_t produced = 77;
    const int result = convert(profile, Bytes(source), sourceMax, &consumed, utf8, utf8Max, &produced);
    if (result != 0) Require(consumed == 77 && produced == 77 && std::memcmp(utf8, "\x55\x55\x55\x55", 4) == 0);
    return result;
}

static void RequireString(void* context, const char* mbcs, const char* utf8) {
    std::uint8_t converted[32] = {};
    std::uint32_t read = 0;
    std::uint32_t written = 0;
    Require(sceCesMbcsStrToUtf8Str(context, Bytes(mbcs), Size(mbcs), &read, converted, sizeof(converted), &written) == 0);
    Require(read == Size(mbcs) && written == Size(utf8) && std::memcmp(converted, utf8, written) == 0);
    std::uint8_t back[32] = {};
    Require(sceCesUtf8StrToMbcsStr(context, Bytes(utf8), Size(utf8), &read, back, sizeof(back), &written) == 0);
    Require(read == Size(utf8) && written == Size(mbcs) && std::memcmp(back, mbcs, written) == 0);
}

static int Utf8ToMbcs(void* context, const char* utf8) {
    std::uint8_t mbcs[8] = {};
    std::uint32_t read = 0;
    std::uint32_t written = 0;
    return sceCesUtf8StrToMbcsStr(context, Bytes(utf8), Size(utf8), &read, mbcs, sizeof(mbcs), &written);
}

static std::uint32_t RoundTripMismatches(void* context) {
    std::uint32_t mismatches = 0;
    for (std::uint32_t lead = 0x81; lead < 0xFF; ++lead) {
        for (std::uint32_t trail = 0x40; trail < 0xFF; ++trail) {
            const std::uint8_t source[2] = {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint16_t ucs2[2] = {};
            std::uint32_t read = 0;
            std::uint32_t written = 0;
            if (sceCesMbcsStrToUcs2Str(context, source, sizeof(source), &read, ucs2, 2, &written) != 0) continue;
            Require(read == 2 && written == 1 && ucs2[1] == 0);
            std::uint8_t back[3] = {};
            Require(sceCesUcs2StrToMbcsStr(context, ucs2, 1, &read, back, sizeof(back), &written) == 0);
            Require(read == 1 && written == 2 && back[2] == 0);
            if (std::memcmp(source, back, sizeof(source)) != 0) ++mismatches;
        }
    }
    return mismatches;
}

static std::uint32_t Load(const void* data, std::uint32_t size, int order) {
    if (order == 0) {
        std::uint16_t unit = 0;
        std::uint32_t wide = 0;
        if (size == 2) std::memcpy(&unit, data, 2);
        else std::memcpy(&wide, data, 4);
        return size == 2 ? unit : wide;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint32_t value = 0;
    for (std::uint32_t i = 0; i < size; ++i) value |= static_cast<std::uint32_t>(bytes[i]) << (8 * (order == 1 ? size - 1 - i : i));
    return value;
}

static void Store(void* data, std::uint32_t size, int order, std::uint32_t value) {
    if (order == 0) {
        const auto unit = static_cast<std::uint16_t>(value);
        if (size == 2) std::memcpy(data, &unit, 2);
        else std::memcpy(data, &value, 4);
        return;
    }
    auto* bytes = static_cast<std::uint8_t*>(data);
    for (std::uint32_t i = 0; i < size; ++i) bytes[i] = static_cast<std::uint8_t>(value >> (8 * (order == 1 ? size - 1 - i : i)));
}

static std::uint32_t Utf8Code(const std::uint8_t* utf8, std::uint32_t length) {
    if (length == 1) return utf8[0];
    if (length == 2) return (utf8[0] & 0x1Fu) << 6 | (utf8[1] & 0x3Fu);
    return (utf8[0] & 0x0Fu) << 12 | (utf8[1] & 0x3Fu) << 6 | (utf8[2] & 0x3Fu);
}

static std::uint32_t Utf8Bytes(std::uint32_t code, std::uint8_t* utf8) {
    if (code < 0x80) {
        utf8[0] = static_cast<std::uint8_t>(code);
        return 1;
    }
    const std::uint32_t length = code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
    for (std::uint32_t i = length - 1; i > 0; --i) {
        utf8[i] = static_cast<std::uint8_t>(0x80 | (code & 0x3F));
        code >>= 6;
    }
    utf8[0] = static_cast<std::uint8_t>((length == 2 ? 0xC0 : length == 3 ? 0xE0 : 0xF0) | code);
    return length;
}

static void RequireDecoders(const Decoders& decoders, const void* profile) {
    for (std::uint32_t lead = 0; lead < 256; ++lead) {
        for (std::uint32_t trail = 0; trail < 256; ++trail) {
            const std::uint8_t source[2] = {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint8_t utf8[4] = {};
            std::uint32_t consumed = 0;
            std::uint32_t produced = 0;
            const int expected = decoders.utf8(profile, source, sizeof(source), &consumed, utf8, sizeof(utf8), &produced);
            const std::uint32_t code = expected == 0 ? Utf8Code(utf8, produced) : 0;
            std::uint16_t ucs2 = 0x5555;
            std::uint32_t read = 77;
            Require(decoders.ucs2(profile, source, sizeof(source), &read, &ucs2) == expected);
            Require(expected == 0 ? read == consumed && ucs2 == code : read == 77 && ucs2 == 0x5555);
            for (int order = 0; order < 3; ++order) {
                std::uint16_t utf16 = 0x5555;
                std::uint32_t units = 77;
                read = 77;
                Require(decoders.utf16[order](profile, source, sizeof(source), &read, &utf16, 1, &units) == expected);
                Require(expected == 0 ? read == consumed && units == 1 && Load(&utf16, 2, order) == code : read == 77 && units == 77 && utf16 == 0x5555);
                std::uint32_t utf32 = 0x55555555;
                read = 77;
                Require(decoders.utf32[order](profile, source, sizeof(source), &read, &utf32) == expected);
                Require(expected == 0 ? read == consumed && Load(&utf32, 4, order) == code : read == 77 && utf32 == 0x55555555);
            }
        }
    }
}

struct Encoded {
    std::uint8_t bytes[2] = {0x55, 0x55};
    std::uint32_t size = 77;
    std::uint32_t read = 77;
};

static int Encode(const Encoders& encoders, int form, std::uint32_t code, const void* profile, Encoded& out) {
    std::uint8_t utf8[4] = {};
    std::uint16_t utf16[2] = {};
    std::uint32_t utf32 = 0;
    if (form == 0) return encoders.ucs2(static_cast<std::uint16_t>(code), profile, out.bytes, sizeof(out.bytes), &out.size);
    if (form == 1) return encoders.utf8(utf8, Utf8Bytes(code, utf8), &out.read, profile, out.bytes, sizeof(out.bytes), &out.size);
    if (form <= 4) {
        const int order = form - 2;
        if (code < 0x10000) Store(utf16, 2, order, code);
        else {
            Store(utf16, 2, order, 0xD800 | (code - 0x10000) >> 10);
            Store(utf16 + 1, 2, order, 0xDC00 | (code & 0x3FF));
        }
        return encoders.utf16[order](utf16, code < 0x10000 ? 1 : 2, &out.read, profile, out.bytes, sizeof(out.bytes), &out.size);
    }
    if (form == 5) return encoders.utf32(code, profile, out.bytes, sizeof(out.bytes), &out.size);
    Store(&utf32, 4, form - 5, code);
    return encoders.utf32Unit[form - 5](&utf32, profile, out.bytes, sizeof(out.bytes), &out.size);
}

static std::uint32_t SourceUnits(int form, std::uint32_t code) {
    if (form == 1) return code < 0x80 ? 1 : code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
    if (form >= 2 && form <= 4) return code < 0x10000 ? 1 : 2;
    return 77;
}

static void RequireEncode(const Encoders& encoders, const void* profile, std::uint32_t code, int expected, const std::uint8_t* bytes, std::uint32_t size) {
    for (int form = 0; form < 8; ++form) {
        if (form == 0 && code > 0xFFFF) continue;
        Encoded out;
        Require(Encode(encoders, form, code, profile, out) == expected);
        if (expected == 0) Require(out.read == SourceUnits(form, code) && out.size == size && std::memcmp(out.bytes, bytes, size) == 0);
        else Require(out.read == 77 && out.size == 77 && out.bytes[0] == 0x55 && out.bytes[1] == 0x55);
    }
}

static void RequireEncoders(const Encoders& encoders, const void* profile, void* context) {
    Require(sceCesMbcsUcsContextInit(context, profile) == 0);
    for (std::uint32_t code = 1; code < 0x10000; ++code) {
        if (code >= 0xD800 && code <= 0xDFFF) continue;
        const auto ucs2 = static_cast<std::uint16_t>(code);
        std::uint8_t bytes[3] = {};
        std::uint32_t read = 0;
        std::uint32_t written = 0;
        const int expected = sceCesUcs2StrToMbcsStr(context, &ucs2, 1, &read, bytes, sizeof(bytes), &written);
        RequireEncode(encoders, profile, code, expected, bytes, written);
    }
    RequireEncode(encoders, profile, 0, 0, Bytes("\0"), 1);
    RequireEncode(encoders, profile, 0x20000, unassignedCode, nullptr, 0);
    RequireEncode(encoders, profile, 0x10FFFF, unassignedCode, nullptr, 0);
}

int main() {
    alignas(8) std::uint8_t eucKrSheet[64] = {};
    alignas(8) std::uint8_t uhcSheet[64] = {};
    alignas(8) std::uint8_t big5Sheet[64] = {};
    alignas(8) std::uint8_t eucJpSheet[64] = {};
    Require(sceCesUcsProfileInitEucKr(nullptr) == nullptr);
    Require(sceCesUcsProfileInitUhc(nullptr) == nullptr);
    Require(sceCesUcsProfileInitBig5Cp950(nullptr) == nullptr);
    void* eucKr = sceCesUcsProfileInitEucKr(eucKrSheet);
    void* uhc = sceCesUcsProfileInitUhc(uhcSheet);
    void* big5 = sceCesUcsProfileInitBig5Cp950(big5Sheet);
    void* eucJp = sceCesUcsProfileInitEucJpCp51932(eucJpSheet);
    Require(eucKr == eucKrSheet && uhc == uhcSheet && big5 == big5Sheet && eucJp == eucJpSheet);

    RequireTable(sceCesEucKrToUtf8, eucKr, 0x01FF720E36F4AD67ull, 8226);
    RequireTable(sceCesUhcToUtf8, uhc, 0x3B861A7E911985ABull, 17048);
    RequireTable(sceCesBig5ToUtf8, big5, 0xE4D0D6A8019F35D9ull, 13503);

    RequireScalar(sceCesEucKrToUtf8, eucKr, "\xB0\xA1", "\xEA\xB0\x80");
    RequireScalar(sceCesEucKrToUtf8, eucKr, "\xA2\xE6", "\xE2\x82\xAC");
    RequireScalar(sceCesUhcToUtf8, uhc, "\xB0\xA1", "\xEA\xB0\x80");
    RequireScalar(sceCesUhcToUtf8, uhc, "\x8C\x63", "\xEB\x98\xA0");
    RequireScalar(sceCesBig5ToUtf8, big5, "\xA4\xA4", "\xE4\xB8\xAD");
    RequireScalar(sceCesBig5ToUtf8, big5, "A", "A");
    std::uint8_t zero[4] = {0x55, 0x55, 0x55, 0x55};
    Require(sceCesUhcToUtf8(uhc, Bytes("\0"), 1, nullptr, zero, sizeof(zero), nullptr) == 0 && zero[0] == 0 && zero[1] == 0x55);

    Require(Scalar(sceCesEucKrToUtf8, nullptr, "\xB0\xA1", 2, 4) == invalidParameter);
    Require(Scalar(sceCesEucKrToUtf8, uhc, "\xB0\xA1", 2, 4) == invalidParameter);
    Require(Scalar(sceCesUhcToUtf8, eucKr, "\xB0\xA1", 2, 4) == invalidParameter);
    Require(Scalar(sceCesBig5ToUtf8, eucJp, "\xA4\xA4", 2, 4) == invalidParameter);
    Require(Scalar(sceCesEucKrToUtf8, eucKr, nullptr, 2, 4) == invalidSrcBuffer);
    Require(Scalar(sceCesEucKrToUtf8, eucKr, "\xB0\xA1", 0, 4) == srcBufferEnd);
    Require(Scalar(sceCesUhcToUtf8, uhc, "\x8C\x63", 1, 4) == srcBufferEnd);
    Require(Scalar(sceCesBig5ToUtf8, big5, "\xA4\xA4", 1, 4) == srcBufferEnd);
    Require(Scalar(sceCesEucKrToUtf8, eucKr, "\xB0\xA1", 2, 2) == dstBufferEnd);
    Require(Scalar(sceCesEucKrToUtf8, eucKr, "\x8C\x63", 2, 4) == invalidEncode);
    Require(Scalar(sceCesUhcToUtf8, uhc, "\x80\xA1", 2, 4) == invalidEncode);
    Require(Scalar(sceCesUhcToUtf8, uhc, "\xB0\x5B", 2, 4) == invalidEncode);
    Require(Scalar(sceCesUhcToUtf8, uhc, "\xC9\xA1", 2, 4) == unassignedCode);
    Require(Scalar(sceCesUhcToUtf8, uhc, "\xC7\x41", 2, 4) == unassignedCode);
    Require(Scalar(sceCesBig5ToUtf8, big5, "\xA4\x80", 2, 4) == invalidEncode);
    Require(Scalar(sceCesBig5ToUtf8, big5, "\xFF\xA1", 2, 4) == invalidEncode);
    Require(Scalar(sceCesBig5ToUtf8, big5, "\x81\x40", 2, 4) == unassignedCode);
    Require(Scalar(sceCesBig5ToUtf8, big5, "\xFA\x40", 2, 4) == unassignedCode);
    Require(sceCesEucKrToUtf8(eucKr, Bytes("\xB0\xA1"), 2, nullptr, nullptr, 4, nullptr) == invalidDstBuffer);

    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesMbcsUcsContextInit(context, eucKr) == 0);
    RequireString(context, "\xC7\xD1\xB1\xB9\xBE\xEE A", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4 A");
    Require(Utf8ToMbcs(context, "\xEB\x98\xA0") == unassignedCode);
    Require(RoundTripMismatches(context) == 0);

    Require(sceCesMbcsUcsContextInit(context, uhc) == 0);
    RequireString(context, "\x8C\x63\xC7\xD1", "\xEB\x98\xA0\xED\x95\x9C");
    Require(Utf8ToMbcs(context, "\xE4\xB8\xAD") == 0);
    Require(Utf8ToMbcs(context, "\xE2\x82\xAC\xEF\xBF\xBF") == unassignedCode);
    Require(RoundTripMismatches(context) == 0);

    Require(sceCesMbcsUcsContextInit(context, big5) == 0);
    RequireString(context, "\xA4\xA4\xA4\xE5", "\xE4\xB8\xAD\xE6\x96\x87");
    RequireString(context, "\xF9\xF9\xA2\x7E\xA4\x51\xA4\xCA", "\xE2\x95\x90\xE2\x95\xAD\xE5\x8D\x81\xE5\x8D\x85");
    RequireString(context, "\xF9\xE9\xF9\xEB\xF9\xEA\xA2\xA1\xA2\xA3\xA2\xA2", "\xE2\x95\x9E\xE2\x95\xA1\xE2\x95\xAA\xE2\x95\xAE\xE2\x95\xAF\xE2\x95\xB0");
    Require(Utf8ToMbcs(context, "\xEA\xB0\x80") == unassignedCode);
    Require(RoundTripMismatches(context) == 10);

    RequireDecoders(eucKrDecoders, eucKr);
    RequireDecoders(uhcDecoders, uhc);
    RequireDecoders(big5Decoders, big5);
    RequireEncoders(eucKrEncoders, eucKr, context);
    RequireEncoders(uhcEncoders, uhc, context);
    RequireEncoders(big5Encoders, big5, context);

    const std::uint8_t* ga = Bytes("\xB0\xA1");
    std::uint16_t unit = 0;
    std::uint32_t wide = 0;
    std::uint32_t read = 0;
    std::uint32_t units = 0;
    Require(sceCesEucKrToUtf16be(eucKr, ga, 2, &read, &unit, 1, &units) == 0 && read == 2 && units == 1 && std::memcmp(&unit, "\xAC\x00", 2) == 0);
    Require(sceCesEucKrToUtf16le(eucKr, ga, 2, &read, &unit, 1, &units) == 0 && std::memcmp(&unit, "\x00\xAC", 2) == 0);
    Require(sceCesEucKrToUtf16(eucKr, ga, 2, nullptr, &unit, 1, nullptr) == 0 && unit == 0xAC00);
    Require(sceCesEucKrToUtf32be(eucKr, ga, 2, &read, &wide) == 0 && read == 2 && std::memcmp(&wide, "\x00\x00\xAC\x00", 4) == 0);
    Require(sceCesEucKrToUtf32le(eucKr, ga, 2, &read, &wide) == 0 && std::memcmp(&wide, "\x00\xAC\x00\x00", 4) == 0);
    Require(sceCesEucKrToUtf32(eucKr, ga, 2, nullptr, &wide) == 0 && wide == 0xAC00);
    Require(sceCesEucKrToUcs2(eucKr, Bytes("\0"), 1, &read, &unit) == 0 && read == 1 && unit == 0);
    Require(sceCesUhcToUcs2(uhc, Bytes("\x8C\x63"), 2, nullptr, &unit) == 0 && unit == 0xB620);
    Require(sceCesBig5ToUtf32le(big5, Bytes("\xA4\xA4"), 2, &read, &wide) == 0 && std::memcmp(&wide, "\x2D\x4E\x00\x00", 4) == 0);

    Require(sceCesEucKrToUtf16(uhc, nullptr, 0, nullptr, nullptr, 0, nullptr) == invalidParameter);
    Require(sceCesEucKrToUtf16(eucKr, nullptr, 0, nullptr, nullptr, 0, nullptr) == invalidSrcBuffer);
    Require(sceCesEucKrToUtf16(eucKr, ga, 0, nullptr, nullptr, 0, nullptr) == srcBufferEnd);
    Require(sceCesEucKrToUtf16(eucKr, ga, 2, nullptr, nullptr, 0, nullptr) == invalidDstBuffer);
    Require(sceCesEucKrToUtf16(eucKr, Bytes("\x8C\x63"), 2, nullptr, &unit, 0, nullptr) == invalidEncode);
    unit = 0x5555;
    read = 77;
    units = 77;
    Require(sceCesEucKrToUtf16(eucKr, ga, 2, &read, &unit, 0, &units) == dstBufferEnd && unit == 0x5555 && read == 77 && units == 77);
    Require(sceCesUhcToUcs2(uhc, ga, 2, nullptr, nullptr) == invalidDstBuffer);
    Require(sceCesBig5ToUtf32(big5, Bytes("\xA4\xA4"), 1, nullptr, &wide) == srcBufferEnd);
    Require(sceCesBig5ToUtf32be(eucJp, Bytes("\xA4\xA4"), 2, nullptr, &wide) == invalidParameter);
    Require(sceCesBig5ToUtf32le(big5, Bytes("\xA4\xA4"), 2, nullptr, nullptr) == invalidDstBuffer);

    RequireEncode(eucKrEncoders, eucKr, 0x20AC, 0, Bytes("\xA2\xE6"), 2);
    RequireEncode(eucKrEncoders, eucKr, 0xB620, unassignedCode, nullptr, 0);
    RequireEncode(uhcEncoders, uhc, 0xB620, 0, Bytes("\x8C\x63"), 2);
    RequireEncode(big5Encoders, big5, 0x2550, 0, Bytes("\xF9\xF9"), 2);
    RequireEncode(big5Encoders, big5, 0x5341, 0, Bytes("\xA4\x51"), 2);
    RequireEncode(big5Encoders, big5, 0xAC00, unassignedCode, nullptr, 0);
    RequireEncode(eucKrEncoders, uhc, 0xAC00, invalidParameter, nullptr, 0);
    RequireEncode(uhcEncoders, nullptr, 0xAC00, invalidParameter, nullptr, 0);
    RequireEncode(big5Encoders, eucJp, 0x4E2D, invalidParameter, nullptr, 0);

    std::uint8_t out[2] = {0x55, 0x55};
    std::uint32_t size = 77;
    read = 77;
    const std::uint32_t big = 0x0000AC00;
    Require(sceCesUcs2ToEucKr(0xAC00, uhc, nullptr, 0, nullptr) == invalidParameter);
    Require(sceCesUcs2ToEucKr(0xD800, eucKr, nullptr, 0, nullptr) == invalidDstBuffer);
    Require(sceCesUcs2ToEucKr(0xD800, eucKr, out, 2, &size) == invalidEncode);
    Require(sceCesUtf32ToUhc(0x110000, uhc, out, 2, &size) == invalidEncode);
    Require(sceCesUtf32ToUhc(0xDFFF, uhc, out, 2, &size) == invalidEncode);
    Require(sceCesUtf32beToBig5(nullptr, eucKr, nullptr, 0, nullptr) == invalidParameter);
    Require(sceCesUtf32beToBig5(nullptr, big5, nullptr, 0, nullptr) == invalidSrcBuffer);
    Require(sceCesUtf32leToBig5(&big, big5, nullptr, 0, nullptr) == invalidDstBuffer);
    Require(sceCesUtf8ToEucKr(nullptr, 0, nullptr, uhc, nullptr, 0, nullptr) == invalidParameter);
    Require(sceCesUtf8ToEucKr(nullptr, 0, nullptr, eucKr, nullptr, 0, nullptr) == invalidSrcBuffer);
    Require(sceCesUtf8ToEucKr(Bytes("\xEA\xB0\x80"), 0, nullptr, eucKr, nullptr, 0, nullptr) == srcBufferEnd);
    Require(sceCesUtf8ToEucKr(Bytes("\xEA\xB0\x80"), 3, nullptr, eucKr, nullptr, 0, nullptr) == invalidDstBuffer);
    Require(sceCesUtf8ToEucKr(Bytes("\xEA\xB0\x80"), 2, &read, eucKr, out, 2, &size) == srcBufferEnd);
    Require(sceCesUtf8ToEucKr(Bytes("\xC0\x80"), 2, &read, eucKr, out, 2, &size) == illegalCode);
    Require(sceCesUtf8ToEucKr(Bytes("\xEA\xB0\x80"), 3, &read, eucKr, out, 1, &size) == dstBufferEnd);
    const std::uint16_t lowSurrogate[1] = {0xDC00};
    const std::uint16_t highSurrogate[2] = {0xD800, 0x0041};
    Require(sceCesUtf16ToUhc(lowSurrogate, 1, &read, uhc, out, 2, &size) == invalidEncode);
    Require(sceCesUtf16ToUhc(highSurrogate, 1, &read, uhc, out, 2, &size) == srcBufferEnd);
    Require(sceCesUtf16ToUhc(highSurrogate, 2, &read, uhc, out, 2, &size) == invalidEncode);
    Require(out[0] == 0x55 && out[1] == 0x55 && size == 77 && read == 77);

    std::memcpy(&unit, "\xAC\x00", 2);
    Require(sceCesUtf16beToEucKr(&unit, 1, &read, eucKr, out, 2, &size) == 0 && read == 1 && size == 2 && std::memcmp(out, "\xB0\xA1", 2) == 0);
    std::memcpy(&unit, "\x00\xAC", 2);
    Require(sceCesUtf16leToUhc(&unit, 1, nullptr, uhc, out, 2, nullptr) == 0 && std::memcmp(out, "\xB0\xA1", 2) == 0);
    std::memcpy(&wide, "\x00\x00\x4E\x2D", 4);
    Require(sceCesUtf32beToBig5(&wide, big5, out, 2, &size) == 0 && size == 2 && std::memcmp(out, "\xA4\xA4", 2) == 0);
    std::memcpy(&wide, "\x2D\x4E\x00\x00", 4);
    Require(sceCesUtf32leToBig5(&wide, big5, out, 2, nullptr) == 0 && std::memcmp(out, "\xA4\xA4", 2) == 0);
    Require(sceCesUcs2ToEucKr(0, eucKr, out, 1, &size) == 0 && size == 1 && out[0] == 0);
    return 0;
}
