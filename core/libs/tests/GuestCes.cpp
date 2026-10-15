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

    const std::uint8_t* latin2 = sceCesRefersUcsProfileCp1250();
    Require(latin2 != nullptr && latin2 == sceCesRefersUcsProfileCp1250() && latin2 != profile && latin2 != cyrillic);
    RequireSbcToUtf8(latin2, 'A', "A");
    RequireSbcToUtf8(latin2, 0x80, "\xE2\x82\xAC");
    RequireSbcToUtf8(latin2, 0x81, "\xC2\x81");
    RequireSbcToUtf8(latin2, 0x8A, "\xC5\xA0");
    RequireSbcToUtf8(latin2, 0xA3, "\xC5\x81");
    RequireSbcToUtf8(latin2, 0xA5, "\xC4\x84");
    RequireSbcToUtf8(latin2, 0xAF, "\xC5\xBB");
    RequireSbcToUtf8(latin2, 0xE9, "\xC3\xA9");
    RequireSbcToUtf8(latin2, 0xFF, "\xCB\x99");
    RequireUtf8ToSbc(latin2, "\xC5\xA0", 0x8A);
    RequireUtf8ToSbc(latin2, "\xC5\x81", 0xA3);
    RequireUtf8ToSbc(latin2, "\xC3\xA9", 0xE9);
    for (int sbc = 1; sbc < 256; ++sbc) {
        std::uint8_t cp[4] = {};
        std::uint32_t cpLength = 0;
        std::uint8_t back = 0;
        Require(sceCesSbcToUtf8(latin2, static_cast<std::uint8_t>(sbc), cp, sizeof(cp), &cpLength) == 0);
        Require(sceCesUtf8ToSbc(cp, cpLength, &cpLength, latin2, &back) == 0 && back == sbc);
    }
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

    std::uint16_t supplementary = 0;
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xA2\xAF"), 3, &used, &supplementary) == 0 && used == 3 && supplementary == 0x02D8);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xA2"), 2, &used, &supplementary) == srcBufferEnd);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\x41\xA1"), 3, &used, &supplementary) == invalidEncode);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xA2\x41"), 3, &used, &supplementary) == invalidEncode);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xA1\xA1"), 3, &used, &supplementary) == unassignedCode);
    Require(sceCesEucJpToUcs2(eucJp, reinterpret_cast<const std::uint8_t*>("\x8F\xF5\xA1"), 3, &used, &supplementary) == unassignedCode);
    const std::uint16_t supplementaryText[] = {0x02D8, 0x0386, 0x0402, 0x6600, 0};
    Require(sceCesUcs2StrToMbcsStr(eucContext, supplementaryText, 5, &used, mbcs, sizeof(mbcs), &produced) == 0);
    Require(used == 4 && produced == 11 && std::memcmp(mbcs, "\x8F\xA2\xAF\x8F\xA6\xE1\x8F\xA7\xC2\xFA\xB4", 11) == 0);
    Require(sceCesUcs2StrGetMbcsLen(eucContext, supplementaryText, 5, &used, &produced) == 0 && produced == 11);
    std::uint16_t supplementaryBack[8] = {};
    Require(sceCesMbcsStrToUcs2Str(eucContext, mbcs, 11, &used, supplementaryBack, 8, &produced) == 0);
    Require(used == 11 && produced == 4 && supplementaryBack[0] == 0x02D8 && supplementaryBack[1] == 0x0386 && supplementaryBack[2] == 0x0402 && supplementaryBack[3] == 0x6600);
    Require(sceCesMbcsStrGetUcs2Len(eucContext, mbcs, 11, &used, &produced) == 0 && produced == 4);
    Require(sceCesMbcsStrToUtf8Str(eucContext, mbcs, 11, &used, japaneseBack, sizeof(japaneseBack), &produced) == 0);
    Require(produced == 9 && std::memcmp(japaneseBack, "\xCB\x98\xCE\x86\xD0\x82\xE6\x98\x80", 9) == 0);
    Require(sceCesMbcsStrGetUtf8Len(eucContext, mbcs, 11, &used, &produced) == 0 && produced == 9);
    Require(sceCesMbcsStrToUcs2Str(eucContext, mbcs, 10, &used, supplementaryBack, 8, &produced) == srcBufferEnd);
    std::uint32_t supplementaryAssigned = 0;
    for (std::uint32_t lead = 0xA2; lead <= 0xF4; ++lead) {
        for (std::uint32_t trail = 0xA1; trail <= 0xFE; ++trail) {
            const std::uint8_t cell[3] = {0x8F, static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint16_t decoded[2] = {};
            if (sceCesEucJpToUcs2(eucJp, cell, 3, &used, decoded) != 0) continue;
            ++supplementaryAssigned;
            decoded[1] = 0;
            std::uint8_t again[4] = {};
            std::uint16_t second[2] = {};
            Require(sceCesUcs2StrToMbcsStr(eucContext, decoded, 2, &used, again, sizeof(again), &produced) == 0);
            if (produced == 3) Require(again[1] == cell[1] && again[2] == cell[2]);
            Require(sceCesEucJpToUcs2(eucJp, again, produced, &used, second) == 0 && second[0] == decoded[0]);
        }
    }
    Require(supplementaryAssigned == 6067);

    Require(sceCesMbcsStrToUtf8Str(context, reinterpret_cast<const std::uint8_t*>("\xC0\xFF"), 2, &used, back, sizeof(back), &produced) == 0);
    Require(produced == 4 && std::memcmp(back, "\xD0\x90\xD1\x8F", 4) == 0);
    Require(sceCesUtf8StrToMbcsStr(context, back, 4, &used, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && mbcs[0] == 0xC0 && mbcs[1] == 0xFF);
}
