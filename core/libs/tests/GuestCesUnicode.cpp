#include "prx/libSceCesCs/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceCesCs/JisX0208.hpp"
#include "prx/libSceCesCs/JisX0212.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int INVALID_PARAMETER = static_cast<int>(0x805C0001);
constexpr int INVALID_SRC_BUFFER = static_cast<int>(0x805C0010);
constexpr int SRC_BUFFER_END = static_cast<int>(0x805C0011);
constexpr int INVALID_ENCODE = static_cast<int>(0x805C0014);
constexpr int UNASSIGNED_CODE = static_cast<int>(0x805C0020);
constexpr int INVALID_DST_BUFFER = static_cast<int>(0x805C0030);
constexpr int DST_BUFFER_END = static_cast<int>(0x805C0031);
constexpr uint32_t SENTINEL = 0xA5A5A5A5;

void Require(bool value) {
    if (!value) std::abort();
}

void Hash(uint64_t& hash, const void* data, uint32_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 1099511628211ull;
}

void TestByteOrder() {
    const uint8_t source[] = {0xF0, 0x9F, 0x98, 0x80};
    const uint8_t expected16be[] = {0xD8, 0x3D, 0xDE, 0x00};
    const uint8_t expected16le[] = {0x3D, 0xD8, 0x00, 0xDE};
    const uint8_t expected32be[] = {0x00, 0x01, 0xF6, 0x00};
    const uint8_t expected32le[] = {0x00, 0xF6, 0x01, 0x00};
    uint16_t units[2] = {};
    uint32_t scalar = 0;
    Require(sceCesUtf8ToUtf16be(source, 4, nullptr, units, 2, nullptr) == 0 && std::memcmp(units, expected16be, 4) == 0);
    Require(sceCesUtf8ToUtf16le(source, 4, nullptr, units, 2, nullptr) == 0 && std::memcmp(units, expected16le, 4) == 0);
    Require(sceCesUtf8ToUtf32be(source, 4, nullptr, &scalar) == 0 && std::memcmp(&scalar, expected32be, 4) == 0);
    Require(sceCesUtf8ToUtf32le(source, 4, nullptr, &scalar) == 0 && std::memcmp(&scalar, expected32le, 4) == 0);
    const uint8_t bom[] = {0xEF, 0xBB, 0xBF};
    Require(sceCesUtf8ToUtf32(bom, 3, nullptr, &scalar) == 0 && scalar == 0xFEFF);
    Require(sceCesUtf8ToUtf16(bom, 3, nullptr, units, 1, nullptr) == 0 && units[0] == 0xFEFF);
}

