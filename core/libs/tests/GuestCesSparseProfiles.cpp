#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

extern "C" {
int APS5_VABI sceCesMbcToUtf8(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcToUtf32(const void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesSbcToUcs2(const std::uint8_t*, std::uint8_t, std::uint16_t*);
int APS5_VABI sceCesSbcToUtf16(const std::uint8_t*, std::uint8_t, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf16be(const std::uint8_t*, std::uint8_t, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf16le(const std::uint8_t*, std::uint8_t, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf32(const std::uint8_t*, std::uint8_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf32be(const std::uint8_t*, std::uint8_t, std::uint32_t*);
int APS5_VABI sceCesSbcToUtf32le(const std::uint8_t*, std::uint8_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUtf16Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUtf16Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUtf16StrToMbcsStr(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf16StrGetMbcsLen(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUtf32Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUtf32Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUtf32StrToMbcsStr(void*, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf32StrGetMbcsLen(void*, const std::uint32_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_3();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_6();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_7();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_8();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_11();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp857();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp869();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp874();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252();
int APS5_VABI sceCesSbcToUtf8(const std::uint8_t*, std::uint8_t, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToSbc(const std::uint8_t*, std::uint32_t, std::uint32_t*, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
int APS5_VABI sceCesMbcsStrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUtf8Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUtf8StrToMbcsStr(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8StrGetMbcsLen(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUcs2Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUcs2Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUcs2StrToMbcsStr(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUcs2StrGetMbcsLen(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
}

namespace {

constexpr int Unassigned = static_cast<int>(0x805C0020);
constexpr int ShortOutput = static_cast<int>(0x805C0031);
constexpr std::uint32_t Untouched = 0x12345678;
const char* currentProfile = "";
unsigned currentByte = 0;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s byte 0x%02X line %u\n", currentProfile, currentByte, location.line());
    std::abort();
}

struct Profile {
    const char* name;
    decltype(&sceCesRefersUcsProfileCp1252) get;
    std::array<std::uint64_t, 2> undefined;
    std::uint64_t checksum;
};

constexpr Profile Profiles[] = {
    {"Iso8859_3", sceCesRefersUcsProfileIso8859_3, {0x4000402000000000ULL, 0x0001000800010008ULL}, 0x3A14FC6A7610759CULL},
    {"Iso8859_6", sceCesRefersUcsProfileIso8859_6, {0x77FFCFEE00000000ULL, 0xFFF80000F8000001ULL}, 0x9C2E4F6D1F8BAAC9ULL},
    {"Iso8859_7", sceCesRefersUcsProfileIso8859_7, {0x0000400000000000ULL, 0x8000000000040000ULL}, 0xA1EC3F5965B3F537ULL},
    {"Iso8859_8", sceCesRefersUcsProfileIso8859_8, {0x8000000200000000ULL, 0x980000007FFFFFFFULL}, 0x7ED25AFCF1761A52ULL},
    {"Iso8859_11", sceCesRefersUcsProfileIso8859_11, {0x0000000000000000ULL, 0xF000000078000000ULL}, 0x6902E282C2CE9744ULL},
    {"Cp857", sceCesRefersUcsProfileCp857, {0x0000000000000000ULL, 0x0004008000200000ULL}, 0x7025402330A56CADULL},
    {"Cp869", sceCesRefersUcsProfileCp869, {0x00000000001800BFULL, 0x0000000000000000ULL}, 0x7B0192F88690DE80ULL},
    {"Cp874", sceCesRefersUcsProfileCp874, {0x00000000FF01FFDEULL, 0xF000000078000000ULL}, 0xD383EE6DBF9B57B2ULL},
};

void Hash(std::uint64_t& value, std::uint8_t byte) {
    value = (value ^ byte) * 1099511628211ULL;
}

void CheckMbcScalar(const std::uint8_t* profile, std::uint8_t byte, const std::uint8_t* expected,
                    std::uint32_t length, std::uint32_t code, bool undefined) {
    const int status = undefined ? Unassigned : 0;
    std::array<std::uint8_t, 4> output = {0xA5, 0xA5, 0xA5, 0xA5};
    std::uint32_t consumed = Untouched;
    std::uint32_t produced = Untouched;
    Require(sceCesMbcToUtf8(profile, &byte, 1, &consumed, output.data(), output.size(), &produced) == status);
    Require(consumed == (undefined ? Untouched : 1) && produced == (undefined ? Untouched : length));
    if (!undefined) Require(std::memcmp(output.data(), expected, length) == 0);
    for (std::size_t i = undefined ? 0 : length; i < output.size(); ++i) Require(output[i] == 0xA5);
    Require(sceCesMbcToUtf8(profile, &byte, 1, nullptr, output.data(), output.size(), nullptr) == status);
    output.fill(0xA5);
    consumed = produced = Untouched;
    Require(sceCesMbcToUtf8(profile, &byte, 1, &consumed, output.data(), 0, &produced) == (undefined ? Unassigned : ShortOutput));
    Require(consumed == Untouched && produced == Untouched);
    for (auto value : output) Require(value == 0xA5);
    std::uint32_t utf32[2] = {0xA5A5A5A5, 0xA5A5A5A5};
    Require(sceCesMbcToUtf32(profile, &byte, 1, &consumed, utf32) == status);
    Require(consumed == (undefined ? Untouched : 1));
    Require(utf32[0] == (undefined ? 0xA5A5A5A5 : code) && utf32[1] == 0xA5A5A5A5);
    Require(sceCesMbcToUtf32(profile, &byte, 1, nullptr, utf32) == status);
}

void CheckWideScalar(const std::uint8_t* profile, std::uint8_t byte, std::uint32_t code, bool undefined) {
    const int status = undefined ? Unassigned : 0;
    std::uint16_t ucs2[2] = {0xA5A5, 0xA5A5};
    Require(sceCesSbcToUcs2(profile, byte, ucs2) == status);
    Require(ucs2[0] == (undefined ? 0xA5A5 : code) && ucs2[1] == 0xA5A5);
    const decltype(&sceCesSbcToUtf16) to16[] = {sceCesSbcToUtf16, sceCesSbcToUtf16be, sceCesSbcToUtf16le};
    const decltype(&sceCesSbcToUtf32) to32[] = {sceCesSbcToUtf32, sceCesSbcToUtf32be, sceCesSbcToUtf32le};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::uint16_t expected16 = i == 1 ? static_cast<std::uint16_t>(code << 8 | code >> 8) : code;
        const std::uint32_t expected32 = i == 1 ? (code & 0xff) << 24 | (code & 0xff00) << 8 : code;
        ucs2[0] = 0xA5A5;
        std::uint32_t length = Untouched;
        Require(to16[i](profile, byte, ucs2, 1, &length) == status);
        Require(ucs2[0] == (undefined ? 0xA5A5 : expected16) && ucs2[1] == 0xA5A5);
        Require(length == (undefined ? Untouched : 1));
        Require(to16[i](profile, byte, ucs2, 1, nullptr) == status);
        ucs2[0] = 0xA5A5;
        length = Untouched;
        Require(to16[i](profile, byte, ucs2, 0, &length) == (undefined ? Unassigned : ShortOutput));
        Require(ucs2[0] == 0xA5A5 && ucs2[1] == 0xA5A5 && length == Untouched);
        std::uint32_t utf32[2] = {0xA5A5A5A5, 0xA5A5A5A5};
        Require(to32[i](profile, byte, utf32) == status);
        Require(utf32[0] == (undefined ? 0xA5A5A5A5 : expected32) && utf32[1] == 0xA5A5A5A5);
    }
}

template<typename TUnit, typename TConvert, typename TLength, typename TReverse, typename TReverseLength>
void CheckWideString(void* context, std::uint8_t byte, TUnit code, bool undefined,
                     TConvert convert, TLength getLength, TReverse reverse, TReverseLength getReverseLength) {
    const TUnit untouched = static_cast<TUnit>(0xA5A5A5A5);
    TUnit output[3] = {untouched, untouched, untouched};
    const int status = undefined ? Unassigned : 0;
    const std::uint32_t units = undefined || byte == 0 ? 0 : 1;
    std::uint32_t consumed = Untouched;
    std::uint32_t produced = Untouched;
    Require(convert(context, &byte, 1, &consumed, output, 2, &produced) == status);
    Require(consumed == units && produced == units);
    Require(output[0] == (units == 0 ? 0 : code) && output[units] == 0 && output[2] == untouched);
    if (units == 0) Require(output[1] == untouched);
    consumed = produced = Untouched;
    Require(getLength(context, &byte, 1, &consumed, &produced) == status);
    Require(consumed == units && produced == units);
    if (undefined) {
        const std::uint8_t input[] = {'A', byte, 'B'};
        Require(convert(context, input, 3, &consumed, output, 3, &produced) == Unassigned);
        Require(output[0] == 'A' && output[1] == 0 && output[2] == untouched);
        Require(consumed == 1 && produced == 1);
        Require(getLength(context, input, 3, &consumed, &produced) == Unassigned);
        Require(consumed == 1 && produced == 1);
        return;
    }
    std::uint8_t back[3] = {0xA5, 0xA5, 0xA5};
    Require(reverse(context, &code, 1, &consumed, back, 2, &produced) == 0);
    Require(consumed == units && produced == units && back[0] == byte && back[units] == 0 && back[2] == 0xA5);
    Require(getReverseLength(context, &code, 1, &consumed, &produced) == 0);
    Require(consumed == units && produced == units);
    code = 0xFFFF;
    consumed = produced = Untouched;
    back[0] = back[1] = 0xA5;
    Require(reverse(context, &code, 1, &consumed, back, 2, &produced) == Unassigned);
    Require(consumed == 0 && produced == 0 && back[0] == 0 && back[1] == 0xA5 && back[2] == 0xA5);
    Require(getReverseLength(context, &code, 1, &consumed, &produced) == Unassigned);
    Require(consumed == 0 && produced == 0);
}

void CheckContext(void* context, std::uint8_t byte, const std::uint8_t* expected, std::uint32_t length, bool undefined) {
    std::array<std::uint8_t, 4> output = {0xA5, 0xA5, 0xA5, 0xA5};
    std::uint32_t consumed = Untouched;
    std::uint32_t produced = Untouched;
    const int status = undefined ? Unassigned : 0;
    const std::uint32_t units = undefined || byte == 0 ? 0 : 1;
    const std::uint32_t bytes = undefined || byte == 0 ? 0 : length;
    Require(sceCesMbcsStrToUtf8Str(context, &byte, 1, &consumed, output.data(), output.size(), &produced) == status);
    Require(consumed == units && produced == bytes && output[bytes] == 0);
    if (!undefined) Require(std::memcmp(output.data(), expected, bytes) == 0);
    for (std::size_t i = bytes + 1; i < output.size(); ++i) Require(output[i] == 0xA5);
    consumed = produced = Untouched;
    Require(sceCesMbcsStrGetUtf8Len(context, &byte, 1, &consumed, &produced) == status);
    Require(consumed == units && produced == bytes);

    std::uint16_t ucs2[3] = {0xA5A5, 0xA5A5, 0xA5A5};
    consumed = produced = Untouched;
    Require(sceCesMbcsStrToUcs2Str(context, &byte, 1, &consumed, ucs2, 2, &produced) == status);
    Require(consumed == units && produced == units && ucs2[units] == 0 && ucs2[2] == 0xA5A5);
    if (units == 0) Require(ucs2[1] == 0xA5A5);
    consumed = produced = Untouched;
    Require(sceCesMbcsStrGetUcs2Len(context, &byte, 1, &consumed, &produced) == status);
    Require(consumed == units && produced == units);
    if (undefined) return;

    std::uint16_t code = expected[0];
    if (length == 2) code = (expected[0] & 0x1F) << 6 | (expected[1] & 0x3F);
    if (length == 3) code = (expected[0] & 0x0F) << 12 | (expected[1] & 0x3F) << 6 | (expected[2] & 0x3F);
    if (byte != 0) Require(ucs2[0] == code);
    std::uint8_t back[3] = {0xA5, 0xA5, 0xA5};
    Require(sceCesUtf8StrToMbcsStr(context, expected, length, &consumed, back, 2, &produced) == 0);
    Require(consumed == bytes && produced == units && back[0] == byte && back[units] == 0 && back[2] == 0xA5);
    Require(sceCesUtf8StrGetMbcsLen(context, expected, length, &consumed, &produced) == 0);
    Require(consumed == bytes && produced == units);
    back[0] = 0xA5;
    Require(sceCesUcs2StrToMbcsStr(context, &code, 1, &consumed, back, 2, &produced) == 0);
    Require(consumed == units && produced == units && back[0] == byte && back[units] == 0 && back[2] == 0xA5);
    Require(sceCesUcs2StrGetMbcsLen(context, &code, 1, &consumed, &produced) == 0);
    Require(consumed == units && produced == units);
    if (byte == 0) return;
    output.fill(0xA5);
    consumed = produced = Untouched;
    Require(sceCesMbcsStrToUtf8Str(context, &byte, 1, &consumed, output.data(), length, &produced) == ShortOutput);
    Require(consumed == 0 && produced == 0 && output[0] == 0);
    for (std::size_t i = 1; i < output.size(); ++i) Require(output[i] == 0xA5);
    output.fill(0xA5);
    Require(sceCesUtf8StrToMbcsStr(context, expected, length, &consumed, output.data(), 0, &produced) == ShortOutput);
    Require(consumed == 0 && produced == 0);
    for (auto value : output) Require(value == 0xA5);
}

void CheckUnmapped(const std::uint8_t* profile, void* context) {
    const std::uint8_t utf8[] = {0xEF, 0xBF, 0xBF};
    const std::uint16_t ucs2 = 0xFFFF;
    std::uint8_t output = 0xA5;
    std::uint32_t consumed = Untouched;
    std::uint32_t produced = Untouched;
    Require(sceCesUtf8ToSbc(utf8, sizeof(utf8), &consumed, profile, &output) == Unassigned);
    Require(consumed == sizeof(utf8) && output == 0);
    Require(sceCesUtf8StrToMbcsStr(context, utf8, sizeof(utf8), &consumed, &output, 1, &produced) == Unassigned);
    Require(consumed == 0 && produced == 0 && output == 0);
    output = 0xA5;
    Require(sceCesUcs2StrToMbcsStr(context, &ucs2, 1, &consumed, &output, 1, &produced) == Unassigned);
    Require(consumed == 0 && produced == 0 && output == 0);
}

void CheckPrefix(void* context, std::uint8_t hole) {
    const std::uint8_t input[] = {'A', hole, 'B'};
    std::uint8_t output[4] = {0xA5, 0xA5, 0xA5, 0xA5};
    std::uint32_t consumed = Untouched;
    std::uint32_t produced = Untouched;
    Require(sceCesMbcsStrToUtf8Str(context, input, 3, &consumed, output, 4, &produced) == Unassigned);
    Require(consumed == 1 && produced == 1);
    Require(output[0] == 'A' && output[1] == 0 && output[2] == 0xA5 && output[3] == 0xA5);
    Require(sceCesMbcsStrGetUtf8Len(context, input, 3, &consumed, &produced) == Unassigned);
    Require(consumed == 1 && produced == 1);
    const std::uint8_t terminated[] = {'A', 0, hole};
    Require(sceCesMbcsStrToUtf8Str(context, terminated, 3, &consumed, output, 4, &produced) == 0);
    Require(consumed == 1 && produced == 1 && output[1] == 0);
}

void CheckProfile(const Profile& fixture) {
    currentProfile = fixture.name;
    const auto* profile = fixture.get();
    Require(profile && fixture.get() == profile && profile != sceCesRefersUcsProfileCp1252());
    alignas(8) std::uint8_t context[256] = {};
    Require(sceCesMbcsUcsContextInit(context, profile) == 0);
    std::uint64_t checksum = 14695981039346656037ULL;
    for (currentByte = 0; currentByte < 256; ++currentByte) {
        const auto byte = static_cast<std::uint8_t>(currentByte);
        const bool undefined = byte >= 128 && (fixture.undefined[(byte - 128) / 64] >> ((byte - 128) % 64) & 1) != 0;
        std::array<std::uint8_t, 4> utf8 = {0xA5, 0xA5, 0xA5, 0xA5};
        std::uint32_t length = Untouched;
        Require(sceCesSbcToUtf8(profile, byte, utf8.data(), utf8.size(), &length) == (undefined ? Unassigned : 0));
        std::uint32_t code = utf8[0];
        if (length == 2) code = (utf8[0] & 0x1F) << 6 | (utf8[1] & 0x3F);
        if (length == 3) code = (utf8[0] & 0x0F) << 12 | (utf8[1] & 0x3F) << 6 | (utf8[2] & 0x3F);
        CheckWideScalar(profile, byte, code, undefined);
        CheckMbcScalar(profile, byte, utf8.data(), length, code, undefined);
        CheckWideString(context, byte, static_cast<std::uint16_t>(code), undefined,
            sceCesMbcsStrToUtf16Str, sceCesMbcsStrGetUtf16Len, sceCesUtf16StrToMbcsStr, sceCesUtf16StrGetMbcsLen);
        CheckWideString(context, byte, code, undefined,
            sceCesMbcsStrToUtf32Str, sceCesMbcsStrGetUtf32Len, sceCesUtf32StrToMbcsStr, sceCesUtf32StrGetMbcsLen);
        if (undefined) {
            Require(length == 0);
            for (auto value : utf8) Require(value == 0xA5);
            CheckContext(context, byte, utf8.data(), 0, true);
            CheckPrefix(context, byte);
            continue;
        }
        Require(length > 0 && length <= 3);
        Hash(checksum, static_cast<std::uint8_t>(length));
        for (std::uint32_t i = 0; i < length; ++i) Hash(checksum, utf8[i]);
        for (std::uint32_t i = length; i < utf8.size(); ++i) Require(utf8[i] == 0xA5);
        std::uint8_t back = 0xA5;
        std::uint32_t consumed = Untouched;
        Require(sceCesUtf8ToSbc(utf8.data(), length, &consumed, profile, &back) == 0 && consumed == length && back == byte);
        Require(sceCesUtf8ToSbc(utf8.data(), length, nullptr, profile, &back) == 0 && back == byte);
        CheckContext(context, byte, utf8.data(), length, false);
        std::array<std::uint8_t, 4> shortOutput = {0xA5, 0xA5, 0xA5, 0xA5};
        consumed = Untouched;
        Require(sceCesSbcToUtf8(profile, byte, shortOutput.data(), length - 1, &consumed) == ShortOutput);
        Require(consumed == length);
        for (auto value : shortOutput) Require(value == 0xA5);
        Require(sceCesSbcToUtf8(profile, byte, shortOutput.data(), length, nullptr) == 0);
        Require(std::memcmp(shortOutput.data(), utf8.data(), length) == 0);
    }
    Require(checksum == fixture.checksum);
    CheckUnmapped(profile, context);
}

} // namespace

int main() {
    for (const auto& profile : Profiles) CheckProfile(profile);
    for (std::size_t i = 0; i < std::size(Profiles); ++i) {
        for (std::size_t j = 0; j < i; ++j) Require(Profiles[i].get() != Profiles[j].get());
    }
}
