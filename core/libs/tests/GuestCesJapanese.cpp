#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

static void Require(bool value) { if (!value) std::abort(); }

static const std::uint8_t* Bytes(const char* text) { return reinterpret_cast<const std::uint8_t*>(text); }

constexpr int invalidParameter = static_cast<int>(0x805C0001);
constexpr int srcBufferEnd = static_cast<int>(0x805C0011);
constexpr int invalidEncode = static_cast<int>(0x805C0014);
constexpr int outOfCodeSpace = static_cast<int>(0x805C0015);
constexpr int unassignedCode = static_cast<int>(0x805C0020);
constexpr int dstBufferEnd = static_cast<int>(0x805C0031);
constexpr std::uint32_t jisX0208Cells = 6878;
constexpr std::uint32_t jisX0208Digest = 0xA614D6F6;
constexpr std::uint32_t jisX0212Cells = 6067;
constexpr std::uint32_t jisX0212Digest = 0x7C193A73;
const std::string unsettledCell = "libSceCesCs: the console's mapping of JIS X 0208 cell 0x2140 (U+005C or U+FF3C) is unknown";
const std::string unsettledByte = "libSceCesCs: the console's mapping of Shift-JIS bytes 0x5C and 0x7E (ASCII or JIS X 0201 Roman) is unknown";

template <typename TCall>
static bool Reports(const std::string& message, TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return error.what() == message;
    }
    return false;
}

static std::uint32_t Digest(std::uint32_t hash, std::uint16_t value) {
    hash = (hash ^ (value & 0xFF)) * 0x01000193u;
    return (hash ^ (value >> 8)) * 0x01000193u;
}

static int EucToUcs2(const void* profile, const char* bytes, std::uint32_t size, std::uint16_t& ucs2) {
    std::uint32_t used = 0;
    return sceCesEucJpToUcs2(profile, Bytes(bytes), size, &used, &ucs2);
}

static int SJisToUcs2(const void* profile, const char* bytes, std::uint32_t size, std::uint16_t& ucs2) {
    std::uint32_t used = 0;
    return sceCesSJisToUcs2(profile, Bytes(bytes), size, &used, &ucs2);
}

static bool EucEncodes(const void* profile, std::uint16_t ucs2, const char* expected) {
    std::uint8_t mbcs[4] = {};
    std::uint32_t size = 0;
    return sceCesUcs2ToEucJp(ucs2, profile, mbcs, sizeof(mbcs), &size) == 0 && size == std::strlen(expected) && std::memcmp(mbcs, expected, size) == 0;
}

static bool SJisEncodes(const void* profile, std::uint16_t ucs2, const char* expected) {
    std::uint8_t mbcs[4] = {};
    std::uint32_t size = 0;
    return sceCesUcs2ToSJis(ucs2, profile, mbcs, sizeof(mbcs), &size) == 0 && size == std::strlen(expected) && std::memcmp(mbcs, expected, size) == 0;
}

static bool EncodeLeavesOutputs(int expected, const void* profile, std::uint16_t ucs2, bool sjis) {
    std::uint8_t mbcs[4] = {0xEE, 0xEE, 0xEE, 0xEE};
    std::uint32_t size = 77;
    const int result = sjis ? sceCesUcs2ToSJis(ucs2, profile, mbcs, sizeof(mbcs), &size) : sceCesUcs2ToEucJp(ucs2, profile, mbcs, sizeof(mbcs), &size);
    return result == expected && size == (expected == static_cast<int>(0x805C0001) ? 77u : 0u) && mbcs[0] == 0xEE && mbcs[1] == 0xEE && mbcs[2] == 0xEE;
}

static bool DecodeRejects(int expected, std::uint32_t length, const void* profile, const char* bytes, std::uint32_t size, bool sjis) {
    std::uint32_t used = 77;
    std::uint16_t ucs2 = 0xEEEE;
    const int result = sjis ? sceCesSJisToUcs2(profile, Bytes(bytes), size, &used, &ucs2) : sceCesEucJpToUcs2(profile, Bytes(bytes), size, &used, &ucs2);
    if (!sjis) return result == expected && ucs2 == 0;
    return result == expected && used == length && ucs2 == 0;
}