void TestScalars() {
    uint64_t utf8Hash = 14695981039346656037ull;
    uint64_t utf16Hash = utf8Hash;
    for (uint32_t code = 0; code <= 0x10FFFF; ++code) {
        if (code >= 0xD800 && code <= 0xDFFF) continue;
        std::array<uint8_t, 8> bytes;
        bytes.fill(0xA5);
        uint32_t produced = SENTINEL;
        Require(sceCesUtf32ToUtf8(code, bytes.data(), 4, &produced) == 0);
        Require(produced >= 1 && produced <= 4 && bytes[produced] == 0xA5);
        Hash(utf8Hash, bytes.data(), produced);
        uint32_t decoded = SENTINEL;
        uint32_t consumed = SENTINEL;
        Require(sceCesUtf8ToUtf32(bytes.data(), produced, &consumed, &decoded) == 0);
        Require(decoded == code && consumed == produced);

        uint16_t ucs2 = 0xA5A5;
        consumed = SENTINEL;
        const int ucsResult = sceCesUtf8ToUcs2(bytes.data(), produced, &consumed, &ucs2);
        std::array<uint8_t, 8> back;
        back.fill(0xA5);
        uint32_t size = SENTINEL;
        if (code <= 0xFFFF) {
            Require(ucsResult == 0 && ucs2 == code && consumed == produced);
            Require(sceCesUcs2ToUtf8(ucs2, back.data(), 4, &size) == 0);
            Require(size == produced && std::memcmp(back.data(), bytes.data(), size) == 0);
        } else Require(ucsResult == UNASSIGNED_CODE && ucs2 == 0xA5A5 && consumed == SENTINEL);

        using To16 = int (APS5_VABI *)(const uint8_t*, uint32_t, uint32_t*, uint16_t*, uint32_t, uint32_t*);
        using From16 = int (APS5_VABI *)(const uint16_t*, uint32_t, uint32_t*, uint8_t*, uint32_t, uint32_t*);
        const To16 to16[] = {sceCesUtf8ToUtf16, sceCesUtf8ToUtf16be, sceCesUtf8ToUtf16le};
        const From16 from16[] = {sceCesUtf16ToUtf8, sceCesUtf16beToUtf8, sceCesUtf16leToUtf8};
        for (uint32_t variant = 0; variant < 3; ++variant) {
            uint16_t units[3] = {0xA5A5, 0xA5A5, 0xA5A5};
            uint32_t unitCount = SENTINEL;
            consumed = SENTINEL;
            Require(to16[variant](bytes.data(), produced, &consumed, units, 2, &unitCount) == 0);
            Require(consumed == produced && unitCount == (code <= 0xFFFF ? 1u : 2u) && units[unitCount] == 0xA5A5);
            if (variant == 2) Hash(utf16Hash, units, 2 * unitCount);
            consumed = size = SENTINEL;
            Require(from16[variant](units, unitCount, &consumed, back.data(), 4, &size) == 0);
            Require(consumed == unitCount && size == produced && std::memcmp(back.data(), bytes.data(), size) == 0);
        }

        using To32 = int (APS5_VABI *)(const uint8_t*, uint32_t, uint32_t*, uint32_t*);
        using From32 = int (APS5_VABI *)(const uint32_t*, uint8_t*, uint32_t, uint32_t*);
        const To32 to32[] = {sceCesUtf8ToUtf32be, sceCesUtf8ToUtf32le};
        const From32 from32[] = {sceCesUtf32beToUtf8, sceCesUtf32leToUtf8};
        for (uint32_t variant = 0; variant < 2; ++variant) {
            uint32_t stored[2] = {SENTINEL, SENTINEL};
            Require(to32[variant](bytes.data(), produced, &consumed, stored) == 0);
            Require(consumed == produced && stored[1] == SENTINEL);
            const auto* raw = reinterpret_cast<const uint8_t*>(stored);
            Require(raw[variant == 0 ? 0 : 3] == 0 && raw[variant == 0 ? 3 : 0] == (code & 0xFF));
            Require(from32[variant](stored, back.data(), 4, &size) == 0);
            Require(size == produced && std::memcmp(back.data(), bytes.data(), size) == 0);
        }
    }
    Require(utf8Hash == 0x957CC0987E6013E5ull);
    Require(utf16Hash == 0x6A25A430597EC325ull);
    std::puts("1112064 Unicode scalars match Python UTF-8 and UTF-16LE reference hashes");
}

