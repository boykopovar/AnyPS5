#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

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

void RequireEncodes(void* context, const char* utf8, const char* mbcs, std::uint32_t size) {
    std::uint8_t out[32] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesUtf8StrToMbcsStr(context, Bytes(utf8), static_cast<std::uint32_t>(std::strlen(utf8)), &used, out, sizeof(out), &produced) == 0);
    Require(produced == size && std::memcmp(out, mbcs, size) == 0);
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

void RequireUtf32Encode(void* context, std::uint32_t first, std::uint32_t second, int error, std::uint32_t consumed, const char* mbcs, std::uint32_t size) {
    const std::uint32_t utf32[2] = {first, second};
    std::uint8_t out[16] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesUtf32StrToMbcsStr(context, utf32, 2, &used, out, sizeof(out), &produced) == error);
    Require(used == consumed && produced == size && std::memcmp(out, mbcs, size) == 0);
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
    RequireRoundTrip(context, "\xA1\xB1", 2, "\xEF\xBF\xA3");
    RequireRoundTrip(context, "\xA1\xEF", 2, "\xEF\xBF\xA5");
    RequireRoundTrip(context, "\xA2\xAF\xA2\xB0\xA2\xB1\xA2\xB2", 8, "\xEF\xBC\x87\xEF\xBC\x82\xEF\xBC\x8D\xEF\xBD\x9E");
    RequireEncodes(context, "\xC2\xA5\xE2\x80\xBE", "\xA1\xEF\xA1\xB1", 4);
    RequireRoundTrip(context, "\x8F\xAA\xAF", 3, "\xC4\x8A");
    RequireRoundTrip(context, "\x8F\xA1\xA2", 3, "\xE4\xB8\x82");
    RequireEncodes(context, "\xE4\xB8\x82", "\x8F\xA1\xA2", 3);
    {
        std::uint8_t out[8] = {};
        std::uint32_t used = 0;
        std::uint32_t produced = 0;
        Require(Decode(context, "\x8F\xB0\xA1", 3, out, sizeof(out), used, produced) == 0 && produced == 3 && std::memcmp(out, "\xE4\xB8\x82", 3) == 0);
    }
    for (const char* combining : {"\xA4\xF7", "\xA5\xF7", "\xA6\xF8", "\xAB\xC4"}) RequireDecodeFails(context, combining, 2, unassignedCode);
    RequireUtf32Encode(context, 0x304B, 0x309A, unassignedCode, 1, "\xA4\xAB", 2);
    RequireUtf32Encode(context, 0x00E6, 0x0300, 0, 2, "\xA9\xDC\xAB\xDC", 4);
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
    RequireRoundTrip(context, "\x81\x50\x81\x8F", 4, "\xEF\xBF\xA3\xEF\xBF\xA5");
    RequireRoundTrip(context, "\x81\xAD\x81\xAE\x81\xAF\x81\xB0", 8, "\xEF\xBC\x87\xEF\xBC\x82\xEF\xBC\x8D\xEF\xBD\x9E");
    RequireEncodes(context, "\xC2\xA5\xE2\x80\xBE", "\x81\x8F\x81\x50", 4);
    RequireEncodes(context, "\xE4\xB8\x82", "\xF0\x41", 2);
    RequireEncodeFails(context, "\xC4\x8A", unassignedCode);
    for (const char* combining : {"\x82\xF5", "\x83\x97", "\x86\x63"}) RequireDecodeFails(context, combining, 2, unassignedCode);
    RequireUtf32Encode(context, 0x304B, 0x309A, unassignedCode, 1, "\x82\xA9", 2);
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