static void SweepEucJisX0208(const void* profile) {
    std::uint32_t assigned = 0;
    std::uint32_t hash = 0x811C9DC5u;
    for (std::uint32_t lead = 0xA1; lead <= 0xFE; ++lead) {
        for (std::uint32_t trail = 0xA1; trail <= 0xFE; ++trail) {
            const std::uint8_t cell[2] = {static_cast<std::uint8_t>(lead), static_cast<std::uint8_t>(trail)};
            std::uint32_t used = 77;
            std::uint16_t ucs2 = 0xEEEE;
            std::uint16_t value = 0;
            if (lead == 0xA1 && trail == 0xC0) {
                Require(Reports(unsettledCell, [&] { sceCesEucJpToUcs2(profile, cell, 2, &used, &ucs2); }));
            } else if (const int result = sceCesEucJpToUcs2(profile, cell, 2, &used, &ucs2); result == 0) {
                Require(used == 2);
                value = ucs2;
                ++assigned;
                std::uint8_t again[4] = {};
                std::uint32_t size = 0;
                Require(sceCesUcs2ToEucJp(ucs2, profile, again, sizeof(again), &size) == 0 && size == 2 && again[0] == lead && again[1] == trail);
            } else {
                Require(result == unassignedCode && used == 2 && ucs2 == 0);
            }
            hash = Digest(hash, value);
        }
    }
    Require(assigned == jisX0208Cells && hash == jisX0208Digest);
}

static void SweepSJisJisX0208(const void* profile) {
    std::uint32_t assigned = 0;
    std::uint32_t hash = 0x811C9DC5u;
    for (int row = 1; row <= 94; ++row) {
        for (int column = 1; column <= 94; ++column) {
            std::uint16_t code = 0;
            Require(sceCesJisX0208ToSJisCode(static_cast<std::uint8_t>(row), static_cast<std::uint8_t>(column), &code) == 0);
            const std::uint8_t cell[2] = {static_cast<std::uint8_t>(code >> 8), static_cast<std::uint8_t>(code)};
            std::uint32_t used = 77;
            std::uint16_t ucs2 = 0xEEEE;
            std::uint16_t value = 0;
            if (row == 1 && column == 32) {
                Require(Reports(unsettledCell, [&] { sceCesSJisToUcs2(profile, cell, 2, &used, &ucs2); }));
            } else if (const int result = sceCesSJisToUcs2(profile, cell, 2, &used, &ucs2); result == 0) {
                Require(used == 2);
                value = ucs2;
                ++assigned;
                std::uint8_t again[4] = {};
                std::uint32_t size = 0;
                Require(sceCesUcs2ToSJis(ucs2, profile, again, sizeof(again), &size) == 0 && size == 2 && again[0] == cell[0] && again[1] == cell[1]);
            } else {
                Require(result == unassignedCode && used == 2 && ucs2 == 0);
            }
            hash = Digest(hash, value);
        }
    }
    Require(assigned == jisX0208Cells && hash == jisX0208Digest);
}