void TestErrors() {
    const uint8_t emoji[] = {0xF0, 0x9F, 0x98, 0x80, 'X'};
    uint32_t consumed = SENTINEL;
    uint32_t produced = SENTINEL;
    uint32_t scalar = SENTINEL;
    uint16_t units[3] = {0xA5A5, 0xA5A5, 0xA5A5};
    uint8_t bytes[5] = {0xA5, 0xA5, 0xA5, 0xA5, 0xA5};
    for (uint32_t cap = 0; cap < 4; ++cap) {
        Require(sceCesUtf32ToUtf8(0x1F600, bytes, cap, &produced) == DST_BUFFER_END);
        Require(produced == SENTINEL && bytes[0] == 0xA5 && bytes[3] == 0xA5);
        Require(sceCesUtf8ToUtf32(emoji, cap, &consumed, &scalar) == SRC_BUFFER_END);
        Require(consumed == SENTINEL && scalar == SENTINEL);
    }
    for (uint32_t cap = 0; cap < 2; ++cap) {
        Require(sceCesUtf8ToUtf16(emoji, 4, &consumed, units, cap, &produced) == DST_BUFFER_END);
        Require(consumed == SENTINEL && produced == SENTINEL && units[0] == 0xA5A5 && units[1] == 0xA5A5);
    }
    const uint8_t invalid[][4] = {{0x80}, {0xC0, 0x80}, {0xC1, 0xBF}, {0xC2, 0x7F}, {0xE0, 0x9F},
        {0xED, 0xA0}, {0xF0, 0x8F}, {0xF4, 0x90}, {0xF5}, {0xFF}, {0xE2, 0x82, 0x41}, {0xF0, 0x9F, 0x98, 0x41}};
    const uint32_t lengths[] = {1, 2, 2, 2, 2, 2, 2, 2, 1, 1, 3, 4};
    for (uint32_t i = 0; i < std::size(invalid); ++i) {
        Require(sceCesUtf8ToUtf32(invalid[i], lengths[i], &consumed, &scalar) == INVALID_ENCODE);
        Require(sceCesUtf8ToUtf16be(invalid[i], lengths[i], &consumed, units, 2, &produced) == INVALID_ENCODE);
        Require(consumed == SENTINEL && produced == SENTINEL && scalar == SENTINEL && units[0] == 0xA5A5);
    }
    for (uint32_t code : {0xD800u, 0xDBFFu, 0xDC00u, 0xDFFFu, 0x110000u, 0xFFFFFFFFu}) {
        Require(sceCesUtf32ToUtf8(code, bytes, 4, &produced) == INVALID_ENCODE);
        if (code <= 0xFFFF) Require(sceCesUcs2ToUtf8(static_cast<uint16_t>(code), bytes, 4, &produced) == INVALID_ENCODE);
        Require(produced == SENTINEL && bytes[0] == 0xA5);
    }
    const uint16_t high[] = {0xD800, 0x0041};
    const uint16_t low[] = {0xDC00};
    Require(sceCesUtf16ToUtf8(high, 1, &consumed, bytes, 4, &produced) == SRC_BUFFER_END);
    Require(sceCesUtf16ToUtf8(high, 2, &consumed, bytes, 4, &produced) == INVALID_ENCODE);
    Require(sceCesUtf16ToUtf8(low, 1, &consumed, bytes, 4, &produced) == INVALID_ENCODE);
    Require(consumed == SENTINEL && produced == SENTINEL && bytes[0] == 0xA5);
    Require(sceCesUtf8ToUtf32(nullptr, 4, &consumed, &scalar) == INVALID_SRC_BUFFER);
    Require(sceCesUtf8ToUtf32(emoji, 4, &consumed, nullptr) == INVALID_DST_BUFFER);
    Require(sceCesUtf8ToUcs2(emoji, 4, &consumed, nullptr) == INVALID_DST_BUFFER);
    Require(sceCesUtf8ToUtf16(emoji, 4, &consumed, nullptr, 2, &produced) == INVALID_DST_BUFFER);
    Require(sceCesUtf16ToUtf8(nullptr, 1, &consumed, bytes, 4, &produced) == INVALID_SRC_BUFFER);
    Require(sceCesUtf16ToUtf8(high, 0, &consumed, bytes, 4, &produced) == SRC_BUFFER_END);
    Require(sceCesUtf32beToUtf8(nullptr, bytes, 4, &produced) == INVALID_SRC_BUFFER);
    Require(sceCesUtf32ToUtf8(0, nullptr, 4, &produced) == INVALID_DST_BUFFER);
    Require(sceCesUtf8ToUtf32(emoji, 5, nullptr, &scalar) == 0 && scalar == 0x1F600);
    Require(sceCesUtf8ToUtf16(emoji, 5, nullptr, units, 2, nullptr) == 0 && units[0] == 0xD83D && units[1] == 0xDE00);
    Require(sceCesUtf16ToUtf8(units, 2, nullptr, bytes, 4, nullptr) == 0 && std::memcmp(bytes, emoji, 4) == 0);
    Require(sceCesUcs2ToUtf8(0, bytes, 1, nullptr) == 0 && bytes[0] == 0);
    Require(sceCesUtf8ToUcs2(bytes, 1, nullptr, units) == 0 && units[0] == 0);
}

