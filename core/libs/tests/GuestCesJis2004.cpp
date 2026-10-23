#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
void* APS5_VABI sceCesUcsProfileInitEucJis2004(void*);
void* APS5_VABI sceCesUcsProfileInitSJis2004X0213(void*);
void* APS5_VABI sceCesUcsProfileInitEucJpCp51932(void*);
void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void*);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
int APS5_VABI sceCesMbcsStrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8StrToMbcsStr(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUtf32Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucJpToUcs2(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
}

namespace {

constexpr int invalidParameter = static_cast<int>(0x805C0001);
constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
constexpr int invalidEncode = static_cast<int>(0x805C0014);
constexpr int unassignedCode = static_cast<int>(0x805C0020);

void Require(bool value) { if (!value) std::abort(); }

const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

int Decode(void* context, const char* mbcs, std::uint32_t size, std::uint8_t* utf8, std::uint32_t max, std::uint32_t& used, std::uint32_t& produced) {
    return sceCesMbcsStrToUtf8Str(context, Bytes(mbcs), size, &used, utf8, max, &produced);
}

void RequireRoundTrip(void* context, const char* mbcs, std::uint32_t size, const char* utf8) {
    std::uint8_t out[32] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(Decode(context, mbcs, size, out, sizeof(out), used, produced) == 0);
    Require(used == size && produced == std::strlen(utf8) && std::memcmp(out, utf8, produced) == 0);
    std::uint8_t back[32] = {};
    Require(sceCesUtf8StrToMbcsStr(context, Bytes(utf8), static_cast<std::uint32_t>(std::strlen(utf8)), &used, back, sizeof(back), &produced) == 0);
    Require(used == std::strlen(utf8) && produced == size && std::memcmp(back, mbcs, size) == 0);
}

void RequireDecodeFails(void* context, const char* mbcs, std::uint32_t size, int error) {
    std::uint8_t out[32] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(Decode(context, mbcs, size, out, sizeof(out), used, produced) == error);
}

void RequireEncodeFails(void* context, const char* utf8, int error) {
    std::uint8_t out[32] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesUtf8StrToMbcsStr(context, Bytes(utf8), static_cast<std::uint32_t>(std::strlen(utf8)), &used, out, sizeof(out), &produced) == error);
}

void CheckEuc() {
    alignas(8) std::uint8_t sheet[64] = {};
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesUcsProfileInitEucJis2004(sheet) == sheet);
    Require(sceCesMbcsUcsContextInit(context, sheet) == 0);
    RequireRoundTrip(context, "A", 1, "A");
    RequireRoundTrip(context, "\xA4\xA2", 2, "\xE3\x81\x82");
    RequireRoundTrip(context, "\xAE\xA1", 2, "\xE4\xBF\xB1");
    RequireRoundTrip(context, "\x8F\xA1\xA1", 3, "\xF0\xA0\x82\x89");
    RequireRoundTrip(context, "\x8E\xB6", 2, "\xEF\xBD\xB6");
    RequireRoundTrip(context, "A\xA4\xA2\x8E\xB6\x8F\xA1\xA1" "B", 9, "A\xE3\x81\x82\xEF\xBD\xB6\xF0\xA0\x82\x89" "B");
    RequireRoundTrip(context, "\xA1\xA4", 2, "\xEF\xBC\x8C");
    RequireDecodeFails(context, "\xA4\xF7", 2, unassignedCode);
    RequireDecodeFails(context, "\xA4", 1, srcBufferEnd);
    RequireDecodeFails(context, "\x8F\xA1", 2, srcBufferEnd);
    RequireDecodeFails(context, "\xA4\x41", 2, invalidEncode);
    RequireDecodeFails(context, "\x80", 1, invalidEncode);
    RequireDecodeFails(context, "\x8E\x41", 2, invalidEncode);
    RequireEncodeFails(context, "\xE0\xB8\x81", unassignedCode);
    RequireEncodeFails(context, "\xE3\x81\x8B\xE3\x82\x9A", unassignedCode);
    std::uint32_t wide[4] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesMbcsStrToUtf32Str(context, Bytes("\x8F\xA1\xA1"), 3, &used, wide, 4, &produced) == 0 && produced == 1 && wide[0] == 0x20089);
    std::uint8_t utf8[8] = {};
    Require(sceCesMbcToUtf8(sheet, Bytes("\xAE\xA1"), 2, &used, utf8, sizeof(utf8), &produced) == 0 && used == 2 && produced == 3 && std::memcmp(utf8, "\xE4\xBF\xB1", 3) == 0);
    std::uint16_t ucs2 = 0;
    Require(sceCesEucJpToUcs2(sheet, Bytes("\xA4\xA2"), 2, &used, &ucs2) == invalidParameter);
    alignas(8) std::uint8_t euc[64] = {};
    Require(sceCesUcsProfileInitEucJpCp51932(euc) == euc);
    Require(sceCesMbcsUcsContextInit(context, euc) == 0);
    RequireDecodeFails(context, "\xAE\xA1", 2, unassignedCode);
}

void CheckShiftJis() {
    alignas(8) std::uint8_t sheet[64] = {};
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesUcsProfileInitSJis2004X0213(sheet) == sheet);
    Require(sceCesMbcsUcsContextInit(context, sheet) == 0);
    RequireRoundTrip(context, "A", 1, "A");
    RequireRoundTrip(context, "\x82\xA0", 2, "\xE3\x81\x82");
    RequireRoundTrip(context, "\x87\x9F", 2, "\xE4\xBF\xB1");
    RequireRoundTrip(context, "\xF0\x40", 2, "\xF0\xA0\x82\x89");
    RequireRoundTrip(context, "\xB6", 1, "\xEF\xBD\xB6");
    RequireRoundTrip(context, "A\x82\xA0\xB6\xF0\x40" "B", 7, "A\xE3\x81\x82\xEF\xBD\xB6\xF0\xA0\x82\x89" "B");
    RequireDecodeFails(context, "\x82\xF5", 2, unassignedCode);
    RequireDecodeFails(context, "\x82", 1, srcBufferEnd);
    RequireDecodeFails(context, "\xFD", 1, invalidEncode);
    RequireDecodeFails(context, "\x82\x7F", 2, invalidEncode);
    RequireEncodeFails(context, "\xE0\xB8\x81", unassignedCode);
    alignas(8) std::uint8_t sjis[64] = {};
    Require(sceCesUcsProfileInitSJis1997Cp932(sjis) == sjis);
    Require(sceCesMbcsUcsContextInit(context, sjis) == 0);
    RequireDecodeFails(context, "\xF0\x40", 2, unassignedCode);
}

}

int main() {
    CheckEuc();
    CheckShiftJis();
    return 0;
}
