#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <cstdlib>
#include <cstring>

extern "C" {
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1251(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1250(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_1(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_2(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_4(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_5(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_9(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_10(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_13(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_14(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_15(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_16(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp437(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp737(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp775(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp850(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp852(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp855(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp860(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp861(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp862(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp863(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp865(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp866(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileKoi8R(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileKoi8U(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1254(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1256(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1258(void);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
void* APS5_VABI sceCesUcsProfileInitEucJpCp51932(void*);
int APS5_VABI sceCesUtf8StrToMbcsStr(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8StrGetMbcsLen(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUtf8Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUcs2StrToMbcsStr(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUcs2StrGetMbcsLen(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUcs2Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUcs2Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesEucJpToUcs2(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
int APS5_VABI sceCesEucJpToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf8(const std::uint8_t*, std::uint8_t, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToSbc(const std::uint8_t*, std::uint32_t, std::uint32_t*, const std::uint8_t*, std::uint8_t*);
void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void*);
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

static void RequireSbcRoundTrip(const std::uint8_t* profile, std::initializer_list<int> unassigned) {
    constexpr int unassignedCode = static_cast<int>(0x805C0020);
    for (int sbc = 1; sbc < 256; ++sbc) {
        std::uint8_t utf8[4] = {};
        std::uint32_t length = 7;
        std::uint8_t back = 0;
        if (std::find(unassigned.begin(), unassigned.end(), sbc) != unassigned.end()) {
            Require(sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(sbc), utf8, sizeof(utf8), &length) == unassignedCode && length == 0);
            continue;
        }
        Require(sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(sbc), utf8, sizeof(utf8), &length) == 0);
        Require(sceCesUtf8ToSbc(utf8, length, &length, profile, &back) == 0 && back == sbc);
    }
}

struct MeasuredUtf8 {
    const char* bytes;
    std::uint32_t available;
    int result;
    std::uint32_t length;
};

static void RequireMeasuredUtf8ToSbc(const std::uint8_t* profile) {
    constexpr MeasuredUtf8 cases[] = {
        {"\xE2\x41", 1, static_cast<int>(0x805C0011), 3},
        {"\xE2\x41", 2, static_cast<int>(0x805C0014), 2},
        {"\xE2\x41\xAC", 3, static_cast<int>(0x805C0014), 1},
        {"\xE2\x82\x41", 3, static_cast<int>(0x805C0014), 2},
        {"\xE2\x82", 2, static_cast<int>(0x805C0011), 3},
        {"\xE2\xC0", 2, static_cast<int>(0x805C0014), 2},
        {"\xF0\x41\x98", 3, static_cast<int>(0x805C0014), 2},
        {"\xF0\x9F\x41", 3, static_cast<int>(0x805C0014), 3},
        {"\xF0\x41\x98\x80", 4, static_cast<int>(0x805C0014), 1},
        {"\xF0\x9F\x98\x41", 4, static_cast<int>(0x805C0014), 3},
        {"\xF0\x9F\x98", 3, static_cast<int>(0x805C0011), 4},
        {"\xC3\x41", 2, static_cast<int>(0x805C0014), 1},
        {"\xC3", 1, static_cast<int>(0x805C0011), 2},
        {"\x80", 1, static_cast<int>(0x805C0014), 0},
        {"\xFE", 1, static_cast<int>(0x805C0014), 0},
        {"\xFF", 1, static_cast<int>(0x805C0014), 0},
        {"\xC0\x80", 2, static_cast<int>(0x805C0015), 2},
        {"\xC1\xBF", 2, static_cast<int>(0x805C0015), 2},
        {"\xE0\x80\x80", 3, static_cast<int>(0x805C0015), 3},
        {"\xE0\x9F\xBF", 3, static_cast<int>(0x805C0015), 3},
        {"\xED\xA0\x80", 3, static_cast<int>(0x805C0015), 3},
        {"\xF0\x8F\xBF\xBF", 4, static_cast<int>(0x805C0015), 4},
        {"\xF8\x80\x80\x80\x80", 5, static_cast<int>(0x805C0015), 5},
        {"\xFC\x80\x80\x80\x80\x80", 6, static_cast<int>(0x805C0015), 6},
        {"\xE0\xA0\x80", 3, static_cast<int>(0x805C0020), 3},
        {"\xED\x9F\xBF", 3, static_cast<int>(0x805C0020), 3},
        {"\xEF\xBF\xBF", 3, static_cast<int>(0x805C0020), 3},
        {"\xF0\x90\x80\x80", 4, static_cast<int>(0x805C0020), 4},
        {"\xF4\x90\x80\x80", 4, static_cast<int>(0x805C0020), 4},
        {"\xF5\x80\x80\x80", 4, static_cast<int>(0x805C0020), 4},
        {"\xF7\xBF\xBF\xBF", 4, static_cast<int>(0x805C0020), 4},
        {"\xF8\x88\x80\x80\x80", 4, static_cast<int>(0x805C0011), 5},
        {"\xF8\x88\x80\x80\x80", 5, static_cast<int>(0x805C0020), 5},
        {"\xFB\xBF\xBF\xBF\xBF", 5, static_cast<int>(0x805C0020), 5},
        {"\xFC\x84\x80", 3, static_cast<int>(0x805C0011), 6},
        {"\xFC\x84\x80\x80\x80\x80", 6, static_cast<int>(0x805C0020), 6},
        {"\xFD\xBF\xBF\xBF\xBF\xBF", 6, static_cast<int>(0x805C0020), 6},
        {"\xC2\x80", 2, static_cast<int>(0x805C0020), 2},
        {"\xC2\x81", 2, static_cast<int>(0x805C0020), 2},
        {"\xC2\x8D", 2, static_cast<int>(0x805C0020), 2},
        {"\xC2\xA0", 2, 0, 2},
        {"\xC5\x92", 2, 0, 2},
    };
    for (const MeasuredUtf8& measured : cases) {
        std::uint32_t length = 0xAAAAAAAA;
        std::uint8_t sbc = 0xBB;
        Require(sceCesUtf8ToSbc(reinterpret_cast<const std::uint8_t*>(measured.bytes), measured.available, &length, profile, &sbc) == measured.result);
        Require(length == measured.length);
        Require(measured.result != 0 ? sbc == 0 : sbc != 0);
    }
}

constexpr std::uint32_t MeasuredEucJpPairs = 0x9295D4B1;
constexpr std::uint32_t MeasuredEucJpSupplementary = 0x75256134;

struct MeasuredString {
    const char* source;
    std::uint32_t available;
    std::uint32_t capacity;
    int result;
    std::uint32_t read;
    std::uint32_t written;
    const char* output;
};

static void RequireMeasuredStrings(void* latin, void* eucJp) {
    constexpr int dstBufferEnd = static_cast<int>(0x805C0031);
    constexpr int invalidEncode = static_cast<int>(0x805C0014);
    constexpr int unassignedCode = static_cast<int>(0x805C0020);
    constexpr MeasuredString toUtf8[] = {
        {"ABC", 3, 16, 0, 3, 3, "ABC"},
        {"ABC\0D", 5, 16, 0, 3, 3, "ABC"},
        {"ABC", 0, 16, 0, 3, 3, "ABC"},
        {"ABC", 3, 4, 0, 3, 3, "ABC"},
        {"ABC", 3, 3, dstBufferEnd, 2, 2, "AB"},
        {"ABC", 3, 2, dstBufferEnd, 1, 1, "A"},
        {"A\xE9" "B", 3, 16, 0, 3, 4, "A\xC3\xA9" "B"},
        {"A\xE9" "B", 3, 3, dstBufferEnd, 1, 1, "A"},
        {"\x80\x80\x80", 3, 7, dstBufferEnd, 2, 6, "\xE2\x82\xAC\xE2\x82\xAC"},
        {"A\x81" "B", 3, 16, unassignedCode, 1, 1, "A"},
        {"", 2, 16, 0, 0, 0, ""},
    };
    for (const MeasuredString& measured : toUtf8) {
        std::uint8_t output[16];
        std::memset(output, 0xBB, sizeof(output));
        std::uint32_t read = 0xAAAAAAAA;
        std::uint32_t written = 0xCCCCCCCC;
        Require(sceCesMbcsStrToUtf8Str(latin, reinterpret_cast<const std::uint8_t*>(measured.source), measured.available, &read, output, measured.capacity, &written) == measured.result);
        Require(read == measured.read && written == measured.written);
        Require(std::memcmp(output, measured.output, measured.written + 1) == 0 && output[measured.written + 1] == 0xBB);
        Require(sceCesMbcsStrGetUtf8Len(latin, reinterpret_cast<const std::uint8_t*>(measured.source), measured.available, &read, &written) == (measured.result == dstBufferEnd ? 0 : measured.result));
    }
    constexpr MeasuredString fromUtf8[] = {
        {"ABC", 3, 3, dstBufferEnd, 2, 2, "AB"},
        {"A\xC3\xA9" "B", 4, 16, 0, 4, 3, "A\xE9" "B"},
        {"A\xE4\xB8\x80" "B", 5, 16, unassignedCode, 1, 1, "A"},
        {"A\xC3", 2, 16, static_cast<int>(0x805C0011), 1, 1, "A"},
        {"A\xFF" "B", 3, 16, invalidEncode, 1, 1, "A"},
        {"A\xC3\0B", 0, 16, invalidEncode, 1, 1, "A"},
        {"A\xC0\x80" "B", 4, 16, static_cast<int>(0x805C0015), 1, 1, "A"},
        {"A\xED\xA0\x80" "B", 5, 16, static_cast<int>(0x805C0015), 1, 1, "A"},
        {"A\xC2\x81" "B", 4, 16, unassignedCode, 1, 1, "A"},
        {"A\xF8\x88\x80\x80\x80" "B", 7, 16, unassignedCode, 1, 1, "A"},
        {"A\xE2\x41\xAC" "B", 5, 16, invalidEncode, 1, 1, "A"},
        {"ABC", 0, 16, 0, 3, 3, "ABC"},
    };
    for (const MeasuredString& measured : fromUtf8) {
        std::uint8_t output[16];
        std::memset(output, 0xBB, sizeof(output));
        std::uint32_t read = 0xAAAAAAAA;
        std::uint32_t written = 0xCCCCCCCC;
        Require(sceCesUtf8StrToMbcsStr(latin, reinterpret_cast<const std::uint8_t*>(measured.source), measured.available, &read, output, measured.capacity, &written) == measured.result);
        Require(read == measured.read && written == measured.written);
        Require(std::memcmp(output, measured.output, measured.written + 1) == 0 && output[measured.written + 1] == 0xBB);
    }
    constexpr MeasuredString eucToUtf8[] = {
        {"A\xA4\xA2" "B", 0, 16, 0, 4, 5, "A\xE3\x81\x82" "B"},
        {"A\xA4\xA2" "B", 0, 4, dstBufferEnd, 1, 1, "A"},
        {"A\xA4\xA2" "B", 0, 5, dstBufferEnd, 3, 4, "A\xE3\x81\x82"},
        {"A\xA4", 2, 16, static_cast<int>(0x805C0011), 1, 1, "A"},
        {"A\x0E" "B", 3, 16, invalidEncode, 1, 1, "A"},
        {"A\x80" "B", 0, 16, 0, 3, 4, "A\xC2\x80" "B"},
        {"A\xA4\0B", 0, 16, invalidEncode, 1, 1, "A"},
    };
    for (const MeasuredString& measured : eucToUtf8) {
        std::uint8_t output[16];
        std::memset(output, 0xBB, sizeof(output));
        std::uint32_t read = 0xAAAAAAAA;
        std::uint32_t written = 0xCCCCCCCC;
        Require(sceCesMbcsStrToUtf8Str(eucJp, reinterpret_cast<const std::uint8_t*>(measured.source), measured.available, &read, output, measured.capacity, &written) == measured.result);
        Require(read == measured.read && written == measured.written);
        Require(std::memcmp(output, measured.output, measured.written + 1) == 0 && output[measured.written + 1] == 0xBB);
    }

    const auto* letters = reinterpret_cast<const std::uint8_t*>("ABC");
    std::uint8_t output[4] = {0xBB, 0xBB, 0xBB, 0xBB};
    std::uint32_t read = 7;
    std::uint32_t written = 7;
    Require(sceCesMbcsStrToUtf8Str(latin, letters, 3, &read, output, 0, &written) == dstBufferEnd && read == 0 && written == 0 && output[0] == 0xBB);
    Require(sceCesMbcsStrToUtf8Str(latin, letters, 3, &read, nullptr, 16, &written) == static_cast<int>(0x805C0030) && read == 0 && written == 0);
    Require(sceCesMbcsStrToUtf8Str(nullptr, letters, 3, &read, nullptr, 16, &written) == static_cast<int>(0x805C0001));
    Require(sceCesMbcsStrToUtf8Str(latin, nullptr, 3, &read, nullptr, 16, &written) == static_cast<int>(0x805C0010));
    Require(sceCesMbcsStrToUtf8Str(nullptr, letters, 3, &read, output, sizeof(output), &written) == static_cast<int>(0x805C0001) && output[0] == 0 && output[1] == 0xBB);
    output[0] = 0xBB;
    Require(sceCesMbcsStrToUtf8Str(latin, nullptr, 3, &read, output, sizeof(output), &written) == static_cast<int>(0x805C0010) && output[0] == 0 && read == 0 && written == 0);
    output[0] = 0xBB;
    Require(sceCesMbcsStrToUtf8Str(latin, nullptr, 3, &read, output, 0, &written) == static_cast<int>(0x805C0010) && output[0] == 0xBB);
    Require(sceCesMbcsStrGetUtf8Len(latin, reinterpret_cast<const std::uint8_t*>("A\x81" "B"), 3, &read, &written) == unassignedCode && read == 1 && written == 1);
    Require(sceCesMbcsStrGetUtf8Len(latin, nullptr, 3, &read, &written) == static_cast<int>(0x805C0010) && read == 0 && written == 0);
    Require(sceCesUtf8StrGetMbcsLen(latin, reinterpret_cast<const std::uint8_t*>("A\xE4\xB8\x80" "B"), 5, &read, &written) == unassignedCode && read == 1 && written == 1);
    Require(sceCesUtf8StrGetMbcsLen(latin, reinterpret_cast<const std::uint8_t*>("A\xC3"), 2, &read, &written) == static_cast<int>(0x805C0011) && read == 1 && written == 1);

    std::uint16_t wide[8];
    for (std::uint16_t& unit : wide) unit = 0xBBBB;
    Require(sceCesMbcsStrToUcs2Str(latin, letters, 0, &read, wide, 3, &written) == dstBufferEnd && read == 2 && written == 2);
    Require(wide[0] == u'A' && wide[1] == u'B' && wide[2] == 0 && wide[3] == 0xBBBB);
    Require(sceCesMbcsStrToUcs2Str(latin, reinterpret_cast<const std::uint8_t*>("A\x81" "B"), 3, &read, wide, 8, &written) == unassignedCode && read == 1 && written == 1 && wide[1] == 0);
    const std::uint16_t han[] = {u'A', 0x4E00, u'B', 0};
    const std::uint16_t surrogate[] = {u'A', 0xD800, u'B', 0};
    const std::uint16_t control[] = {u'A', 0x0081, u'B', 0};
    const std::uint16_t plain[] = {u'A', u'B', u'C', 0};
    std::memset(output, 0xBB, sizeof(output));
    Require(sceCesUcs2StrToMbcsStr(latin, plain, 2, &read, output, sizeof(output), &written) == 0 && read == 2 && written == 2 && std::memcmp(output, "AB", 3) == 0);
    Require(sceCesUcs2StrToMbcsStr(latin, plain, 0, &read, output, 3, &written) == dstBufferEnd && read == 2 && written == 2);
    Require(sceCesUcs2StrToMbcsStr(latin, han, 0, &read, output, sizeof(output), &written) == unassignedCode && read == 1 && written == 1);
    Require(sceCesUcs2StrToMbcsStr(latin, surrogate, 0, &read, output, sizeof(output), &written) == static_cast<int>(0x805C0015) && read == 1 && written == 1);
    Require(sceCesUcs2StrToMbcsStr(latin, control, 0, &read, output, sizeof(output), &written) == unassignedCode && read == 1 && written == 1);
    Require(sceCesUcs2StrGetMbcsLen(latin, han, 0, &read, &written) == unassignedCode && read == 1 && written == 1);
}

static std::uint32_t Fnv(std::uint32_t hash, std::uint32_t word) {
    for (int k = 0; k < 4; ++k) {
        hash ^= (word >> (8 * k)) & 0xFFu;
        hash *= 16777619u;
    }
    return hash;
}

constexpr std::uint32_t MeasuredEucJpEncoder = 0x3AF3CD39;
constexpr std::uint32_t MeasuredSjisEncoder = 0x2FB8906B;

static std::uint32_t EncoderSweep(void* context) {
    std::uint32_t hash = 2166136261u;
    for (std::uint32_t value = 0; value <= 0xFFFF; ++value) {
        const std::uint16_t source[1] = {static_cast<std::uint16_t>(value)};
        std::uint8_t output[4] = {0xBB, 0xBB, 0xBB, 0xBB};
        std::uint32_t used = 0xAAAAAAAA;
        std::uint32_t produced = 0xCCCCCCCC;
        const int result = sceCesUcs2StrToMbcsStr(context, source, 1, &used, output, sizeof(output), &produced);
        std::uint32_t bytes = 0;
        std::memcpy(&bytes, output, sizeof(bytes));
        hash = Fnv(Fnv(Fnv(Fnv(hash, static_cast<std::uint32_t>(result)), produced), bytes), used);
    }
    return hash;
}

constexpr std::uint32_t MeasuredSjisDecoder = 0xCA99178C;

static std::uint32_t SjisDecoderSweep(void* sjis) {
    std::uint32_t hash = 2166136261u;
    for (std::uint32_t lead = 0; lead < 256; ++lead) {
        for (std::uint32_t trail = 0; trail < 256; ++trail) {
            const std::uint8_t source[2] = {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint16_t output[3] = {0xBBBB, 0xBBBB, 0xBBBB};
            std::uint32_t used = 0xAAAAAAAA;
            std::uint32_t produced = 0xCCCCCCCC;
            const int result = sceCesMbcsStrToUcs2Str(sjis, source, 2, &used, output, 3, &produced);
            hash = Fnv(Fnv(Fnv(Fnv(Fnv(hash, static_cast<std::uint32_t>(result)), used), produced), static_cast<std::uint32_t>(output[0]) | static_cast<std::uint32_t>(output[1]) << 16), output[2]);
        }
    }
    return hash;
}

static void RequireMeasuredEncoders(void* eucJp, void* sjis) {
    Require(SjisDecoderSweep(sjis) == MeasuredSjisDecoder);
    Require(EncoderSweep(eucJp) == MeasuredEucJpEncoder);
    Require(EncoderSweep(sjis) == MeasuredSjisEncoder);
    const std::uint16_t aliases[] = {0x00A2, 0x00A3, 0x00AC, 0x2014, 0x2016, 0x203E, 0x2212, 0x301C, 0xFEFF, 0x0080, 0};
    std::uint8_t output[32] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesUcs2StrToMbcsStr(eucJp, aliases, 0, &used, output, sizeof(output), &produced) == 0 && used == 10 && produced == 17);
    Require(std::memcmp(output, "\xA1\xF1\xA1\xF2\xA2\xCC\xA1\xBD\xA1\xC2\xA1\xB1\xA1\xDD\xA1\xC1\x80", 18) == 0);
    Require(sceCesUcs2StrToMbcsStr(sjis, aliases, 0, &used, output, sizeof(output), &produced) == static_cast<int>(0x805C0024) && used == 9 && produced == 16);
    Require(std::memcmp(output, "\x81\x91\x81\x92\x81\xCA\x81\x5C\x81\x61\x81\x50\x81\x7C\x81\x60", 17) == 0);
    const std::uint16_t shift[] = {u'A', 0x000E, 0};
    Require(sceCesUcs2StrToMbcsStr(sjis, shift, 0, &used, output, sizeof(output), &produced) == 0 && produced == 2);
    Require(sceCesUcs2StrToMbcsStr(eucJp, shift, 0, &used, output, sizeof(output), &produced) == static_cast<int>(0x805C0020) && used == 1 && produced == 1);
}

static void RequireMeasuredEucJp(void* eucJp) {
    constexpr int invalidProfile = static_cast<int>(0x805C0004);
    constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
    constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
    constexpr int invalidEncode = static_cast<int>(0x805C0014);
    constexpr int unassignedCode = static_cast<int>(0x805C0020);
    constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
    constexpr int dstBufferEnd = static_cast<int>(0x805C0031);
    std::uint32_t pairs = 2166136261u;
    for (int lead = 0; lead < 256; ++lead) {
        for (int trail = 0; trail < 256; ++trail) {
            const std::uint8_t cell[2] = {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint32_t used = 0xAAAAAAAA;
            std::uint16_t code = 0xBBBB;
            const int result = sceCesEucJpToUcs2(eucJp, cell, 2, &used, &code);
            pairs = Fnv(Fnv(Fnv(Fnv(pairs, static_cast<std::uint32_t>(cell[1])), static_cast<std::uint32_t>(result)), used), code);
        }
    }
    Require(pairs == MeasuredEucJpPairs);
    std::uint32_t supplementary = 2166136261u;
    for (int second = 0; second < 256; ++second) {
        for (int third = 0; third < 256; ++third) {
            const std::uint8_t cell[3] = {0x8F, static_cast<std::uint8_t>(second), static_cast<std::uint8_t>(third)};
            std::uint32_t used = 0xAAAAAAAA;
            std::uint16_t code = 0xBBBB;
            const int result = sceCesEucJpToUcs2(eucJp, cell, 3, &used, &code);
            supplementary = Fnv(Fnv(Fnv(Fnv(supplementary, static_cast<std::uint32_t>(third)), static_cast<std::uint32_t>(result)), used), code);
        }
    }
    Require(supplementary == MeasuredEucJpSupplementary);

    const auto* hiragana = reinterpret_cast<const std::uint8_t*>("\xA4\xA2");
    std::uint32_t used = 7;
    std::uint32_t produced = 7;
    std::uint16_t code = 0xBBBB;
    std::uint8_t utf8[4] = {0xBB, 0xBB, 0xBB, 0xBB};
    Require(sceCesEucJpToUcs2(nullptr, hiragana, 2, &used, &code) == invalidProfile && used == 0 && code == 0);
    Require(sceCesEucJpToUcs2(eucJp, nullptr, 2, &used, &code) == invalidSrcBuffer && used == 0);
    Require(sceCesEucJpToUcs2(eucJp, hiragana, 0, &used, nullptr) == invalidSrcBuffer && used == 0);
    Require(sceCesEucJpToUcs2(eucJp, hiragana, 1, &used, nullptr) == srcBufferEnd && used == 2);
    Require(sceCesEucJpToUcs2(eucJp, hiragana, 2, &used, nullptr) == invalidDstBuffer && used == 2);
    Require(sceCesEucJpToUcs2(eucJp, hiragana, 2, nullptr, &code) == 0 && code == 0x3042);
    Require(sceCesEucJpToUtf8(eucJp, hiragana, 2, &used, utf8, sizeof(utf8), &produced) == 0 && used == 2 && produced == 3 && std::memcmp(utf8, "\xE3\x81\x82", 3) == 0);
    Require(sceCesEucJpToUtf8(eucJp, reinterpret_cast<const std::uint8_t*>("\x8E\xB1"), 2, &used, utf8, sizeof(utf8), &produced) == 0 && used == 2 && produced == 3 && std::memcmp(utf8, "\xEF\xBD\xB1", 3) == 0);
    Require(sceCesEucJpToUtf8(eucJp, reinterpret_cast<const std::uint8_t*>("\x80"), 1, &used, utf8, sizeof(utf8), &produced) == 0 && used == 1 && produced == 2 && std::memcmp(utf8, "\xC2\x80", 2) == 0);
    std::memset(utf8, 0xBB, sizeof(utf8));
    Require(sceCesEucJpToUtf8(nullptr, nullptr, 2, &used, utf8, sizeof(utf8), &produced) == invalidProfile && used == 0 && produced == 0);
    Require(sceCesEucJpToUtf8(eucJp, nullptr, 0, &used, utf8, sizeof(utf8), &produced) == invalidSrcBuffer && used == 0 && produced == 0);
    Require(sceCesEucJpToUtf8(eucJp, hiragana, 1, &used, nullptr, sizeof(utf8), &produced) == srcBufferEnd && used == 2 && produced == 0);
    Require(sceCesEucJpToUtf8(eucJp, reinterpret_cast<const std::uint8_t*>("\xA4\x20"), 2, &used, utf8, sizeof(utf8), &produced) == invalidEncode && used == 2 && produced == 0);
    Require(sceCesEucJpToUtf8(eucJp, reinterpret_cast<const std::uint8_t*>("\x8E\xE0"), 2, &used, utf8, sizeof(utf8), &produced) == unassignedCode && used == 2 && produced == 0);
    Require(sceCesEucJpToUtf8(eucJp, reinterpret_cast<const std::uint8_t*>("\xA9\xA1"), 2, &used, utf8, sizeof(utf8), &produced) == unassignedCode && used == 2 && produced == 0);
    Require(sceCesEucJpToUtf8(eucJp, reinterpret_cast<const std::uint8_t*>("\xFF\xA1"), 2, &used, utf8, sizeof(utf8), &produced) == invalidEncode && used == 1 && produced == 0);
    Require(sceCesEucJpToUtf8(eucJp, hiragana, 2, &used, nullptr, sizeof(utf8), &produced) == invalidDstBuffer && used == 2 && produced == 3);
    Require(sceCesEucJpToUtf8(eucJp, hiragana, 2, &used, utf8, 2, &produced) == dstBufferEnd && used == 2 && produced == 3);
    Require(sceCesEucJpToUtf8(eucJp, hiragana, 2, &used, utf8, 0, &produced) == dstBufferEnd && used == 2 && produced == 3);
    Require(utf8[0] == 0xBB);
    Require(sceCesEucJpToUtf8(eucJp, hiragana, 2, nullptr, utf8, sizeof(utf8), nullptr) == 0 && std::memcmp(utf8, "\xE3\x81\x82", 3) == 0);
}

static void RequireSbcCode(const std::uint8_t* profile, std::uint8_t sbc, std::uint32_t code) {
    char expected[4] = {};
    if (code < 0x80) {
        expected[0] = static_cast<char>(code);
    } else if (code < 0x800) {
        expected[0] = static_cast<char>(0xC0 | (code >> 6));
        expected[1] = static_cast<char>(0x80 | (code & 0x3F));
    } else {
        expected[0] = static_cast<char>(0xE0 | (code >> 12));
        expected[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        expected[2] = static_cast<char>(0x80 | (code & 0x3F));
    }
    RequireSbcToUtf8(profile, sbc, expected);
}

static void CheckSingleByteProfiles() {
    constexpr int unassignedCode = static_cast<int>(0x805C0020);
    const auto getters = {sceCesRefersUcsProfileIso8859_1, sceCesRefersUcsProfileIso8859_2, sceCesRefersUcsProfileIso8859_4, sceCesRefersUcsProfileIso8859_5, sceCesRefersUcsProfileIso8859_9, sceCesRefersUcsProfileIso8859_10, sceCesRefersUcsProfileIso8859_13, sceCesRefersUcsProfileIso8859_14, sceCesRefersUcsProfileIso8859_15, sceCesRefersUcsProfileIso8859_16, sceCesRefersUcsProfileCp437, sceCesRefersUcsProfileCp737, sceCesRefersUcsProfileCp775, sceCesRefersUcsProfileCp850, sceCesRefersUcsProfileCp852, sceCesRefersUcsProfileCp855, sceCesRefersUcsProfileCp860, sceCesRefersUcsProfileCp861, sceCesRefersUcsProfileCp862, sceCesRefersUcsProfileCp863, sceCesRefersUcsProfileCp865, sceCesRefersUcsProfileCp866, sceCesRefersUcsProfileKoi8R, sceCesRefersUcsProfileKoi8U, sceCesRefersUcsProfileCp1254, sceCesRefersUcsProfileCp1256, sceCesRefersUcsProfileCp1258};
    const std::uint8_t* existing[] = {sceCesRefersUcsProfileCp1252(), sceCesRefersUcsProfileCp1251(), sceCesRefersUcsProfileCp1250()};
    std::size_t index = 0;
    for (const auto getter : getters) {
        const std::uint8_t* profile = getter();
        Require(profile != nullptr && profile == getter());
        for (const std::uint8_t* other : existing) Require(profile != other);
        std::size_t inner = 0;
        for (const auto earlier : getters) {
            if (inner++ == index) break;
            Require(profile != earlier());
        }
        ++index;
        for (int sbc = 1; sbc < 256; ++sbc) {
            std::uint8_t cp[4] = {};
            std::uint32_t cpLength = 0;
            std::uint8_t back = 0;
            Require(sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(sbc), cp, sizeof(cp), &cpLength) == 0);
            if (sbc < 0x80) Require(cpLength == 1 && cp[0] == sbc);
            Require(sceCesUtf8ToSbc(cp, cpLength, &cpLength, profile, &back) == 0 && back == sbc);
        }
        Require(Utf8ToSbc(profile, "\xE3\x81\x82", 3) == unassignedCode);
    }
    struct Check {
        decltype(&sceCesRefersUcsProfileIso8859_1) profile;
        std::uint8_t sbc;
        std::uint32_t code;
    };
    const Check checks[] = {
        {sceCesRefersUcsProfileIso8859_1, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_1, 0xA4, 0x00A4},
        {sceCesRefersUcsProfileIso8859_1, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileIso8859_1, 0xFF, 0x00FF},
        {sceCesRefersUcsProfileIso8859_2, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_2, 0xA4, 0x00A4},
        {sceCesRefersUcsProfileIso8859_2, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileIso8859_2, 0xFF, 0x02D9},
        {sceCesRefersUcsProfileIso8859_4, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_4, 0xA4, 0x00A4},
        {sceCesRefersUcsProfileIso8859_4, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileIso8859_4, 0xFF, 0x02D9},
        {sceCesRefersUcsProfileIso8859_5, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_5, 0xA4, 0x0404},
        {sceCesRefersUcsProfileIso8859_5, 0xC1, 0x0421},
        {sceCesRefersUcsProfileIso8859_5, 0xFF, 0x045F},
        {sceCesRefersUcsProfileIso8859_9, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_9, 0xA4, 0x00A4},
        {sceCesRefersUcsProfileIso8859_9, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileIso8859_9, 0xFF, 0x00FF},
        {sceCesRefersUcsProfileIso8859_10, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_10, 0xA4, 0x012A},
        {sceCesRefersUcsProfileIso8859_10, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileIso8859_10, 0xFF, 0x0138},
        {sceCesRefersUcsProfileIso8859_13, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_13, 0xA4, 0x00A4},
        {sceCesRefersUcsProfileIso8859_13, 0xC1, 0x012E},
        {sceCesRefersUcsProfileIso8859_13, 0xFF, 0x2019},
        {sceCesRefersUcsProfileIso8859_14, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_14, 0xA4, 0x010A},
        {sceCesRefersUcsProfileIso8859_14, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileIso8859_14, 0xFF, 0x00FF},
        {sceCesRefersUcsProfileIso8859_15, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_15, 0xA4, 0x20AC},
        {sceCesRefersUcsProfileIso8859_15, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileIso8859_15, 0xFF, 0x00FF},
        {sceCesRefersUcsProfileIso8859_16, 0x80, 0x0080},
        {sceCesRefersUcsProfileIso8859_16, 0xA4, 0x20AC},
        {sceCesRefersUcsProfileIso8859_16, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileIso8859_16, 0xFF, 0x00FF},
        {sceCesRefersUcsProfileCp437, 0x80, 0x00C7},
        {sceCesRefersUcsProfileCp437, 0xA4, 0x00F1},
        {sceCesRefersUcsProfileCp437, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp437, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp737, 0x80, 0x0391},
        {sceCesRefersUcsProfileCp737, 0xA4, 0x03BD},
        {sceCesRefersUcsProfileCp737, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp737, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp775, 0x80, 0x0106},
        {sceCesRefersUcsProfileCp775, 0xA4, 0x017C},
        {sceCesRefersUcsProfileCp775, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp775, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp850, 0x80, 0x00C7},
        {sceCesRefersUcsProfileCp850, 0xA4, 0x00F1},
        {sceCesRefersUcsProfileCp850, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp850, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp852, 0x80, 0x00C7},
        {sceCesRefersUcsProfileCp852, 0xA4, 0x0104},
        {sceCesRefersUcsProfileCp852, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp852, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp855, 0x80, 0x0452},
        {sceCesRefersUcsProfileCp855, 0xA4, 0x0446},
        {sceCesRefersUcsProfileCp855, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp855, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp860, 0x80, 0x00C7},
        {sceCesRefersUcsProfileCp860, 0xA4, 0x00F1},
        {sceCesRefersUcsProfileCp860, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp860, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp861, 0x80, 0x00C7},
        {sceCesRefersUcsProfileCp861, 0xA4, 0x00C1},
        {sceCesRefersUcsProfileCp861, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp861, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp862, 0x80, 0x05D0},
        {sceCesRefersUcsProfileCp862, 0xA4, 0x00F1},
        {sceCesRefersUcsProfileCp862, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp862, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp863, 0x80, 0x00C7},
        {sceCesRefersUcsProfileCp863, 0xA4, 0x00A8},
        {sceCesRefersUcsProfileCp863, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp863, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp865, 0x80, 0x00C7},
        {sceCesRefersUcsProfileCp865, 0xA4, 0x00F1},
        {sceCesRefersUcsProfileCp865, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp865, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileCp866, 0x80, 0x0410},
        {sceCesRefersUcsProfileCp866, 0xA4, 0x0434},
        {sceCesRefersUcsProfileCp866, 0xC1, 0x2534},
        {sceCesRefersUcsProfileCp866, 0xFF, 0x00A0},
        {sceCesRefersUcsProfileKoi8R, 0x80, 0x2500},
        {sceCesRefersUcsProfileKoi8R, 0xA4, 0x2553},
        {sceCesRefersUcsProfileKoi8R, 0xC1, 0x0430},
        {sceCesRefersUcsProfileKoi8R, 0xFF, 0x042A},
        {sceCesRefersUcsProfileKoi8U, 0x80, 0x2500},
        {sceCesRefersUcsProfileKoi8U, 0xA4, 0x0454},
        {sceCesRefersUcsProfileKoi8U, 0xC1, 0x0430},
        {sceCesRefersUcsProfileKoi8U, 0xFF, 0x042A},
        {sceCesRefersUcsProfileCp1254, 0x80, 0x20AC},
        {sceCesRefersUcsProfileCp1254, 0xA4, 0x00A4},
        {sceCesRefersUcsProfileCp1254, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileCp1254, 0xFF, 0x00FF},
        {sceCesRefersUcsProfileCp1256, 0x80, 0x20AC},
        {sceCesRefersUcsProfileCp1256, 0xA4, 0x00A4},
        {sceCesRefersUcsProfileCp1256, 0xC1, 0x0621},
        {sceCesRefersUcsProfileCp1256, 0xFF, 0x06D2},
        {sceCesRefersUcsProfileCp1258, 0x80, 0x20AC},
        {sceCesRefersUcsProfileCp1258, 0xA4, 0x00A4},
        {sceCesRefersUcsProfileCp1258, 0xC1, 0x00C1},
        {sceCesRefersUcsProfileCp1258, 0xFF, 0x00FF},
    };
    for (const Check& check : checks) RequireSbcCode(check.profile(), check.sbc, check.code);

    char context[256] = {};
    Require(sceCesMbcsUcsContextInit(context, sceCesRefersUcsProfileKoi8R()) == 0);
    std::uint8_t koi8[16] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesUtf8StrToMbcsStr(context, reinterpret_cast<const std::uint8_t*>("\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"), 12, &used, koi8, sizeof(koi8), &produced) == 0);
    Require(used == 12 && produced == 6 && std::memcmp(koi8, "\xF0\xD2\xC9\xD7\xC5\xD4", 6) == 0);
    std::uint8_t utf8[32] = {};
    Require(sceCesMbcsStrToUtf8Str(context, koi8, 6, &used, utf8, sizeof(utf8), &produced) == 0);
    Require(used == 6 && produced == 12 && std::memcmp(utf8, "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", 12) == 0);
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
    CheckSingleByteProfiles();

    RequireSbcToUtf8(profile, 'A', "A");
    RequireSbcToUtf8(profile, 0x80, "\xE2\x82\xAC");
    RequireSbcToUtf8(profile, 0x9F, "\xC5\xB8");
    RequireSbcToUtf8(profile, 0xE9, "\xC3\xA9");
    RequireSbcToUtf8(profile, 0xFF, "\xC3\xBF");

    RequireUtf8ToSbc(profile, "A", 'A');
    RequireUtf8ToSbc(profile, "\xE2\x82\xAC", 0x80);
    RequireUtf8ToSbc(profile, "\xE2\x84\xA2", 0x99);
    RequireUtf8ToSbc(profile, "\xC3\xA9", 0xE9);
    RequireSbcRoundTrip(profile, {0x81, 0x8D, 0x8F, 0x90, 0x9D});
    RequireMeasuredUtf8ToSbc(profile);

    std::uint8_t utf8[4] = {};
    std::uint32_t length = 0;
    std::uint8_t sbc = 0;
    constexpr int invalidProfile = static_cast<int>(0x805C0004);
    constexpr int illegalCode = static_cast<int>(0x805C0015);
    length = 7;
    Require(sceCesSbcToUtf8(nullptr, 'A', utf8, sizeof(utf8), &length) == invalidProfile && length == 0);
    Require(sceCesSbcToUtf8(nullptr, 0x81, utf8, sizeof(utf8), &length) == invalidProfile && length == 0);
    Require(sceCesSbcToUtf8(profile, 0x80, nullptr, sizeof(utf8), &length) == invalidDstBuffer && length == 3);
    Require(sceCesSbcToUtf8(profile, 0x81, nullptr, sizeof(utf8), &length) == unassignedCode && length == 0);
    Require(sceCesSbcToUtf8(profile, 0x81, utf8, 0, &length) == unassignedCode && length == 0);
    utf8[0] = 0xBB;
    Require(sceCesSbcToUtf8(profile, 0x80, utf8, 2, &length) == dstBufferEnd && length == 3 && utf8[0] == 0xBB);
    Require(sceCesSbcToUtf8(profile, 'A', utf8, 0, &length) == dstBufferEnd && length == 1 && utf8[0] == 0xBB);
    Require(sceCesSbcToUtf8(profile, 0xE9, utf8, 1, nullptr) == dstBufferEnd);
    const auto* euro = reinterpret_cast<const std::uint8_t*>("\xE2\x82\xAC");
    const auto* han = reinterpret_cast<const std::uint8_t*>("\xE4\xB8\x80");
    const auto* letter = reinterpret_cast<const std::uint8_t*>("A");
    sbc = 0xBB;
    Require(sceCesUtf8ToSbc(letter, 1, &length, nullptr, &sbc) == invalidProfile && length == 1 && sbc == 0);
    Require(sceCesUtf8ToSbc(letter, 1, &length, nullptr, nullptr) == invalidProfile && length == 1);
    Require(sceCesUtf8ToSbc(euro, 3, &length, nullptr, &sbc) == invalidProfile && length == 3);
    Require(sceCesUtf8ToSbc(euro, 2, &length, nullptr, &sbc) == srcBufferEnd && length == 3);
    Require(sceCesUtf8ToSbc(reinterpret_cast<const std::uint8_t*>("\xFF"), 1, &length, nullptr, &sbc) == invalidEncode && length == 0);
    sbc = 0xBB;
    Require(sceCesUtf8ToSbc(nullptr, 1, &length, nullptr, &sbc) == invalidSrcBuffer && length == 0 && sbc == 0);
    Require(sceCesUtf8ToSbc(nullptr, 1, &length, profile, nullptr) == invalidSrcBuffer && length == 0);
    sbc = 0xBB;
    Require(sceCesUtf8ToSbc(letter, 0, &length, profile, &sbc) == invalidSrcBuffer && length == 0 && sbc == 0);
    Require(sceCesUtf8ToSbc(euro, 3, &length, profile, nullptr) == invalidDstBuffer && length == 3);
    Require(sceCesUtf8ToSbc(han, 3, &length, profile, nullptr) == unassignedCode && length == 3);
    sbc = 0xBB;
    Require(sceCesUtf8ToSbc(han, 3, &length, profile, &sbc) == unassignedCode && length == 3 && sbc == 0);
    Require(sceCesUtf8ToSbc(euro, 3, nullptr, profile, &sbc) == 0 && sbc == 0x80);
    Require(Utf8ToSbc(profile, "A", 0) == invalidSrcBuffer);
    Require(Utf8ToSbc(profile, "\xE2\x82\xAC", 2) == srcBufferEnd);
    Require(Utf8ToSbc(profile, "\x80", 1) == invalidEncode);
    Require(Utf8ToSbc(profile, "\xC0\xAF", 2) == illegalCode);
    Require(Utf8ToSbc(profile, "\xED\xA0\x80", 3) == illegalCode);
    Require(Utf8ToSbc(profile, "\xE3\x81\x82", 3) == unassignedCode);
    Require(Utf8ToSbc(profile, "\xF0\x9F\x98\x80", 4) == unassignedCode);

    const std::uint8_t* cyrillic = sceCesRefersUcsProfileCp1251();
    Require(cyrillic != nullptr && cyrillic == sceCesRefersUcsProfileCp1251() && cyrillic != profile);
    RequireSbcToUtf8(cyrillic, 'A', "A");
    RequireSbcToUtf8(cyrillic, 0x88, "\xE2\x82\xAC");
    RequireSbcToUtf8(cyrillic, 0xC0, "\xD0\x90");
    RequireSbcToUtf8(cyrillic, 0xFF, "\xD1\x8F");
    RequireSbcToUtf8(cyrillic, 0xA8, "\xD0\x81");
    RequireSbcToUtf8(cyrillic, 0xB9, "\xE2\x84\x96");
    RequireUtf8ToSbc(cyrillic, "\xD0\x9F", 0xCF);
    RequireUtf8ToSbc(cyrillic, "\xD1\x91", 0xB8);
    RequireSbcRoundTrip(cyrillic, {0x98});
    Require(Utf8ToSbc(cyrillic, "\xC3\xA9", 2) == unassignedCode);
    Require(Utf8ToSbc(profile, "\xD0\x90", 2) == unassignedCode);

    const std::uint8_t* latin2 = sceCesRefersUcsProfileCp1250();
    Require(latin2 != nullptr && latin2 == sceCesRefersUcsProfileCp1250() && latin2 != profile && latin2 != cyrillic);
    RequireSbcToUtf8(latin2, 'A', "A");
    RequireSbcToUtf8(latin2, 0x80, "\xE2\x82\xAC");
    RequireSbcToUtf8(latin2, 0x8A, "\xC5\xA0");
    RequireSbcToUtf8(latin2, 0xA3, "\xC5\x81");
    RequireSbcToUtf8(latin2, 0xA5, "\xC4\x84");
    RequireSbcToUtf8(latin2, 0xAF, "\xC5\xBB");
    RequireSbcToUtf8(latin2, 0xE9, "\xC3\xA9");
    RequireSbcToUtf8(latin2, 0xFF, "\xCB\x99");
    RequireUtf8ToSbc(latin2, "\xC5\xA0", 0x8A);
    RequireUtf8ToSbc(latin2, "\xC5\x81", 0xA3);
    RequireUtf8ToSbc(latin2, "\xC3\xA9", 0xE9);
    RequireSbcRoundTrip(latin2, {0x81, 0x83, 0x88, 0x90, 0x98});
    Require(Utf8ToSbc(latin2, "\xD0\x90", 2) == unassignedCode);
    char latin2Context[256] = {};
    Require(sceCesMbcsUcsContextInit(latin2Context, latin2) == 0);
    std::uint8_t latin2Back[16] = {};
    std::uint32_t latin2Used = 0;
    std::uint32_t latin2Produced = 0;
    Require(sceCesMbcsStrToUtf8Str(latin2Context, reinterpret_cast<const std::uint8_t*>("\x80\xA3\xFF"), 3, &latin2Used, latin2Back, sizeof(latin2Back), &latin2Produced) == 0);
    Require(latin2Used == 3 && latin2Produced == 7 && std::memcmp(latin2Back, "\xE2\x82\xAC\xC5\x81\xCB\x99", 7) == 0);
    Require(sceCesMbcsStrGetUtf8Len(latin2Context, reinterpret_cast<const std::uint8_t*>("\x80\xA3\xFF"), 3, &latin2Used, &latin2Produced) == 0 && latin2Produced == 7);

    char context[256] = {};
    Require(sceCesMbcsUcsContextInit(context, cyrillic) == 0);
    Require(sceCesMbcsUcsContextInit(nullptr, cyrillic) == invalidParameter);

    char sjisSheet[256] = {};
    Require(sceCesUcsProfileInitSJis1997Cp932(sjisSheet) == sjisSheet);
    Require(sceCesUcsProfileInitSJis1997Cp932(nullptr) == nullptr);
    char sjisContext[256] = {};
    Require(sceCesMbcsUcsContextInit(sjisContext, sjisSheet) == 0);
    const auto* eucSource = reinterpret_cast<const std::uint8_t*>("Go \xEF\xBD\xB1!");
    std::uint8_t mbcs[32] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    char sheet[1024] = {};
    Require(sceCesUcsProfileInitEucJpCp51932(nullptr) == nullptr);
    void* eucJp = sceCesUcsProfileInitEucJpCp51932(sheet);
    Require(eucJp != nullptr);
    char eucContext[256] = {};
    Require(sceCesMbcsUcsContextInit(eucContext, eucJp) == 0);
    Require(sceCesUtf8StrToMbcsStr(eucContext, eucSource, 16, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 7 && produced == 6 && std::memcmp(mbcs, "Go \x8E\xB1!", 6) == 0);
    Require(sceCesUtf8StrGetMbcsLen(eucContext, eucSource, 16, &used, &produced) == 0 && used == 7 && produced == 6);
    Require(sceCesUtf8StrToMbcsStr(eucContext, eucSource, 16, &used, mbcs, 4, &produced) == dstBufferEnd);
    Require(used == 3 && produced == 3 && std::memcmp(mbcs, "Go ", 4) == 0);
    Require(sceCesUtf8StrToMbcsStr(eucContext, eucSource, 16, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(sceCesUtf8StrToMbcsStr(nullptr, eucSource, 16, &used, mbcs, sizeof(mbcs), &produced) == invalidParameter);
    Require(sceCesUtf8StrToMbcsStr(eucContext, reinterpret_cast<const std::uint8_t*>("\xF0\x9F\x98\x80"), 4, &used, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(used == 0 && produced == 0 && mbcs[0] == 0);
    Require(sceCesUtf8StrToMbcsStr(eucContext, eucSource, 16, &used, mbcs, sizeof(mbcs), &produced) == 0);

    std::uint8_t back[16] = {};
    Require(sceCesMbcsStrToUtf8Str(nullptr, mbcs, 6, &used, back, sizeof(back), &produced) == invalidParameter);
    Require(sceCesMbcsStrToUtf8Str(eucContext, mbcs, 6, &used, nullptr, sizeof(back), &produced) == invalidDstBuffer);
    Require(sceCesMbcsStrToUtf8Str(eucContext, mbcs, 6, &used, back, sizeof(back), &produced) == 0);
    Require(used == 6 && produced == 7 && std::memcmp(back, eucSource, 7) == 0);
    Require(sceCesMbcsStrGetUtf8Len(eucContext, mbcs, 6, &used, &produced) == 0 && produced == 7);
    Require(sceCesMbcsStrToUtf8Str(eucContext, reinterpret_cast<const std::uint8_t*>("\x8E"), 1, &used, back, sizeof(back), &produced) == srcBufferEnd);
    Require(sceCesMbcsStrToUtf8Str(eucContext, reinterpret_cast<const std::uint8_t*>("\x8E\x41"), 2, &used, back, sizeof(back), &produced) == invalidEncode);

    const std::uint16_t ucs2[] = {u'O', u'K', 0xFF9F, 0};
    Require(sceCesUcs2StrToMbcsStr(eucContext, ucs2, 4, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 3 && produced == 4 && std::memcmp(mbcs, "OK\x8E\xDF", 4) == 0);
    Require(sceCesUcs2StrGetMbcsLen(eucContext, ucs2, 4, &used, &produced) == 0 && produced == 4);
    std::uint16_t wide[16] = {};
    Require(sceCesMbcsStrToUcs2Str(eucContext, mbcs, 4, &used, wide, 16, &produced) == 0);
    Require(used == 4 && produced == 3 && wide[0] == u'O' && wide[1] == u'K' && wide[2] == 0xFF9F);
    Require(sceCesMbcsStrGetUcs2Len(eucContext, mbcs, 4, &used, &produced) == 0 && produced == 3);
    std::uint16_t single = 0;
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8E\xB1"), 2, &used, &single) == 0 && used == 2 && single == 0xFF71);
    Require(sceCesEucJpToUcs2(cyrillic, reinterpret_cast<const std::uint8_t*>("A"), 1, &used, &single) == invalidParameter);

    const auto* japanese = reinterpret_cast<const std::uint8_t*>("\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");
    Require(sceCesUtf8StrToMbcsStr(eucContext, japanese, 24, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 24 && produced == 16 && std::memcmp(mbcs, "\xA4\xB3\xA4\xF3\xA4\xCB\xA4\xC1\xA4\xCF\xC6\xFC\xCB\xDC\xB8\xEC", 16) == 0);
    std::uint8_t japaneseBack[32] = {};
    Require(sceCesMbcsStrToUtf8Str(eucContext, mbcs, 16, &used, japaneseBack, sizeof(japaneseBack), &produced) == 0 && produced == 24 && std::memcmp(japaneseBack, japanese, 24) == 0);
    Require(sceCesMbcsStrToUcs2Str(eucContext, mbcs, 16, &used, wide, 16, &produced) == 0 && used == 16 && produced == 8 && wide[0] == 0x3053 && wide[5] == 0x65E5 && wide[7] == 0x8A9E);
    Require(sceCesMbcsStrToUcs2Str(eucContext, mbcs, 15, &used, wide, 16, &produced) == srcBufferEnd);
    const std::uint16_t extensions[] = {0x2460, 0xFF5E, 0x2252, 0};
    Require(sceCesUcs2StrToMbcsStr(eucContext, extensions, 4, &used, mbcs, sizeof(mbcs), &produced) == 0 && produced == 6 && std::memcmp(mbcs, "\xAD\xA1\xA1\xC1\xA2\xE2", 6) == 0);
    Require(sceCesMbcsStrToUcs2Str(eucContext, reinterpret_cast<const std::uint8_t*>("\xA9\xA1"), 2, &used, wide, 16, &produced) == unassignedCode);
    Require(sceCesMbcsStrToUcs2Str(eucContext, reinterpret_cast<const std::uint8_t*>("\xA4\x41"), 2, &used, wide, 16, &produced) == invalidEncode);
    Require(sceCesMbcsStrToUcs2Str(eucContext, reinterpret_cast<const std::uint8_t*>("\xA4"), 1, &used, wide, 16, &produced) == srcBufferEnd);
    const std::uint16_t yen[] = {0x00A5, 0};
    Require(sceCesUcs2StrToMbcsStr(eucContext, yen, 2, &used, mbcs, sizeof(mbcs), &produced) == 0 && used == 1 && produced == 2 && std::memcmp(mbcs, "\xA1\xEF", 3) == 0);
    std::uint32_t assigned = 0;
    for (std::uint32_t lead = 0xA1; lead <= 0xFE; ++lead) {
        for (std::uint32_t trail = 0xA1; trail <= 0xFE; ++trail) {
            const std::uint8_t cell[2] = {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint16_t decoded[2] = {};
            if (sceCesEucJpToUcs2(eucJp, cell, 2, &used, decoded) != 0) continue;
            ++assigned;
            decoded[1] = 0;
            std::uint8_t again[4] = {};
            std::uint16_t second[2] = {};
            Require(sceCesUcs2StrToMbcsStr(eucContext, decoded, 2, &used, again, sizeof(again), &produced) == 0 && produced == 2);
            Require(sceCesEucJpToUcs2(eucJp, again, 2, &used, second) == 0 && second[0] == decoded[0]);
            if (again[0] != cell[0] || again[1] != cell[1]) Require(again[0] < cell[0] || (again[0] == cell[0] && again[1] < cell[1]));
        }
    }
    Require(assigned == 7336);

    std::uint16_t supplementary = 0;
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xA2\xAF"), 3, &used, &supplementary) == invalidEncode && used == 3 && supplementary == 0);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xA2"), 2, &used, &supplementary) == srcBufferEnd && used == 3);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\x41\xA1"), 3, &used, &supplementary) == invalidEncode);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xA2\x41"), 3, &used, &supplementary) == invalidEncode);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xA1\xA1"), 3, &used, &supplementary) == unassignedCode);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xF5\xA1"), 3, &used, &supplementary) == unassignedCode);
    const std::uint16_t supplementaryText[] = {u'A', 0x02D8, 0};
    Require(sceCesUcs2StrToMbcsStr(eucContext, supplementaryText, 0, &used, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(used == 1 && produced == 1 && std::memcmp(mbcs, "A", 2) == 0);
    const std::uint16_t ibmText[] = {0x6600, 0x2170, 0};
    Require(sceCesUcs2StrToMbcsStr(eucContext, ibmText, 0, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 2 && produced == 4 && std::memcmp(mbcs, "\xFA\xB4\xFC\xF1", 5) == 0);
    Require(sceCesMbcsStrToUtf8Str(eucContext, reinterpret_cast<const std::uint8_t*>("A\x8F\xA2\xAF" "B"), 0, &used, japaneseBack, sizeof(japaneseBack), &produced) == invalidEncode);
    Require(used == 1 && produced == 1 && std::memcmp(japaneseBack, "A", 2) == 0);
    RequireMeasuredEucJp(eucJp);
    char measuredLatin[256] = {};
    Require(sceCesMbcsUcsContextInit(measuredLatin, profile) == 0);
    RequireMeasuredStrings(measuredLatin, eucContext);
    Require(sceCesMbcsUcsContextInit(measuredLatin, nullptr) == invalidParameter);

    const auto* sjisSource = reinterpret_cast<const std::uint8_t*>("Go \xEF\xBD\xB1!");
    Require(sceCesUtf8StrToMbcsStr(sjisContext, sjisSource, 16, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 7 && produced == 5 && std::memcmp(mbcs, "Go \xB1!", 5) == 0);
    Require(sceCesUtf8StrGetMbcsLen(sjisContext, sjisSource, 16, &used, &produced) == 0 && used == 7 && produced == 5);
    Require(sceCesUtf8StrToMbcsStr(sjisContext, sjisSource, 16, &used, mbcs, 4, &produced) == dstBufferEnd);
    Require(used == 3 && produced == 3 && std::memcmp(mbcs, "Go ", 4) == 0);
    Require(sceCesUtf8StrToMbcsStr(sjisContext, sjisSource, 16, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(sceCesUtf8StrToMbcsStr(sjisContext, reinterpret_cast<const std::uint8_t*>("\xF0\x9F\x98\x80"), 4, &used, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(used == 0 && produced == 0 && mbcs[0] == 0);
    Require(sceCesUtf8StrToMbcsStr(sjisContext, sjisSource, 16, &used, mbcs, sizeof(mbcs), &produced) == 0);

    std::uint8_t sjisBack[16] = {};
    Require(sceCesMbcsStrToUtf8Str(sjisContext, mbcs, 5, &used, sjisBack, sizeof(sjisBack), &produced) == 0);
    Require(used == 5 && produced == 7 && std::memcmp(sjisBack, sjisSource, 7) == 0);
    Require(sceCesMbcsStrGetUtf8Len(sjisContext, mbcs, 5, &used, &produced) == 0 && produced == 7);
    Require(sceCesMbcsStrToUtf8Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x82"), 1, &used, sjisBack, sizeof(sjisBack), &produced) == srcBufferEnd);
    Require(sceCesMbcsStrToUtf8Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x82\x20"), 2, &used, sjisBack, sizeof(sjisBack), &produced) == invalidEncode);
    Require(sceCesMbcsStrToUtf8Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x82\x7F"), 2, &used, sjisBack, sizeof(sjisBack), &produced) == invalidEncode);

    const std::uint16_t sjisUcs2[] = {u'O', u'K', 0xFF9F, 0};
    Require(sceCesUcs2StrToMbcsStr(sjisContext, sjisUcs2, 4, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 3 && produced == 3 && std::memcmp(mbcs, "OK\xDF", 3) == 0);
    Require(sceCesUcs2StrGetMbcsLen(sjisContext, sjisUcs2, 4, &used, &produced) == 0 && produced == 3);
    std::uint16_t sjisWide[16] = {};
    Require(sceCesMbcsStrToUcs2Str(sjisContext, mbcs, 3, &used, sjisWide, 16, &produced) == 0);
    Require(used == 3 && produced == 3 && sjisWide[0] == u'O' && sjisWide[1] == u'K' && sjisWide[2] == 0xFF9F);
    Require(sceCesMbcsStrGetUcs2Len(sjisContext, mbcs, 3, &used, &produced) == 0 && produced == 3);

    Require(sceCesUtf8StrToMbcsStr(sjisContext, japanese, 24, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 24 && produced == 16 && std::memcmp(mbcs, "\x82\xB1\x82\xF1\x82\xC9\x82\xBF\x82\xCD\x93\xFA\x96\x7B\x8C\xEA", 16) == 0);
    std::uint8_t sjisJapaneseBack[32] = {};
    Require(sceCesMbcsStrToUtf8Str(sjisContext, mbcs, 16, &used, sjisJapaneseBack, sizeof(sjisJapaneseBack), &produced) == 0 && produced == 24 && std::memcmp(sjisJapaneseBack, japanese, 24) == 0);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, mbcs, 16, &used, sjisWide, 16, &produced) == 0 && used == 16 && produced == 8 && sjisWide[0] == 0x3053 && sjisWide[5] == 0x65E5 && sjisWide[7] == 0x8A9E);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, mbcs, 15, &used, sjisWide, 16, &produced) == srcBufferEnd);
    const std::uint16_t sjisExtensions[] = {0x2460, 0xFF5E, 0x2252, 0};
    Require(sceCesUcs2StrToMbcsStr(sjisContext, sjisExtensions, 4, &used, mbcs, sizeof(mbcs), &produced) == 0 && produced == 6 && std::memcmp(mbcs, "\x87\x40\x81\x60\x81\xE0", 6) == 0);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x85\x40"), 2, &used, sjisWide, 16, &produced) == unassignedCode);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x80"), 1, &used, sjisWide, 16, &produced) == invalidEncode);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xA0"), 1, &used, sjisWide, 16, &produced) == invalidEncode);
    Require(sceCesUcs2StrToMbcsStr(sjisContext, yen, 2, &used, mbcs, sizeof(mbcs), &produced) == 0 && used == 1 && produced == 2 && std::memcmp(mbcs, "\x81\x8F", 3) == 0);
    RequireMeasuredEncoders(eucContext, sjisContext);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xFB\xFC"), 2, &used, sjisWide, 16, &produced) == 0 && used == 2 && produced == 1 && sjisWide[0] == 0x9AD9);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xFA\x40"), 2, &used, sjisWide, 16, &produced) == 0 && used == 2 && produced == 1 && sjisWide[0] == 0x2170);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xFC\x4B"), 2, &used, sjisWide, 16, &produced) == 0 && used == 2 && produced == 1 && sjisWide[0] == 0x9ED1);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xFC\x4C"), 2, &used, sjisWide, 16, &produced) == unassignedCode);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xF0\x40"), 2, &used, sjisWide, 16, &produced) == unassignedCode);
    std::uint32_t sjisAssigned = 0;
    for (std::uint32_t lead = 0x81; lead <= 0xFC; ++lead) {
        if (lead > 0x9F && lead < 0xE0) continue;
        for (std::uint32_t trail = 0x40; trail <= 0xFC; ++trail) {
            if (trail == 0x7F) continue;
            const std::uint8_t cell[2] = {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint16_t decoded[2] = {};
            if (sceCesMbcsStrToUcs2Str(sjisContext, cell, 2, &used, decoded, 2, &produced) != 0) continue;
            ++sjisAssigned;
            decoded[1] = 0;
            std::uint8_t again[4] = {};
            std::uint16_t second[2] = {};
            Require(sceCesUcs2StrToMbcsStr(sjisContext, decoded, 2, &used, again, sizeof(again), &produced) == 0 && produced == 2);
            Require(sceCesMbcsStrToUcs2Str(sjisContext, again, 2, &used, second, 2, &produced) == 0 && second[0] == decoded[0]);
            if (again[0] != cell[0] || again[1] != cell[1]) Require(again[0] < cell[0] || (again[0] == cell[0] && again[1] < cell[1]));
        }
    }
    Require(sjisAssigned == 7724);

    Require(sceCesMbcsStrToUtf8Str(context, reinterpret_cast<const std::uint8_t*>("\xC0\xFF"), 2, &used, back, sizeof(back), &produced) == 0);
    Require(produced == 4 && std::memcmp(back, "\xD0\x90\xD1\x8F", 4) == 0);
    Require(sceCesUtf8StrToMbcsStr(context, back, 4, &used, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && mbcs[0] == 0xC0 && mbcs[1] == 0xFF);
}