void TestEucJp() {
    std::array<uint8_t, 1024> sheet{};
    const void* profile = sceCesUcsProfileInitEucJpCp51932(sheet.data());
    auto check = [&](const uint8_t* source, uint32_t length, uint32_t code) {
        uint8_t expected[4] = {};
        uint32_t expectedLength = 0;
        Require(sceCesUtf32ToUtf8(code, expected, 4, &expectedLength) == 0);
        std::array<uint8_t, 8> output;
        output.fill(0xA5);
        uint32_t consumed = SENTINEL;
        uint32_t produced = SENTINEL;
        Require(sceCesEucJpToUtf8(profile, source, length, &consumed, output.data(), expectedLength, &produced) == 0);
        Require(consumed == length && produced == expectedLength && output[produced] == 0xA5);
        Require(std::memcmp(output.data(), expected, produced) == 0);
        for (uint32_t cap = 0; cap < expectedLength; ++cap) {
            output.fill(0xA5);
            consumed = produced = SENTINEL;
            Require(sceCesEucJpToUtf8(profile, source, length, &consumed, output.data(), cap, &produced) == DST_BUFFER_END);
            Require(consumed == SENTINEL && produced == SENTINEL && output[0] == 0xA5);
        }
    };
    for (uint32_t code = 0; code < 128; ++code) {
        const uint8_t source[] = {static_cast<uint8_t>(code)};
        check(source, 1, code);
    }
    for (uint32_t cell = 0; cell < 63; ++cell) {
        const uint8_t source[] = {0x8E, static_cast<uint8_t>(0xA1 + cell)};
        check(source, 2, 0xFF61 + cell);
    }
    uint32_t assigned = 0;
    for (uint32_t supplementary = 0; supplementary < 2; ++supplementary) {
        for (uint32_t cell = 0; cell < 94 * 94; ++cell) {
            const uint32_t code = supplementary ? JisX0212::TO_UCS2[cell] : JisX0208::TO_UCS2[cell];
            const uint8_t source[] = {0x8F, static_cast<uint8_t>(0xA1 + cell / 94), static_cast<uint8_t>(0xA1 + cell % 94)};
            const uint8_t* start = source + (supplementary ? 0 : 1);
            const uint32_t length = supplementary ? 3 : 2;
            if (code) { check(start, length, code); ++assigned; }
            else {
                uint8_t output[4] = {0xA5, 0xA5, 0xA5, 0xA5};
                uint32_t consumed = SENTINEL;
                uint32_t produced = SENTINEL;
                Require(sceCesEucJpToUtf8(profile, start, length, &consumed, output, 4, &produced) == UNASSIGNED_CODE);
                Require(consumed == SENTINEL && produced == SENTINEL && output[0] == 0xA5);
            }
        }
    }
    Require(assigned == 13403);
    const uint8_t source[] = {0x8F, 0xA2, 0xAF, 'X'};
    uint8_t output[4] = {0xA5, 0xA5, 0xA5, 0xA5};
    uint32_t consumed = SENTINEL;
    uint32_t produced = SENTINEL;
    Require(sceCesEucJpToUtf8(nullptr, source, 3, &consumed, output, 4, &produced) == INVALID_PARAMETER);
    Require(sceCesEucJpToUtf8(profile, nullptr, 3, &consumed, output, 4, &produced) == INVALID_SRC_BUFFER);
    Require(sceCesEucJpToUtf8(profile, source, 0, &consumed, output, 4, &produced) == SRC_BUFFER_END);
    Require(sceCesEucJpToUtf8(profile, source, 3, &consumed, nullptr, 4, &produced) == INVALID_DST_BUFFER);
    for (uint32_t size : {1u, 2u}) Require(sceCesEucJpToUtf8(profile, source, size, &consumed, output, 4, &produced) == SRC_BUFFER_END);
    const uint8_t malformed[] = {0x8E, 0x41};
    Require(sceCesEucJpToUtf8(profile, malformed, 2, &consumed, output, 4, &produced) == INVALID_ENCODE);
    Require(consumed == SENTINEL && produced == SENTINEL && output[0] == 0xA5);
    Require(sceCesEucJpToUtf8(profile, source, 4, &consumed, output, 4, &produced) == 0);
    Require(consumed == 3 && produced == 2 && output[0] == 0xCB && output[1] == 0x98 && output[2] == 0xA5);
    Require(sceCesEucJpToUtf8(profile, source, 4, nullptr, output, 4, nullptr) == 0);
    std::puts("13403 assigned JIS cells plus ASCII, half-width katakana and unassigned cells checked");
}

}

int main() {
    TestByteOrder();
    TestErrors();
    TestScalars();
    TestEucJp();
}