static void TestJisX0208Profiles(const void* cp932, const void* cp51932) {
    alignas(8) std::uint8_t sheets[4][256] = {};
    Require(sceCesUcsProfileInitEucJpX0208(nullptr) == nullptr && sceCesUcsProfileInitEucJpX0208Ss2(nullptr) == nullptr);
    Require(sceCesUcsProfileInitEucJpX0208Ss2Ss3(nullptr) == nullptr && sceCesUcsProfileInitSJis1997X0208(nullptr) == nullptr);
    const void* euc = sceCesUcsProfileInitEucJpX0208(sheets[0]);
    const void* ss2 = sceCesUcsProfileInitEucJpX0208Ss2(sheets[1]);
    const void* ss3 = sceCesUcsProfileInitEucJpX0208Ss2Ss3(sheets[2]);
    const void* sjis = sceCesUcsProfileInitSJis1997X0208(sheets[3]);
    Require(euc == sheets[0] && ss2 == sheets[1] && ss3 == sheets[2] && sjis == sheets[3]);
    const void* strictEuc[3] = {euc, ss2, ss3};

    std::uint16_t ucs2 = 0;
    Require(EucToUcs2(sjis, "\xA4\xA2", 2, ucs2) == invalidParameter);
    Require(SJisToUcs2(euc, "\x82\xA0", 2, ucs2) == invalidParameter);
    Require(EncodeLeavesOutputs(invalidParameter, sjis, 0x3042, false) && EncodeLeavesOutputs(invalidParameter, ss3, 0x3042, true));

    for (const void* profile : strictEuc) {
        SweepEucJisX0208(profile);
        Require(EucToUcs2(profile, "\\~", 2, ucs2) == 0 && ucs2 == 0x5C && EucEncodes(profile, 0x5C, "\\") && EucEncodes(profile, 0x7E, "~"));
        Require(EucToUcs2(profile, "\xA1\xC1", 2, ucs2) == 0 && ucs2 == 0x301C && EucEncodes(profile, 0x301C, "\xA1\xC1"));
        Require(EucToUcs2(profile, "\xA1\xC2", 2, ucs2) == 0 && ucs2 == 0x2016);
        Require(EucToUcs2(profile, "\xA1\xDD", 2, ucs2) == 0 && ucs2 == 0x2212);
        Require(EucToUcs2(profile, "\xA1\xF1", 2, ucs2) == 0 && ucs2 == 0x00A2);
        Require(EucToUcs2(profile, "\xA1\xF2", 2, ucs2) == 0 && ucs2 == 0x00A3);
        Require(EucToUcs2(profile, "\xA2\xCC", 2, ucs2) == 0 && ucs2 == 0x00AC);
        for (const std::uint16_t cp932Form : {0xFF5E, 0x2225, 0xFF0D, 0xFFE0, 0xFFE1, 0xFFE2, 0x2460, 0x2170}) Require(EncodeLeavesOutputs(unassignedCode, profile, cp932Form, false));
        Require(Reports(unsettledCell, [&] { EucEncodes(profile, 0xFF3C, ""); }));
        Require(DecodeRejects(unassignedCode, 2, profile, "\xAD\xA1", 2, false) && DecodeRejects(unassignedCode, 2, profile, "\xF9\xA1", 2, false));
        Require(DecodeRejects(unassignedCode, 2, profile, "\xA9\xA1", 2, false) && DecodeRejects(unassignedCode, 2, profile, "\xFE\xFE", 2, false));
        Require(DecodeRejects(invalidEncode, 2, profile, "\xA4\x41", 2, false) && DecodeRejects(invalidEncode, 2, profile, "\xA4\xFF", 2, false));
        Require(DecodeRejects(invalidEncode, 1, profile, "\x80", 1, false) && DecodeRejects(invalidEncode, 1, profile, "\xFF", 1, false));
        Require(DecodeRejects(srcBufferEnd, 2, profile, "\xA4\xA2", 1, false));
        std::uint8_t mbcs[4] = {0xEE, 0xEE, 0xEE, 0xEE};
        std::uint32_t size = 77;
        Require(sceCesUcs2ToEucJp(0x3042, profile, mbcs, 1, &size) == dstBufferEnd && size == 2 && mbcs[0] == 0xEE);
    }
    Require(EucToUcs2(cp51932, "\xA1\xC0", 2, ucs2) == 0 && ucs2 == 0xFF3C && EucEncodes(cp51932, 0xFF3C, "\xA1\xC0"));
    Require(EucToUcs2(cp51932, "\xA1\xC1", 2, ucs2) == 0 && ucs2 == 0xFF5E && EucToUcs2(cp51932, "\xAD\xA1", 2, ucs2) == 0 && ucs2 == 0x2460);
    Require(EucEncodes(cp51932, 0x7E8A, "\xF9\xA1") && EucEncodes(ss3, 0x7E8A, "\x8F\xD4\xE3"));
    Require(EncodeLeavesOutputs(unassignedCode, euc, 0x7E8A, false) && EncodeLeavesOutputs(unassignedCode, ss2, 0x7E8A, false));

    Require(DecodeRejects(invalidEncode, 1, euc, "\x8E\xB1", 2, false) && DecodeRejects(invalidEncode, 1, euc, "\x8E", 1, false));
    Require(DecodeRejects(invalidEncode, 1, euc, "\x8F\xB0\xA1", 3, false) && DecodeRejects(invalidEncode, 1, ss2, "\x8F\xB0\xA1", 3, false));
    Require(EncodeLeavesOutputs(unassignedCode, euc, 0xFF71, false));
    Require(EncodeLeavesOutputs(unassignedCode, euc, 0x4E02, false) && EncodeLeavesOutputs(unassignedCode, ss2, 0x4E02, false));
    for (const void* profile : {ss2, ss3}) {
        for (std::uint32_t kana = 0xA1; kana <= 0xDF; ++kana) {
            const char bytes[3] = {'\x8E', static_cast<char>(kana), 0};
            Require(EucToUcs2(profile, bytes, 2, ucs2) == 0 && ucs2 == 0xFF61 + kana - 0xA1 && EucEncodes(profile, ucs2, bytes));
        }
        Require(DecodeRejects(invalidEncode, 2, profile, "\x8E\xE0", 2, false) && DecodeRejects(invalidEncode, 2, profile, "\x8E\xA0", 2, false));
        Require(DecodeRejects(srcBufferEnd, 2, profile, "\x8E\xB1", 1, false));
    }
    std::uint32_t supplementary = 0;
    std::uint32_t hash = 0x811C9DC5u;
    for (std::uint32_t lead = 0xA1; lead <= 0xFE; ++lead) {
        for (std::uint32_t trail = 0xA1; trail <= 0xFE; ++trail) {
            const char bytes[4] = {'\x8F', static_cast<char>(lead), static_cast<char>(trail), 0};
            std::uint16_t value = 0;
            if (const int result = EucToUcs2(ss3, bytes, 3, ucs2); result == 0) {
                value = ucs2;
                ++supplementary;
                Require(EucEncodes(ss3, ucs2, ucs2 == 0x7E ? "~" : bytes));
            } else {
                Require(result == unassignedCode);
            }
            hash = Digest(hash, value);
        }
    }
    Require(supplementary == jisX0212Cells && hash == jisX0212Digest);
    Require(DecodeRejects(srcBufferEnd, 3, ss3, "\x8F\xB0", 2, false) && DecodeRejects(invalidEncode, 3, ss3, "\x8F\xB0\x41", 3, false));
    Require(EucEncodes(ss3, 0x4E02, "\x8F\xB0\xA1") && EucEncodes(ss2, 0xFF71, "\x8E\xB1"));

    std::uint8_t utf8[4] = {};
    std::uint32_t used = 0;
    std::uint32_t produced = 0;
    Require(sceCesEucJpToUtf8(euc, Bytes("\xA1\xC1"), 2, &used, utf8, sizeof(utf8), &produced) == 0 && used == 2 && produced == 3 && std::memcmp(utf8, "\xE3\x80\x9C", 3) == 0);
    alignas(8) std::uint8_t context[256] = {};
    std::uint16_t wide[8] = {};
    Require(sceCesMbcsUcsContextInit(context, ss2) == 0);
    Require(sceCesMbcsStrToUcs2Str(context, Bytes("A\xA4\xA2\x8E\xB1"), 5, &used, wide, 8, &produced) == 0 && used == 5 && produced == 3 && wide[1] == 0x3042 && wide[2] == 0xFF71);
    Require(sceCesMbcsUcsContextInit(context, euc) == 0);
    Require(sceCesMbcsStrToUcs2Str(context, Bytes("A\xA4\xA2\x8E\xB1"), 5, &used, wide, 8, &produced) == invalidEncode);

    SweepSJisJisX0208(sjis);
    Require(SJisToUcs2(sjis, "\x81\x60", 2, ucs2) == 0 && ucs2 == 0x301C && SJisEncodes(sjis, 0x301C, "\x81\x60"));
    Require(SJisToUcs2(sjis, "\x81\x91", 2, ucs2) == 0 && ucs2 == 0x00A2 && SJisEncodes(sjis, 0x00AC, "\x81\xCA"));
    for (const std::uint16_t cp932Form : {0xFF5E, 0x2225, 0xFF0D, 0xFFE0, 0xFFE1, 0xFFE2, 0x2460, 0x2170, 0x7E8A, 0x4E02}) Require(EncodeLeavesOutputs(unassignedCode, sjis, cp932Form, true));
    for (const char* removed : {"\x87\x40", "\xED\x40", "\xEE\xFC", "\xFA\x40", "\xFC\x4B", "\xF0\x40", "\xEF\xFC"}) Require(DecodeRejects(unassignedCode, 2, sjis, removed, 2, true));
    Require(SJisToUcs2(cp932, "\x87\x40", 2, ucs2) == 0 && ucs2 == 0x2460 && SJisToUcs2(cp932, "\xFA\x40", 2, ucs2) == 0 && ucs2 == 0x2170);
    Require(SJisToUcs2(cp932, "\x81\x5F", 2, ucs2) == 0 && ucs2 == 0xFF3C && SJisToUcs2(cp932, "\\", 1, ucs2) == 0 && ucs2 == 0x5C);
    Require(DecodeRejects(invalidEncode, 2, sjis, "\x82\x7F", 2, true) && DecodeRejects(invalidEncode, 2, sjis, "\x82\xFD", 2, true));
    Require(DecodeRejects(srcBufferEnd, 2, sjis, "\x82\xA0", 1, true));
    for (std::uint32_t byte = 0; byte <= 0xFF; ++byte) {
        const char bytes[2] = {static_cast<char>(byte), 0};
        if (byte == 0x5C || byte == 0x7E) {
            Require(Reports(unsettledByte, [&] { SJisToUcs2(sjis, bytes, 1, ucs2); }));
        } else if (byte < 0x80) {
            Require(SJisToUcs2(sjis, bytes, 1, ucs2) == 0 && ucs2 == byte && (byte == 0 || SJisEncodes(sjis, ucs2, bytes)));
        } else if (byte >= 0xA1 && byte <= 0xDF) {
            Require(SJisToUcs2(sjis, bytes, 1, ucs2) == 0 && ucs2 == 0xFF61 + byte - 0xA1 && SJisEncodes(sjis, ucs2, bytes));
        } else {
            Require(DecodeRejects(byte == 0x80 || byte == 0xA0 || byte >= 0xFD ? invalidEncode : srcBufferEnd, byte == 0x80 || byte == 0xA0 || byte >= 0xFD ? 1u : 2u, sjis, bytes, 1, true));
        }
    }
    for (const std::uint16_t unsettled : {0x5C, 0x7E, 0xA5, 0x203E}) Require(Reports(unsettledByte, [&] { SJisEncodes(sjis, unsettled, ""); }));
    Require(Reports(unsettledCell, [&] { SJisEncodes(sjis, 0xFF3C, ""); }));
    Require(sceCesMbcsUcsContextInit(context, sjis) == 0);
    const std::uint16_t text[] = {0x3042, 0x301C, 0xFF71, 0};
    std::uint8_t mbcs[8] = {};
    Require(sceCesUcs2StrToMbcsStr(context, text, 4, &used, mbcs, sizeof(mbcs), &produced) == 0 && used == 3 && produced == 5 && std::memcmp(mbcs, "\x82\xA0\x81\x60\xB1", 5) == 0);
    Require(Reports(unsettledByte, [&] { sceCesMbcsStrToUcs2Str(context, Bytes("A\\"), 2, &used, wide, 8, &produced); }));
}

