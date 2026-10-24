#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

extern "C" {
void* APS5_VABI sceCesUcsProfileInitIso2022JpRfc1468(void*);
void* APS5_VABI sceCesUcsProfileInitIso2022JpCp50221(void*);
void* APS5_VABI sceCesUcsProfileInitIso2022JpCp50222(void*);
void* APS5_VABI sceCesUcsProfileInitEucJpX0208Ss2Ss3(void*);
void* APS5_VABI sceCesUcsProfileInitSJis1997Cp932(void*);
int APS5_VABI sceCesIso2022UcsContextInit(void*, const void*);
int APS5_VABI sceCesIso2022UcsContextInitCopy(void*, const void*);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
int APS5_VABI sceCesUtf8StrToIso2022Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8StrGetIso2022Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesIso2022StrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesIso2022StrGetUtf8Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUcs2StrToIso2022Str(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUcs2StrGetIso2022Len(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesIso2022StrToUcs2Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesIso2022StrGetUcs2Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUtf16StrToIso2022Str(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16StrGetIso2022Len(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesIso2022StrToUtf16Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesIso2022StrGetUtf16Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUtf32StrToIso2022Str(void*, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32StrGetIso2022Len(void*, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesIso2022StrToUtf32Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesIso2022StrGetUtf32Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesEucJpToUcs2(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*);
}

namespace {

constexpr int invalidParameter = static_cast<int>(0x805C0001);
constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
constexpr int invalidEncode = static_cast<int>(0x805C0014);
constexpr int unassignedCode = static_cast<int>(0x805C0020);
constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);
constexpr int dstBufferEnd = static_cast<int>(0x805C0031);

void Require(bool value) { if (!value) std::abort(); }

const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

int Encode(void* context, const char* utf8, std::uint8_t* iso, std::uint32_t isoMax, std::uint32_t& used, std::uint32_t& produced) {
    return sceCesUtf8StrToIso2022Str(context, Bytes(utf8), static_cast<std::uint32_t>(std::strlen(utf8)), &used, iso, isoMax, &produced);
}

int Decode(void* context, const char* iso, std::uint32_t isoMax, std::uint8_t* utf8, std::uint32_t utf8Max, std::uint32_t& used, std::uint32_t& produced) {
    return sceCesIso2022StrToUtf8Str(context, Bytes(iso), isoMax, &used, utf8, utf8Max, &produced);
}

void RequireEncodes(void* context, const char* utf8, const char* iso, std::uint32_t isoSize) {
    std::uint8_t out[64] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(Encode(context, utf8, out, sizeof(out), used, produced) == 0);
    Require(used == std::strlen(utf8) && produced == isoSize && std::memcmp(out, iso, isoSize) == 0);
    Require(sceCesUtf8StrGetIso2022Len(context, Bytes(utf8), static_cast<std::uint32_t>(std::strlen(utf8)), &used, &produced) == 0 && produced == isoSize);
    std::uint8_t back[64] = {};
    Require(Decode(context, iso, isoSize, back, sizeof(back), used, produced) == 0);
    Require(used == isoSize && produced == std::strlen(utf8) && std::memcmp(back, utf8, produced) == 0);
    Require(sceCesIso2022StrGetUtf8Len(context, Bytes(iso), isoSize, &used, &produced) == 0 && produced == std::strlen(utf8));
}

void RequireDecodes(void* context, const char* iso, std::uint32_t isoSize, const char* utf8) {
    std::uint8_t out[64] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(Decode(context, iso, isoSize, out, sizeof(out), used, produced) == 0);
    Require(used == isoSize && produced == std::strlen(utf8) && std::memcmp(out, utf8, produced) == 0);
}

void RequireDecodeFails(void* context, const char* iso, std::uint32_t isoSize, int error) {
    std::uint8_t out[64] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(Decode(context, iso, isoSize, out, sizeof(out), used, produced) == error);
}

void RequireEncodeFails(void* context, const char* utf8, int error) {
    std::uint8_t out[64] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(Encode(context, utf8, out, sizeof(out), used, produced) == error);
}

void CheckRfc1468() {
    alignas(8) std::uint8_t sheet[64] = {};
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesUcsProfileInitIso2022JpRfc1468(sheet) == sheet);
    Require(sceCesIso2022UcsContextInit(context, sheet) == 0);
    RequireEncodes(context, "A", "A", 1);
    RequireEncodes(context, "", "", 0);
    RequireEncodes(context, "\xE3\x81\x82", "\x1B$B\x24\x22\x1B(B", 8);
    RequireEncodes(context, "A\xE3\x81\x82\xC2\xA5" "B\n", "A\x1B$B\x24\x22\x1B(J\x5C" "B\x1B(B\n", 15);
    RequireEncodes(context, "\xE2\x80\xBE\x7E", "\x1B(J\x7E\x1B(B\x7E", 8);
    RequireEncodes(context, "\xE3\x81\x82\xE3\x81\x84", "\x1B$B\x24\x22\x24\x24\x1B(B", 10);
    RequireDecodes(context, "\x1B$@\x24\x22\x1B(B", 8, "\xE3\x81\x82");
    RequireDecodeFails(context, "\x1B$B\x24\x22", 5, invalidEncode);
    RequireDecodeFails(context, "\x1B$B\x24\x22\x1B(J", 8, invalidEncode);
    RequireDecodeFails(context, "\x1B(J\x41", 4, invalidEncode);
    RequireDecodes(context, "\x1B(J\x41\x1B(B", 7, "A");
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesIso2022StrGetUtf8Len(context, Bytes("\x1B$B\x24\x22"), 5, &used, &produced) == invalidEncode);
    for (const char* reserved : {"\x1B", "\x0E", "\x0F", "A\x1B" "B"}) RequireEncodeFails(context, reserved, unassignedCode);
    RequireDecodes(context, "\x1B(J\x5C\x7E\x41\x1B(B", 9, "\xC2\xA5\xE2\x80\xBE\x41");
    RequireDecodes(context, "\x1B$B\x24\x22\n\x24\x24\x1B(B", 11, "\xE3\x81\x82\n\xE3\x81\x84");
    RequireEncodeFails(context, "\xEF\xBD\xB6", unassignedCode);
    RequireEncodeFails(context, "\xF0\x9F\x98\x80", unassignedCode);
    RequireDecodeFails(context, "\x1B(I\x36", 4, invalidEncode);
    RequireDecodeFails(context, "\x0E\x36\x0F", 3, invalidEncode);
    RequireDecodeFails(context, "\x1B$(D\x30\x21", 6, invalidEncode);
    RequireDecodeFails(context, "\x1B$", 2, srcBufferEnd);
    RequireDecodeFails(context, "\x1B$B\x24", 4, srcBufferEnd);
    RequireDecodeFails(context, "\x1B(Z", 3, invalidEncode);
    RequireDecodeFails(context, "\xB6", 1, invalidEncode);
    RequireDecodeFails(context, "\x1B$B\x29\x21", 5, unassignedCode);
    RequireDecodeFails(context, "\x1B$B\x24\x7F", 5, invalidEncode);
    std::uint8_t out[4] = {};
    Require(Encode(context, "\xE3\x81\x82", out, sizeof(out), used, produced) == dstBufferEnd);
    Require(Encode(context, "A", nullptr, 0, used, produced) == invalidDstBuffer);
    Require(sceCesUtf8StrToIso2022Str(context, nullptr, 1, &used, out, sizeof(out), &produced) == invalidSrcBuffer);
    Require(sceCesIso2022StrToUtf8Str(context, nullptr, 1, &used, out, sizeof(out), &produced) == invalidSrcBuffer);
    Require(sceCesIso2022StrToUtf8Str(context, Bytes("A"), 1, &used, nullptr, 0, &produced) == invalidDstBuffer);
    Require(sceCesUtf8StrToIso2022Str(nullptr, Bytes("A"), 1, &used, out, sizeof(out), &produced) == invalidParameter);
    Require(sceCesIso2022UcsContextInit(nullptr, sheet) == invalidParameter);
    alignas(8) std::uint8_t copy[64] = {};
    Require(sceCesIso2022UcsContextInitCopy(copy, context) == 0);
    RequireEncodes(copy, "\xE3\x81\x82", "\x1B$B\x24\x22\x1B(B", 8);
    Require(sceCesIso2022UcsContextInitCopy(copy, nullptr) == invalidParameter);
    Require(sceCesMbcsStrToUtf8Str(context, Bytes("A"), 1, &used, out, sizeof(out), &produced) == invalidParameter);
    alignas(8) std::uint8_t sjis[64] = {};
    alignas(8) std::uint8_t other[64] = {};
    Require(sceCesUcsProfileInitSJis1997Cp932(sjis) == sjis);
    Require(sceCesMbcsUcsContextInit(other, sjis) == 0);
    Require(sceCesUtf8StrToIso2022Str(other, Bytes("A"), 1, &used, out, sizeof(out), &produced) == invalidParameter);
    Require(sceCesIso2022StrToUtf8Str(other, Bytes("A"), 1, &used, out, sizeof(out), &produced) == invalidParameter);
}

void CheckCodePages() {
    alignas(8) std::uint8_t sheet[64] = {};
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesUcsProfileInitIso2022JpCp50221(sheet) == sheet);
    Require(sceCesIso2022UcsContextInit(context, sheet) == 0);
    RequireEncodes(context, "\xEF\xBD\xB6", "\x1B(I\x36\x1B(B", 7);
    RequireEncodes(context, "\xEF\xBD\xB6\xE3\x81\x82", "\x1B(I\x36\x1B$B\x24\x22\x1B(B", 12);
    RequireDecodeFails(context, "\x0E\x36\x0F", 3, invalidEncode);
    RequireDecodeFails(context, "\x1B(I\x60", 4, unassignedCode);
    RequireDecodes(context, "\x1B$B\x24\x22", 5, "\xE3\x81\x82");
    for (const char* reserved : {"\x1B", "\x0E", "\x0F"}) RequireEncodeFails(context, reserved, unassignedCode);
    alignas(8) std::uint8_t euc[64] = {};
    Require(sceCesUcsProfileInitEucJpX0208Ss2Ss3(euc) == euc);
    std::uint16_t expected = 0;
    std::uint32_t size = 0;
    Require(sceCesEucJpToUcs2(euc, Bytes("\x8F\xB0\xA1"), 3, &size, &expected) == 0 && size == 3);
    std::uint16_t ucs2[4] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesIso2022StrToUcs2Str(context, Bytes("\x1B$(D\x30\x21"), 6, &used, ucs2, 4, &produced) == 0);
    Require(used == 6 && produced == 1 && ucs2[0] == expected);
    std::uint8_t iso[16] = {};
    Require(sceCesUcs2StrToIso2022Str(context, ucs2, 1, &used, iso, sizeof(iso), &produced) == 0);
    Require(used == 1 && produced == 9 && std::memcmp(iso, "\x1B$(D\x30\x21\x1B(B", 9) == 0);
    Require(sceCesUcsProfileInitIso2022JpRfc1468(sheet) == sheet);
    Require(sceCesUcs2StrToIso2022Str(context, ucs2, 1, &used, iso, sizeof(iso), &produced) == unassignedCode);
    Require(sceCesUcsProfileInitIso2022JpCp50222(sheet) == sheet);
    RequireEncodes(context, "\xEF\xBD\xB6", "\x0E\x36\x0F", 3);
    RequireEncodes(context, "\xEF\xBD\xB6\xE3\x81\x82\xEF\xBD\xB6" "A", "\x0E\x36\x0F\x1B$B\x24\x22\x0E\x36\x0F\x1B(B" "A", 15);
    RequireDecodes(context, "\x1B(I\x36\x1B(B", 7, "\xEF\xBD\xB6");
    RequireDecodes(context, "\x0E\x36\x0F" "A", 4, "\xEF\xBD\xB6" "A");
    RequireDecodes(context, "\x0E\x36", 2, "\xEF\xBD\xB6");
    for (const char* reserved : {"\x1B", "\x0E", "\x0F"}) RequireEncodeFails(context, reserved, unassignedCode);
}

void CheckWideUnits() {
    alignas(8) std::uint8_t sheet[64] = {};
    alignas(8) std::uint8_t context[64] = {};
    Require(sceCesUcsProfileInitIso2022JpRfc1468(sheet) == sheet);
    Require(sceCesIso2022UcsContextInit(context, sheet) == 0);
    const std::uint16_t ucs2[] = {0x41, 0x3042, 0};
    const std::uint32_t utf32[] = {0x41, 0x3042, 0};
    std::uint8_t iso[32] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesUcs2StrToIso2022Str(context, ucs2, 2, &used, iso, sizeof(iso), &produced) == 0 && used == 2 && produced == 9);
    Require(std::memcmp(iso, "A\x1B$B\x24\x22\x1B(B", 9) == 0);
    Require(sceCesUcs2StrGetIso2022Len(context, ucs2, 2, &used, &produced) == 0 && produced == 9);
    Require(sceCesUtf16StrToIso2022Str(context, ucs2, 2, &used, iso, sizeof(iso), &produced) == 0 && produced == 9);
    Require(sceCesUtf16StrGetIso2022Len(context, ucs2, 2, &used, &produced) == 0 && produced == 9);
    Require(sceCesUtf32StrToIso2022Str(context, utf32, 2, &used, iso, sizeof(iso), &produced) == 0 && produced == 9);
    Require(sceCesUtf32StrGetIso2022Len(context, utf32, 2, &used, &produced) == 0 && produced == 9);
    std::uint16_t units[8] = {};
    std::uint32_t wide[8] = {};
    Require(sceCesIso2022StrToUcs2Str(context, iso, 9, &used, units, 8, &produced) == 0 && used == 9 && produced == 2 && units[0] == 0x41 && units[1] == 0x3042);
    Require(sceCesIso2022StrGetUcs2Len(context, iso, 9, &used, &produced) == 0 && produced == 2);
    Require(sceCesIso2022StrToUtf16Str(context, iso, 9, &used, units, 8, &produced) == 0 && produced == 2 && units[1] == 0x3042);
    Require(sceCesIso2022StrGetUtf16Len(context, iso, 9, &used, &produced) == 0 && produced == 2);
    Require(sceCesIso2022StrToUtf32Str(context, iso, 9, &used, wide, 8, &produced) == 0 && produced == 2 && wide[1] == 0x3042);
    Require(sceCesIso2022StrGetUtf32Len(context, iso, 9, &used, &produced) == 0 && produced == 2);
    Require(sceCesIso2022StrToUcs2Str(context, iso, 9, &used, units, 1, &produced) == dstBufferEnd);
    Require(sceCesIso2022StrToUcs2Str(context, iso, 9, &used, nullptr, 0, &produced) == invalidDstBuffer);
    Require(sceCesUcs2StrToIso2022Str(context, ucs2, 2, &used, nullptr, 0, &produced) == invalidDstBuffer);
}

}

int main() {
    CheckRfc1468();
    CheckCodePages();
    CheckWideUnits();
    return 0;
}
