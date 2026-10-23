#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

extern "C" {
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileJisX0201Katakana(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileAscii(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileJisX0201(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileJisX0201Roman(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileKsX1003(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileGbT1988(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_1(void);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252(void);
void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void*);
void* APS5_VABI sceCesUcsProfileInitEucJpCp51932(void*);
void* APS5_VABI sceCesUcsProfileInitEucKr(void*);
void* APS5_VABI sceCesUcsProfileInitUhc(void*);
void* APS5_VABI sceCesUcsProfileInitEucCnGb2312(void*);
void* APS5_VABI sceCesUcsProfileInitGbkCp936(void*);
void* APS5_VABI sceCesUcsProfileInitGb18030_2000(void*);
void* APS5_VABI sceCesUcsProfileInitGb18030_2005(void*);
int APS5_VABI sceCesSJisUcsProfileSetSbcs(void*, const void*);
int APS5_VABI sceCesEucJpUcsProfileSetSbcs(void*, const void*);
int APS5_VABI sceCesEucKrUcsProfileSetSbcs(void*, const void*);
int APS5_VABI sceCesUhcUcsProfileSetSbcs(void*, const void*);
int APS5_VABI sceCesEucCnUcsProfileSetSbcs(void*, const void*);
int APS5_VABI sceCesGbUcsProfileSetSbcs(void*, const void*);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
int APS5_VABI sceCesMbcsUcsContextInitCopy(void*, const void*);
int APS5_VABI sceCesMbcsUcsSetMbcsReplacementCharCode(void*, std::uint32_t);
int APS5_VABI sceCesMbcsUcsSetMbcsReplacementCharUCode(void*, std::uint32_t);
int APS5_VABI sceCesMbcsStrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8StrToMbcsStr(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8StrGetMbcsLen(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesMbcToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToMbc(const std::uint8_t*, std::uint32_t, std::uint32_t*, const void*, std::uint8_t*, std::uint32_t, std::uint32_t*);
}

namespace {

constexpr int invalidParameter = static_cast<int>(0x805C0001);
constexpr int invalidProfile = static_cast<int>(0x805C0004);
constexpr int outOfCodeSpace = static_cast<int>(0x805C0015);
constexpr int invalidEncode = static_cast<int>(0x805C0014);
constexpr int unassignedCode = static_cast<int>(0x805C0020);

void Require(bool value) { if (!value) std::abort(); }

struct Sheet {
    alignas(8) std::uint8_t bytes[64];
};

int Convert(void* context, const char* mbcs, std::uint8_t* utf8, std::uint32_t utf8Max, std::uint32_t& produced) {
    std::uint32_t used = 0;
    return sceCesMbcsStrToUtf8Str(context, reinterpret_cast<const std::uint8_t*>(mbcs), static_cast<std::uint32_t>(std::strlen(mbcs)), &used, utf8, utf8Max, &produced);
}

int Encode(void* context, const char* utf8, std::uint8_t* mbcs, std::uint32_t mbcsMax, std::uint32_t& produced) {
    std::uint32_t used = 0;
    return sceCesUtf8StrToMbcsStr(context, reinterpret_cast<const std::uint8_t*>(utf8), static_cast<std::uint32_t>(std::strlen(utf8)), &used, mbcs, mbcsMax, &produced);
}

void RequireConverts(void* context, const char* mbcs, const char* utf8) {
    std::uint8_t out[32] = {};
    std::uint32_t produced = 0;
    Require(Convert(context, mbcs, out, sizeof(out), produced) == 0);
    Require(produced == std::strlen(utf8) && std::memcmp(out, utf8, produced) == 0);
    std::uint8_t back[32] = {};
    Require(Encode(context, utf8, back, sizeof(back), produced) == 0);
    Require(produced == std::strlen(mbcs) && std::memcmp(back, mbcs, produced) == 0);
}

void CheckShiftJis() {
    Sheet sheet{};
    std::memset(sheet.bytes, 0xA5, sizeof(sheet.bytes));
    Require(sceCesUcsProfileInitSJis1997Cp932(sheet.bytes) == sheet.bytes);
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesMbcsUcsContextInit(context, sheet.bytes) == 0);
    RequireConverts(context, "\x5C\xB6\x41", "\x5C\xEF\xBD\xB6\x41");
    Require(sceCesSJisUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileJisX0201()) == 0);
    RequireConverts(context, "\x5C\xB6\x41\x7E\x82\xA0", "\xC2\xA5\xEF\xBD\xB6\x41\xE2\x80\xBE\xE3\x81\x82");
    std::uint8_t out[8] = {};
    std::uint32_t produced = 0;
    Require(Encode(context, "\x5C", out, sizeof(out), produced) == 0 && produced == 2 && std::memcmp(out, "\x81\x5F", 2) == 0);
    Require(Encode(context, "~", out, sizeof(out), produced) == unassignedCode);
    Require(sceCesSJisUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileAscii()) == 0);
    RequireConverts(context, "\x5C\x41\x82\xA0", "\x5C\x41\xE3\x81\x82");
    Require(Convert(context, "\xB6", out, sizeof(out), produced) == unassignedCode);
    Require(Encode(context, "\xEF\xBD\xB6", out, sizeof(out), produced) == unassignedCode);
    for (const std::uint8_t* other : {sceCesRefersUcsProfileIso8859_1(), sceCesRefersUcsProfileKsX1003(), sceCesRefersUcsProfileGbT1988(), sceCesRefersUcsProfileJisX0201Katakana(), sceCesRefersUcsProfileCp1252()}) {
        Require(sceCesSJisUcsProfileSetSbcs(sheet.bytes, other) == invalidProfile);
    }
    Require(sceCesSJisUcsProfileSetSbcs(sheet.bytes, nullptr) == invalidProfile);
    Require(sceCesSJisUcsProfileSetSbcs(nullptr, sceCesRefersUcsProfileAscii()) == invalidProfile);
    Require(sceCesSJisUcsProfileSetSbcs(sheet.bytes, sheet.bytes) == invalidProfile);
    Require(sceCesEucJpUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileAscii()) == invalidProfile);
    RequireConverts(context, "\x5C\x41", "\x5C\x41");
    Require(Convert(context, "\xB6", out, sizeof(out), produced) == unassignedCode);
    Require(sceCesUcsProfileInitSJis1997Cp932(sheet.bytes) == sheet.bytes);
    RequireConverts(context, "\x5C\xB6", "\x5C\xEF\xBD\xB6");
    Require(sceCesSJisUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileJisX0201()) == 0);
    Sheet copy{};
    std::memcpy(copy.bytes, sheet.bytes, 8);
    alignas(8) std::uint8_t copied[64] = {};
    Require(sceCesMbcsUcsContextInit(copied, copy.bytes) == 0);
    RequireConverts(copied, "\x5C\xB6", "\xC2\xA5\xEF\xBD\xB6");
    RequireConverts(context, "\x5C\xB6", "\xC2\xA5\xEF\xBD\xB6");
    Require(sceCesUcsProfileInitSJis1997Cp932(copy.bytes) == copy.bytes);
    RequireConverts(copied, "\x5C\xB6", "\x5C\xEF\xBD\xB6");
    RequireConverts(context, "\x5C\xB6", "\xC2\xA5\xEF\xBD\xB6");
}

void CheckEucFamilies() {
    Sheet sheet{};
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesUcsProfileInitEucJpCp51932(sheet.bytes) == sheet.bytes);
    Require(sceCesMbcsUcsContextInit(context, sheet.bytes) == 0);
    RequireConverts(context, "\x5C\x8E\xB6", "\x5C\xEF\xBD\xB6");
    Require(sceCesEucJpUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileJisX0201Roman()) == 0);
    RequireConverts(context, "\x5C\x8E\xB6\xA4\xA2", "\xC2\xA5\xEF\xBD\xB6\xE3\x81\x82");
    std::uint8_t out[8] = {};
    std::uint32_t produced = 0;
    Require(Encode(context, "\x5C", out, sizeof(out), produced) == 0 && produced == 2 && std::memcmp(out, "\xA1\xC0", 2) == 0);
    Require(sceCesEucJpUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileJisX0201()) == invalidProfile);
    Require(sceCesSJisUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileAscii()) == invalidProfile);
    Require(sceCesUcsProfileInitEucKr(sheet.bytes) == sheet.bytes);
    Require(sceCesEucKrUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileKsX1003()) == 0);
    RequireConverts(context, "\x5C\xB0\xA1", "\xE2\x82\xA9\xEA\xB0\x80");
    Require(sceCesUcsProfileInitUhc(sheet.bytes) == sheet.bytes);
    Require(sceCesUhcUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileKsX1003()) == 0);
    RequireConverts(context, "\x5C\xB0\xA1", "\xE2\x82\xA9\xEA\xB0\x80");
    Require(sceCesEucKrUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileKsX1003()) == invalidProfile);
    Require(sceCesUhcUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileJisX0201Roman()) == invalidProfile);
    Require(sceCesUcsProfileInitEucCnGb2312(sheet.bytes) == sheet.bytes);
    Require(sceCesEucCnUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileGbT1988()) == 0);
    RequireConverts(context, "\x24\xB0\xA1", "\xC2\xA5\xE5\x95\x8A");
    Require(sceCesGbUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileGbT1988()) == invalidProfile);
    Require(sceCesUcsProfileInitGbkCp936(sheet.bytes) == sheet.bytes);
    Require(sceCesGbUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileGbT1988()) == 0);
    RequireConverts(context, "\x24\xB0\xA1", "\xC2\xA5\xE5\x95\x8A");
    Require(sceCesUcsProfileInitGb18030_2000(sheet.bytes) == sheet.bytes);
    Require(sceCesGbUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileGbT1988()) == 0);
    RequireConverts(context, "\x24\xB0\xA1\x81\x30\x81\x30", "\xC2\xA5\xE5\x95\x8A\xC2\x80");
    Require(sceCesUcsProfileInitGb18030_2005(sheet.bytes) == sheet.bytes);
    Require(sceCesGbUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileGbT1988()) == 0);
    RequireConverts(context, "\x24\xB0\xA1", "\xC2\xA5\xE5\x95\x8A");
    Require(sceCesEucCnUcsProfileSetSbcs(sheet.bytes, sceCesRefersUcsProfileGbT1988()) == invalidProfile);
}