int main() {
    constexpr int invalidSrcBuffer = static_cast<int>(0x805C0010);
    constexpr int invalidDstBuffer = static_cast<int>(0x805C0030);

    alignas(8) std::uint8_t sjisSheet[64] = {};
    alignas(8) std::uint8_t eucSheet[64] = {};
    const void* sjis = sceCesUcsProfileInitSJis1997Cp932(sjisSheet);
    const void* euc = sceCesUcsProfileInitEucJpCp51932(eucSheet);
    Require(sjis != nullptr && euc != nullptr);

    std::uint32_t consumed = 0;
    std::uint32_t produced = 0;
    std::uint16_t ucs2 = 0;
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\xA0"), 2, &consumed, &ucs2) == 0 && consumed == 2 && ucs2 == 0x3042);
    Require(sceCesSJisToUcs2(sjis, Bytes("\xB1"), 1, &consumed, &ucs2) == 0 && consumed == 1 && ucs2 == 0xFF71);
    Require(sceCesSJisToUcs2(sjis, Bytes("A"), 1, &consumed, &ucs2) == 0 && consumed == 1 && ucs2 == 'A');
    Require(sceCesSJisToUcs2(sjis, Bytes("\x81\x60"), 2, &consumed, &ucs2) == 0 && ucs2 == 0xFF5E);
    Require(sceCesSJisToUcs2(sjis, Bytes("\xFA\x40"), 2, &consumed, &ucs2) == 0 && ucs2 == 0x2170);
    Require(sceCesSJisToUcs2(euc, Bytes("\x82\xA0"), 2, &consumed, &ucs2) == invalidParameter);
    Require(sceCesSJisToUcs2(sjis, nullptr, 2, &consumed, &ucs2) == invalidSrcBuffer);
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\xA0"), 0, &consumed, &ucs2) == srcBufferEnd);
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\xA0"), 1, &consumed, &ucs2) == srcBufferEnd);
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\xA0"), 2, &consumed, nullptr) == invalidDstBuffer);
    Require(sceCesSJisToUcs2(sjis, Bytes("\x82\x7F"), 2, &consumed, &ucs2) == invalidEncode);
    Require(sceCesSJisToUcs2(sjis, Bytes("\xF0\x40"), 2, &consumed, &ucs2) == unassignedCode);

    std::uint8_t utf8[4] = {};
    Require(sceCesSJisToUtf8(sjis, Bytes("\x93\xFA"), 2, &consumed, utf8, sizeof(utf8), &produced) == 0);
    Require(consumed == 2 && produced == 3 && std::memcmp(utf8, "\xE6\x97\xA5", 3) == 0);
    Require(sceCesSJisToUtf8(sjis, Bytes("\x93\xFA"), 2, &consumed, utf8, 2, &produced) == dstBufferEnd);

    std::uint16_t utf16 = 0;
    Require(sceCesSJisToUtf16(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 1, &produced) == 0 && utf16 == 0x3042 && produced == 1);
    Require(sceCesSJisToUtf16(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 0, &produced) == dstBufferEnd);
    Require(sceCesSJisToUtf16be(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 1, &produced) == 0);
    Require(std::memcmp(&utf16, "\x30\x42", 2) == 0);
    Require(sceCesSJisToUtf16le(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf16, 1, &produced) == 0);
    Require(std::memcmp(&utf16, "\x42\x30", 2) == 0);

    std::uint32_t utf32 = 0;
    Require(sceCesSJisToUtf32(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf32) == 0 && utf32 == 0x3042);
    Require(sceCesSJisToUtf32be(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf32) == 0);
    Require(std::memcmp(&utf32, "\x00\x00\x30\x42", 4) == 0);
    Require(sceCesSJisToUtf32le(sjis, Bytes("\x82\xA0"), 2, &consumed, &utf32) == 0);
    Require(std::memcmp(&utf32, "\x42\x30\x00\x00", 4) == 0);

    Require(sceCesEucJpToUtf16(euc, Bytes("\xA4\xA2"), 2, &consumed, &utf16, 1, &produced) == 0 && consumed == 2 && utf16 == 0x3042);
    Require(sceCesEucJpToUtf16be(euc, Bytes("\x8E\xB1"), 2, &consumed, &utf16, 1, &produced) == 0);
    Require(std::memcmp(&utf16, "\xFF\x71", 2) == 0);
    Require(sceCesEucJpToUtf32(euc, Bytes("\x8F\xB0\xA1"), 3, &consumed, &utf32) == invalidEncode);
    Require(sceCesEucJpToUtf32le(euc, Bytes("\xA4\xA2"), 2, &consumed, &utf32) == 0);
    Require(std::memcmp(&utf32, "\x42\x30\x00\x00", 4) == 0);
    Require(sceCesEucJpToUtf32(sjis, Bytes("\xA4\xA2"), 2, &consumed, &utf32) == invalidParameter);

    std::uint8_t mbcs[4] = {};
    Require(sceCesUcs2ToSJis(0x3042, sjis, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && std::memcmp(mbcs, "\x82\xA0", 2) == 0);
    Require(sceCesUcs2ToSJis(0xFF71, sjis, mbcs, sizeof(mbcs), &produced) == 0 && produced == 1 && mbcs[0] == 0xB1);
    Require(sceCesUcs2ToSJis(0x3042, sjis, mbcs, 1, &produced) == dstBufferEnd);
    Require(sceCesUcs2ToSJis(0xD800, sjis, mbcs, sizeof(mbcs), &produced) == outOfCodeSpace);
    Require(sceCesUcs2ToSJis(0x4E02, sjis, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUcs2ToSJis(0x3042, euc, mbcs, sizeof(mbcs), &produced) == invalidParameter);
    Require(sceCesUcs2ToSJis(0x3042, sceCesRefersUcsProfileCp1252(), mbcs, sizeof(mbcs), &produced) == invalidParameter);
    Require(sceCesUcs2ToSJis(0x3042, sjis, nullptr, sizeof(mbcs), &produced) == invalidDstBuffer);
    Require(sceCesUcs2ToEucJp(0x4E02, euc, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUcs2ToEucJp(0xFF71, euc, mbcs, sizeof(mbcs), &produced) == 0 && produced == 2 && std::memcmp(mbcs, "\x8E\xB1", 2) == 0);

    Require(sceCesUtf8ToSJis(Bytes("\xE6\x97\xA5"), 3, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == 0);
    Require(consumed == 3 && produced == 2 && std::memcmp(mbcs, "\x93\xFA", 2) == 0);
    Require(sceCesUtf8ToSJis(Bytes("\xE6\x97\xA5"), 2, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == srcBufferEnd);
    Require(sceCesUtf8ToSJis(Bytes("\xF0\x9F\x98\x80"), 4, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUtf8ToSJis(nullptr, 3, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == invalidSrcBuffer);
    Require(sceCesUtf8ToEucJp(Bytes("\xE6\x97\xA5"), 3, &consumed, euc, mbcs, sizeof(mbcs), &produced) == 0);
    Require(produced == 2 && std::memcmp(mbcs, "\xC6\xFC", 2) == 0);

    const std::uint16_t hiragana[] = {0x3042};
    const std::uint16_t pair[] = {0xD83D, 0xDE00};
    const std::uint16_t lone[] = {0xDC00};
    Require(sceCesUtf16ToSJis(hiragana, 1, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == 0 && consumed == 1 && std::memcmp(mbcs, "\x82\xA0", 2) == 0);
    Require(sceCesUtf16ToSJis(pair, 2, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == unassignedCode);
    Require(sceCesUtf16ToSJis(pair, 1, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == srcBufferEnd);
    Require(sceCesUtf16ToSJis(lone, 1, &consumed, sjis, mbcs, sizeof(mbcs), &produced) == invalidEncode);
    std::uint16_t hiraganaBe = 0;
    std::memcpy(&hiraganaBe, "\x30\x42", 2);
    Require(sceCesUtf16beToEucJp(&hiraganaBe, 1, &consumed, euc, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\xA4\xA2", 2) == 0);

    Require(sceCesUtf32ToSJis(0x65E5, sjis, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\x93\xFA", 2) == 0);
    Require(sceCesUtf32ToSJis(0x110000, sjis, mbcs, sizeof(mbcs), &produced) == invalidEncode);
    std::uint32_t utf32Be = 0;
    std::memcpy(&utf32Be, "\x00\x00\x65\xE5", 4);
    Require(sceCesUtf32beToSJis(&utf32Be, sjis, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\x93\xFA", 2) == 0);
    Require(sceCesUtf32beToSJis(nullptr, sjis, mbcs, sizeof(mbcs), &produced) == invalidSrcBuffer);
    std::uint32_t utf32Le = 0;
    std::memcpy(&utf32Le, "\xE5\x65\x00\x00", 4);
    Require(sceCesUtf32leToEucJp(&utf32Le, euc, mbcs, sizeof(mbcs), &produced) == 0 && std::memcmp(mbcs, "\xC6\xFC", 2) == 0);

    constexpr int microsoftForms[] = {0x00A2, 0x00A3, 0x00A5, 0x00AC, 0x2014, 0x2016, 0x203E, 0x2212, 0x301C, 0xFEFF};
    for (int code = 1; code < 0x10000; ++code) {
        if (code >= 0xD800 && code <= 0xDFFF) continue;
        if (std::find(std::begin(microsoftForms), std::end(microsoftForms), code) != std::end(microsoftForms)) continue;
        std::uint8_t encoded[4] = {};
        std::uint32_t size = 0;
        if (sceCesUcs2ToSJis(static_cast<std::uint16_t>(code), sjis, encoded, sizeof(encoded), &size) == 0) {
            std::uint16_t back = 0;
            Require(sceCesSJisToUcs2(sjis, encoded, size, &consumed, &back) == 0 && consumed == size && back == code);
        }
    }

    std::uint8_t ku = 0;
    std::uint8_t ten = 0;
    std::uint16_t sjisCode = 0;
    Require(sceCesSJisCodeToJisX0208(0x82A0, &ku, &ten) == 0 && ku == 4 && ten == 2);
    Require(sceCesSJisCodeToJisX0208(0x93FA, &ku, &ten) == 0 && ku == 38 && ten == 92);
    Require(sceCesSJisCodeToJisX0208(0xEFFC, &ku, &ten) == 0 && ku == 94 && ten == 94);
    Require(sceCesSJisCodeToJisX0208(0xF040, &ku, &ten) == invalidEncode);
    Require(sceCesSJisCodeToJisX0208(0x0041, &ku, &ten) == invalidEncode);
    Require(sceCesSJisCodeToJisX0208(0x827F, &ku, &ten) == invalidEncode);
    Require(sceCesSJisCodeToJisX0208(0x82A0, nullptr, &ten) == invalidDstBuffer);
    Require(sceCesJisX0208ToSJisCode(4, 2, &sjisCode) == 0 && sjisCode == 0x82A0);
    Require(sceCesJisX0208ToSJisCode(0, 2, &sjisCode) == invalidEncode);
    Require(sceCesJisX0208ToSJisCode(4, 95, &sjisCode) == invalidEncode);
    Require(sceCesJisX0208ToSJisCode(4, 2, nullptr) == invalidDstBuffer);
    for (int row = 1; row <= 94; ++row) {
        for (int cell = 1; cell <= 94; ++cell) {
            Require(sceCesJisX0208ToSJisCode(static_cast<std::uint8_t>(row), static_cast<std::uint8_t>(cell), &sjisCode) == 0);
            Require(sceCesSJisCodeToJisX0208(sjisCode, &ku, &ten) == 0 && ku == row && ten == cell);
        }
    }
    TestJisX0208Profiles(sjis, euc);
    return 0;
}
