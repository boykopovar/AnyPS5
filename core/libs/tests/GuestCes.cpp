#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1251(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1250(void);
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

    RequireSbcToUtf8(profile, 'A', "A");
    RequireSbcToUtf8(profile, 0x80, "\xE2\x82\xAC");
    RequireSbcToUtf8(profile, 0x9F, "\xC5\xB8");
    RequireSbcToUtf8(profile, 0x81, "\xC2\x81");
    RequireSbcToUtf8(profile, 0xE9, "\xC3\xA9");
    RequireSbcToUtf8(profile, 0xFF, "\xC3\xBF");

    RequireUtf8ToSbc(profile, "A", 'A');
    RequireUtf8ToSbc(profile, "\xE2\x82\xAC", 0x80);
    RequireUtf8ToSbc(profile, "\xE2\x84\xA2", 0x99);
    RequireUtf8ToSbc(profile, "\xC3\xA9", 0xE9);
    RequireUtf8ToSbc(profile, "\xC2\x8D", 0x8D);
    for (int sbc = 1; sbc < 256; ++sbc) {
        std::uint8_t utf8[4] = {};
        std::uint32_t length = 0;
        std::uint8_t back = 0;
        Require(sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(sbc), utf8, sizeof(utf8), &length) == 0);
        Require(sceCesUtf8ToSbc(utf8, length, &length, profile, &back) == 0 && back == sbc);
    }

    std::uint8_t utf8[4] = {};
    std::uint32_t length = 0;
    std::uint8_t sbc = 0;
    Require(sceCesSbcToUtf8(nullptr, 'A', utf8, sizeof(utf8), &length) == invalidParameter);
    Require(sceCesSbcToUtf8(profile, 'A', nullptr, sizeof(utf8), &length) == invalidDstBuffer);
    Require(sceCesSbcToUtf8(profile, 0x80, utf8, 2, &length) == dstBufferEnd);
    Require(sceCesUtf8ToSbc(utf8, 1, &length, nullptr, &sbc) == invalidParameter);
    Require(sceCesUtf8ToSbc(nullptr, 1, &length, profile, &sbc) == invalidSrcBuffer);
    Require(sceCesUtf8ToSbc(utf8, 1, &length, profile, nullptr) == invalidDstBuffer);
    Require(Utf8ToSbc(profile, "A", 0) == srcBufferEnd);
    Require(Utf8ToSbc(profile, "\xE2\x82\xAC", 2) == srcBufferEnd);
    Require(Utf8ToSbc(profile, "\x80", 1) == invalidEncode);
    Require(Utf8ToSbc(profile, "\xC0\xAF", 2) == invalidEncode);
    Require(Utf8ToSbc(profile, "\xED\xA0\x80", 3) == invalidEncode);
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
    for (int sbc = 1; sbc < 256; ++sbc) {
        std::uint8_t cp[4] = {};
        std::uint32_t cpLength = 0;
        std::uint8_t back = 0;
        Require(sceCesSbcToUtf8(cyrillic, static_cast<std::uint8_t>(sbc), cp, sizeof(cp), &cpLength) == 0);
        Require(sceCesUtf8ToSbc(cp, cpLength, &cpLength, cyrillic, &back) == 0 && back == sbc);
    }
    Require(Utf8ToSbc(cyrillic, "\xC3\xA9", 2) == unassignedCode);
    Require(Utf8ToSbc(profile, "\xD0\x90", 2) == unassignedCode);

    const std::uint8_t* centralEurope = sceCesRefersUcsProfileCp1250();
    Require(centralEurope != nullptr && centralEurope == sceCesRefersUcsProfileCp1250() && centralEurope != profile && centralEurope != cyrillic);
    RequireSbcToUtf8(centralEurope, 'A', "A");
    RequireSbcToUtf8(centralEurope, 0x80, "\xE2\x82\xAC");
    RequireSbcToUtf8(centralEurope, 0x8C, "\xC5\x9A");
    RequireSbcToUtf8(centralEurope, 0x8F, "\xC5\xB9");
    RequireSbcToUtf8(centralEurope, 0xA1, "\xCB\x87");
    RequireSbcToUtf8(centralEurope, 0xA5, "\xC4\x84");
    RequireSbcToUtf8(centralEurope, 0xC1, "\xC3\x81");
    RequireSbcToUtf8(centralEurope, 0xFF, "\xCB\x99");
    RequireUtf8ToSbc(centralEurope, "\xC5\x9A", 0x8C);
    RequireUtf8ToSbc(centralEurope, "\xC5\xB9", 0x8F);
    RequireUtf8ToSbc(centralEurope, "\xC4\x84", 0xA5);
    RequireUtf8ToSbc(centralEurope, "\xCB\x99", 0xFF);
    for (int sbc = 1; sbc < 256; ++sbc) {
        std::uint8_t cp[4] = {};
        std::uint32_t cpLength = 0;
        std::uint8_t back = 0;
        Require(sceCesSbcToUtf8(centralEurope, static_cast<std::uint8_t>(sbc), cp, sizeof(cp), &cpLength) == 0);
        Require(sceCesUtf8ToSbc(cp, cpLength, &cpLength, centralEurope, &back) == 0 && back == sbc);
    }
    Require(Utf8ToSbc(centralEurope, "\xD0\x90", 2) == unassignedCode);
    Require(Utf8ToSbc(centralEurope, "\xE3\x81\x82", 3) == unassignedCode);

    char context[256] = {};
    Require(sceCesMbcsUcsContextInit(context, cyrillic) == 0);
    Require(sceCesMbcsUcsContextInit(nullptr, cyrillic) == invalidParameter);

    char sjisSheet[256] = {};
    Require(sceCesUcsProfileInitSJis1997Cp932(sjisSheet) == sjisSheet);
    Require(sceCesUcsProfileInitSJis1997Cp932(nullptr) == nullptr);
    char sjisContext[256] = {};
    Require(sceCesMbcsUcsContextInit(sjisContext, sjisSheet) == 0);
    const auto* eucSource = reinterpret_cast<const std::uint8_t*>("Go \xEF\xBD\xB1!");
    std::uint8_t mbcs[16] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    char sheet[1024] = {};
    void* eucJp = sceCesUcsProfileInitEucJpCp51932(sheet);
    Require(eucJp != nullptr);
    char eucContext[256] = {};
    Require(sceCesMbcsUcsContextInit(eucContext, eucJp) == 0);
    Require(sceCesUtf8StrToMbcsStr(eucContext, eucSource, 16, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 7 && produced == 6 && std::memcmp(mbcs, "Go \x8E\xB1!", 6) == 0);
    Require(sceCesUtf8StrGetMbcsLen(eucContext, eucSource, 16, &used, &produced) == 0 && used == 7 && produced == 6);
    Require(sceCesUtf8StrToMbcsStr(eucContext, eucSource, 16, &used, mbcs, 4, &produced) == dstBufferEnd);
    Require(sceCesUtf8StrToMbcsStr(nullptr, eucSource, 16, &used, mbcs, sizeof(mbcs), &produced) == invalidParameter);
    Require(sceCesUtf8StrToMbcsStr(eucContext, reinterpret_cast<const std::uint8_t*>("\xF0\x9F\x98\x80"), 4, &used, mbcs, sizeof(mbcs), &produced) == unassignedCode);

    std::uint8_t back[16] = {};
    Require(sceCesMbcsStrToUtf8Str(eucContext, mbcs, 6, &used, back, sizeof(back), &produced) == 0);
    Require(used == 6 && produced == 7 && std::memcmp(back, eucSource, 7) == 0);
    Require(sceCesMbcsStrGetUtf8Len(eucContext, mbcs, 6, &used, &produced) == 0 && produced == 7);
    Require(sceCesMbcsStrToUtf8Str(eucContext, reinterpret_cast<const std::uint8_t*>("\x8E"), 1, &used, back, sizeof(back), &produced) == srcBufferEnd);
    Require(sceCesMbcsStrToUtf8Str(eucContext, reinterpret_cast<const std::uint8_t*>("\x8E\x41"), 2, &used, back, sizeof(back), &produced) == invalidEncode);

    const std::uint16_t ucs2[] = {u'O', u'K', 0xFF9F, 0};
    Require(sceCesUcs2StrToMbcsStr(eucContext, ucs2, 4, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 3 && produced == 4 && std::memcmp(mbcs, "OK\x8E\xDF", 4) == 0);
    Require(sceCesUcs2StrGetMbcsLen(eucContext, ucs2, 4, &used, &produced) == 0 && produced == 4);
    std::uint16_t wide[8] = {};
    Require(sceCesMbcsStrToUcs2Str(eucContext, mbcs, 4, &used, wide, 8, &produced) == 0);
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
    Require(sceCesMbcsStrToUcs2Str(eucContext, mbcs, 16, &used, wide, 8, &produced) == 0 && used == 16 && produced == 8 && wide[0] == 0x3053 && wide[5] == 0x65E5 && wide[7] == 0x8A9E);
    Require(sceCesMbcsStrToUcs2Str(eucContext, mbcs, 15, &used, wide, 8, &produced) == srcBufferEnd);
    const std::uint16_t extensions[] = {0x2460, 0xFF5E, 0x2252, 0};
    Require(sceCesUcs2StrToMbcsStr(eucContext, extensions, 4, &used, mbcs, sizeof(mbcs), &produced) == 0 && produced == 6 && std::memcmp(mbcs, "\xAD\xA1\xA1\xC1\xA2\xE2", 6) == 0);
    Require(sceCesMbcsStrToUcs2Str(eucContext, reinterpret_cast<const std::uint8_t*>("\xA9\xA1"), 2, &used, wide, 8, &produced) == unassignedCode);
    Require(sceCesMbcsStrToUcs2Str(eucContext, reinterpret_cast<const std::uint8_t*>("\xA4\x41"), 2, &used, wide, 8, &produced) == invalidEncode);
    Require(sceCesMbcsStrToUcs2Str(eucContext, reinterpret_cast<const std::uint8_t*>("\xA4"), 1, &used, wide, 8, &produced) == srcBufferEnd);
    const std::uint16_t yen[] = {0x00A5, 0};
    Require(sceCesUcs2StrToMbcsStr(eucContext, yen, 2, &used, mbcs, sizeof(mbcs), &produced) == unassignedCode);
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

    const auto* sjisSource = reinterpret_cast<const std::uint8_t*>("Go \xEF\xBD\xB1!");
    Require(sceCesUtf8StrToMbcsStr(sjisContext, sjisSource, 16, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 7 && produced == 4 && std::memcmp(mbcs, "Go \xB1!", 4) == 0);
    Require(sceCesUtf8StrGetMbcsLen(sjisContext, sjisSource, 16, &used, &produced) == 0 && used == 7 && produced == 4);
    Require(sceCesUtf8StrToMbcsStr(sjisContext, sjisSource, 16, &used, mbcs, 3, &produced) == dstBufferEnd);
    Require(sceCesUtf8StrToMbcsStr(sjisContext, reinterpret_cast<const std::uint8_t*>("\xF0\x9F\x98\x80"), 4, &used, mbcs, sizeof(mbcs), &produced) == unassignedCode);

    std::uint8_t sjisBack[16] = {};
    Require(sceCesMbcsStrToUtf8Str(sjisContext, mbcs, 4, &used, sjisBack, sizeof(sjisBack), &produced) == 0);
    Require(used == 4 && produced == 7 && std::memcmp(sjisBack, sjisSource, 7) == 0);
    Require(sceCesMbcsStrGetUtf8Len(sjisContext, mbcs, 4, &used, &produced) == 0 && produced == 7);
    Require(sceCesMbcsStrToUtf8Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x82"), 1, &used, sjisBack, sizeof(sjisBack), &produced) == srcBufferEnd);
    Require(sceCesMbcsStrToUtf8Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x82\x20"), 2, &used, sjisBack, sizeof(sjisBack), &produced) == invalidEncode);
    Require(sceCesMbcsStrToUtf8Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x82\x7F"), 2, &used, sjisBack, sizeof(sjisBack), &produced) == invalidEncode);

    const std::uint16_t sjisUcs2[] = {u'O', u'K', 0xFF9F, 0};
    Require(sceCesUcs2StrToMbcsStr(sjisContext, sjisUcs2, 4, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 3 && produced == 3 && std::memcmp(mbcs, "OK\xDF", 3) == 0);
    Require(sceCesUcs2StrGetMbcsLen(sjisContext, sjisUcs2, 4, &used, &produced) == 0 && produced == 3);
    std::uint16_t sjisWide[8] = {};
    Require(sceCesMbcsStrToUcs2Str(sjisContext, mbcs, 3, &used, sjisWide, 8, &produced) == 0);
    Require(used == 3 && produced == 3 && sjisWide[0] == u'O' && sjisWide[1] == u'K' && sjisWide[2] == 0xFF9F);
    Require(sceCesMbcsStrGetUcs2Len(sjisContext, mbcs, 3, &used, &produced) == 0 && produced == 3);

    Require(sceCesUtf8StrToMbcsStr(sjisContext, japanese, 24, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 24 && produced == 16 && std::memcmp(mbcs, "\x82\xB1\x82\xF1\x82\xC9\x82\xBF\x82\xCD\x93\xFA\x96\x7B\x8C\xEA", 16) == 0);
    std::uint8_t sjisJapaneseBack[32] = {};
    Require(sceCesMbcsStrToUtf8Str(sjisContext, mbcs, 16, &used, sjisJapaneseBack, sizeof(sjisJapaneseBack), &produced) == 0 && produced == 24 && std::memcmp(sjisJapaneseBack, japanese, 24) == 0);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, mbcs, 16, &used, sjisWide, 8, &produced) == 0 && used == 16 && produced == 8 && sjisWide[0] == 0x3053 && sjisWide[5] == 0x65E5 && sjisWide[7] == 0x8A9E);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, mbcs, 15, &used, sjisWide, 8, &produced) == srcBufferEnd);
    const std::uint16_t sjisExtensions[] = {0x2460, 0xFF5E, 0x2252, 0};
    Require(sceCesUcs2StrToMbcsStr(sjisContext, sjisExtensions, 4, &used, mbcs, sizeof(mbcs), &produced) == 0 && produced == 6 && std::memcmp(mbcs, "\x87\x40\x81\x60\x81\xE0", 6) == 0);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x85\x40"), 2, &used, sjisWide, 8, &produced) == unassignedCode);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\x80"), 1, &used, sjisWide, 8, &produced) == invalidEncode);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xA0"), 1, &used, sjisWide, 8, &produced) == invalidEncode);
    Require(sceCesUcs2StrToMbcsStr(sjisContext, yen, 2, &used, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xFB\xFC"), 2, &used, sjisWide, 8, &produced) == 0 && used == 2 && produced == 1 && sjisWide[0] == 0x9AD9);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xFA\x40"), 2, &used, sjisWide, 8, &produced) == 0 && used == 2 && produced == 1 && sjisWide[0] == 0x2170);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xFC\x4B"), 2, &used, sjisWide, 8, &produced) == 0 && used == 2 && produced == 1 && sjisWide[0] == 0x9ED1);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xFC\x4C"), 2, &used, sjisWide, 8, &produced) == unassignedCode);
    Require(sceCesMbcsStrToUcs2Str(sjisContext, reinterpret_cast<const std::uint8_t*>("\xF0\x40"), 2, &used, sjisWide, 8, &produced) == unassignedCode);
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

    char cp1250Context[256] = {};
    Require(sceCesMbcsUcsContextInit(cp1250Context, centralEurope) == 0);
    Require(sceCesMbcsStrToUtf8Str(cp1250Context, reinterpret_cast<const std::uint8_t*>("\x8C\x8F"), 2, &used, back, sizeof(back), &produced) == 0);
    Require(produced == 4 && std::memcmp(back, "\xC5\x9A\xC5\xB9", 4) == 0);
    Require(sceCesUtf8StrToMbcsStr(cp1250Context, back, 4, &used, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && mbcs[0] == 0x8C && mbcs[1] == 0x8F);

    const std::uint16_t cp1250Wide[] = {0x015A, 0x0179, 0};
    Require(sceCesUcs2StrToMbcsStr(cp1250Context, cp1250Wide, 3, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 2 && produced == 2 && mbcs[0] == 0x8C && mbcs[1] == 0x8F);
    Require(sceCesUcs2StrGetMbcsLen(cp1250Context, cp1250Wide, 3, &used, &produced) == 0 && used == 2 && produced == 2);
    Require(sceCesMbcsStrToUcs2Str(cp1250Context, mbcs, 2, &used, wide, 8, &produced) == 0);
    Require(used == 2 && produced == 2 && wide[0] == 0x015A && wide[1] == 0x0179);
    Require(sceCesMbcsStrGetUcs2Len(cp1250Context, mbcs, 2, &used, &produced) == 0 && produced == 2);

    char cp1252Context[256] = {};
    Require(sceCesMbcsUcsContextInit(cp1252Context, profile) == 0);
    const std::uint16_t cp1252Wide[] = {0x20AC, 0x0160, 0};
    Require(sceCesUcs2StrToMbcsStr(cp1252Context, cp1252Wide, 3, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 2 && produced == 2 && mbcs[0] == 0x80 && mbcs[1] == 0x8A);
    Require(sceCesUcs2StrGetMbcsLen(cp1252Context, cp1252Wide, 3, &used, &produced) == 0 && used == 2 && produced == 2);
    Require(sceCesMbcsStrToUcs2Str(cp1252Context, mbcs, 2, &used, wide, 8, &produced) == 0);
    Require(used == 2 && produced == 2 && wide[0] == 0x20AC && wide[1] == 0x0160);
    Require(sceCesMbcsStrGetUcs2Len(cp1252Context, mbcs, 2, &used, &produced) == 0 && produced == 2);

    const std::uint16_t cp1251Wide[] = {0x0410, 0x044F, 0};
    Require(sceCesUcs2StrToMbcsStr(context, cp1251Wide, 3, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 2 && produced == 2 && mbcs[0] == 0xC0 && mbcs[1] == 0xFF);
    Require(sceCesUcs2StrGetMbcsLen(context, cp1251Wide, 3, &used, &produced) == 0 && used == 2 && produced == 2);
    Require(sceCesMbcsStrToUcs2Str(context, mbcs, 2, &used, wide, 8, &produced) == 0);
    Require(used == 2 && produced == 2 && wide[0] == 0x0410 && wide[1] == 0x044F);
    Require(sceCesMbcsStrGetUcs2Len(context, mbcs, 2, &used, &produced) == 0 && produced == 2);
}