void CheckReplacement() {
    Sheet sheet{};
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesUcsProfileInitSJis1997Cp932(sheet.bytes) == sheet.bytes);
    Require(sceCesMbcsUcsContextInit(context, sheet.bytes) == 0);
    std::uint8_t out[16] = {};
    std::uint32_t produced = 0;
    for (const std::uint32_t code : {0x3Fu, 0x8148u, 0x81A6u, 0xFFu, 0u}) {
        Require(sceCesMbcsUcsSetMbcsReplacementCharCode(context, code) == 0);
        Require(Encode(context, "A\xE0\xB8\x81" "B", out, sizeof(out), produced) == unassignedCode);
    }
    for (const std::uint32_t code : {0x100u, 0x10000u}) Require(sceCesMbcsUcsSetMbcsReplacementCharCode(context, code) == outOfCodeSpace);
    for (const std::uint32_t code : {0x3Fu, 0x3013u, 0u}) {
        Require(sceCesMbcsUcsSetMbcsReplacementCharUCode(context, code) == 0);
        Require(Convert(context, "A\xFF" "B", out, sizeof(out), produced) == invalidEncode);
    }
    Require(sceCesMbcsUcsSetMbcsReplacementCharUCode(context, 0xFFFD) == unassignedCode);
    Require(sceCesMbcsUcsSetMbcsReplacementCharUCode(context, 0x110000) == invalidEncode);
    Require(sceCesMbcsUcsSetMbcsReplacementCharUCode(context, 0xD800) == outOfCodeSpace);
    alignas(8) std::uint8_t copy[64] = {};
    Require(sceCesMbcsUcsContextInitCopy(copy, context) == 0);
    Require(Encode(copy, "\xE0\xB8\x81", out, sizeof(out), produced) == unassignedCode);
    Require(sceCesMbcsUcsSetMbcsReplacementCharUCode(nullptr, 0x3F) == invalidParameter);
    Require(sceCesMbcsUcsSetMbcsReplacementCharCode(nullptr, 0x3F) == invalidParameter);
    const void* bad = nullptr;
    alignas(8) std::uint8_t broken[64] = {};
    std::memcpy(broken, &bad, sizeof(bad));
    Require(sceCesMbcsUcsSetMbcsReplacementCharCode(broken, 0x3F) == invalidParameter);
    Require(Convert(context, "\x82\xA0", out, sizeof(out), produced) == 0 && produced == 3);
}

}

int main() {
    CheckShiftJis();
    CheckEucFamilies();
    CheckReplacement();
    return 0;
}
