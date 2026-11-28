#include "prx/libSceCesCs/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr std::uint32_t Unset = 0xBBBBBBBBu;
constexpr std::uint8_t UnsetByte = 0xAA;
constexpr int InvalidEncode = static_cast<int>(0x805C0014);
constexpr int UnassignedCode = static_cast<int>(0x805C0020);

int failures = 0;

void Expect(bool ok, const char* name) {
    if (ok) return;
    std::fprintf(stderr, "FAILED %s\n", name);
    ++failures;
}

std::uint8_t sheet[4096];

const void* EucCnProfile() {
    std::memset(sheet, 0, 2048);
    std::memset(sheet + 2048, 0xCD, 2048);
    return sceCesUcsProfileInitEucCnGb2312(sheet);
}

bool IsC1(std::uint32_t value) {
    return (value >= 0x80 && value <= 0x8D) || (value >= 0x90 && value <= 0x9F);
}

void FirstBytes(const void* profile) {
    for (std::uint32_t lead = 0x80; lead <= 0xA0; ++lead) {
        const std::uint8_t alone[1] = {static_cast<std::uint8_t>(lead)};
        const std::uint8_t pair[2] = {static_cast<std::uint8_t>(lead), 0xA1};
        std::uint32_t length = Unset;
        std::uint32_t scalar = Unset;
        const int first = sceCesMbcToUtf32(profile, alone, 1, &length, &scalar);
        char name[64];
        std::snprintf(name, sizeof(name), "EucCn first byte 0x%02X alone", lead);
        if (IsC1(lead)) Expect(first == 0 && length == 1 && scalar == lead, name);
        else if (lead == 0x8E || lead == 0x8F || lead == 0xA0) Expect(first == InvalidEncode && length == 1 && scalar == 0, name);
        length = Unset;
        scalar = Unset;
        const int second = sceCesMbcToUtf32(profile, pair, 2, &length, &scalar);
        std::snprintf(name, sizeof(name), "EucCn first byte 0x%02X before 0xA1", lead);
        if (IsC1(lead)) Expect(second == 0 && length == 1 && scalar == lead, name);
        else if (lead == 0x8E || lead == 0x8F || lead == 0xA0) Expect(second == InvalidEncode && length == 1 && scalar == 0, name);
    }
}

void C1Reverse(const void* profile) {
    for (std::uint32_t value = 0x7F; value <= 0xA0; ++value) {
        std::uint8_t out[8];
        std::memset(out, UnsetByte, sizeof(out));
        std::uint32_t length = Unset;
        const int result = sceCesUtf32ToMbc(value, profile, out, 4, &length);
        char name[64];
        std::snprintf(name, sizeof(name), "EucCn U+%04X to bytes", value);
        if (value == 0x7F || IsC1(value)) Expect(result == 0 && length == 1 && out[0] == value && out[1] == UnsetByte, name);
        else Expect(result == UnassignedCode && length == 0 && out[0] == UnsetByte, name);
    }
}

struct LeftOut {
    std::uint8_t lead;
    std::uint32_t assigned;
    const std::uint8_t* trails;
    std::size_t count;
};

const std::uint8_t TrailsA2[] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xE3, 0xE4, 0xEF, 0xF0, 0xFD, 0xFE};
const std::uint8_t TrailsA6[] = {0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE};
const std::uint8_t TrailsA8[] = {0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE};

void LeftOutCells(const void* profile) {
    const LeftOut rows[] = {
        {0xA2, 72, TrailsA2, sizeof(TrailsA2)},
        {0xA6, 48, TrailsA6, sizeof(TrailsA6)},
        {0xA8, 63, TrailsA8, sizeof(TrailsA8)},
    };
    for (const auto& row : rows) {
        std::uint32_t converted = 0;
        for (std::uint32_t trail = 0xA1; trail <= 0xFE; ++trail) {
            const std::uint8_t pair[2] = {row.lead, static_cast<std::uint8_t>(trail)};
            std::uint32_t length = Unset;
            std::uint32_t scalar = Unset;
            if (sceCesMbcToUtf32(profile, pair, 2, &length, &scalar) == 0) ++converted;
        }
        char name[64];
        std::snprintf(name, sizeof(name), "EucCn lead 0x%02X converts %u cells", row.lead, row.assigned);
        Expect(converted == row.assigned, name);
        for (std::size_t index = 0; index < row.count; ++index) {
            const std::uint8_t pair[2] = {row.lead, row.trails[index]};
            std::uint32_t length = Unset;
            std::uint32_t scalar = Unset;
            const int result = sceCesMbcToUtf32(profile, pair, 2, &length, &scalar);
            std::snprintf(name, sizeof(name), "EucCn left out cell 0x%02X%02X", row.lead, row.trails[index]);
            Expect(result != 0, name);
        }
    }
    const std::uint8_t samples[][2] = {{0xA2, 0xE3}, {0xA6, 0xD9}, {0xA8, 0xBF}};
    for (const auto& pair : samples) {
        std::uint32_t length = Unset;
        std::uint32_t scalar = Unset;
        const int result = sceCesMbcToUtf32(profile, pair, 2, &length, &scalar);
        char name[64];
        std::snprintf(name, sizeof(name), "EucCn left out cell 0x%02X%02X output", pair[0], pair[1]);
        Expect(result == UnassignedCode && length == 2 && scalar == 0, name);
    }
}

void LeftOutReverse(const void* profile) {
    struct Cell {
        std::uint32_t scalar;
        std::uint16_t code;
    };
    const Cell cells[] = {{0x2170, 0xA2A1}, {0x2171, 0xA2A2}, {0x2172, 0xA2A3}, {0x2173, 0xA2A4}, {0x2174, 0xA2A5}, {0x2175, 0xA2A6}, {0x2176, 0xA2A7}, {0x2177, 0xA2A8}, {0x2178, 0xA2A9}, {0x2179, 0xA2AA}, {0xFE35, 0xA6E0}, {0xFE36, 0xA6E1}, {0xFE39, 0xA6E2}, {0xFE3A, 0xA6E3}, {0xFE3F, 0xA6E4}, {0xFE40, 0xA6E5}, {0xFE3D, 0xA6E6}, {0xFE3E, 0xA6E7}, {0xFE41, 0xA6E8}, {0xFE42, 0xA6E9}, {0xFE43, 0xA6EA}, {0xFE44, 0xA6EB}, {0xFE3B, 0xA6EE}, {0xFE3C, 0xA6EF}, {0xFE37, 0xA6F0}, {0xFE38, 0xA6F1}, {0xFE31, 0xA6F2}, {0xFE33, 0xA6F4}, {0xFE34, 0xA6F5}, {0x0251, 0xA8BB}};
    for (const auto& cell : cells) {
        std::uint8_t out[8];
        std::memset(out, UnsetByte, sizeof(out));
        std::uint32_t length = Unset;
        const int result = sceCesUtf32ToMbc(cell.scalar, profile, out, 4, &length);
        char name[64];
        std::snprintf(name, sizeof(name), "EucCn U+%04X to bytes", cell.scalar);
        Expect(result == 0 && length == 2 && out[0] == (cell.code >> 8) && out[1] == (cell.code & 0xFF) && out[2] == UnsetByte, name);
    }
}

void ShiftOut(const void* profile) {
    const std::uint8_t byte[1] = {0x0E};
    std::uint32_t length = Unset;
    std::uint32_t scalar = Unset;
    Expect(sceCesMbcToUtf32(profile, byte, 1, &length, &scalar) == InvalidEncode && length == 1 && scalar == 0, "EucCn byte 0x0E");
    std::uint8_t out[8];
    std::memset(out, UnsetByte, sizeof(out));
    length = Unset;
    Expect(sceCesUtf32ToMbc(0x0E, profile, out, 4, &length) == UnassignedCode && length == 0 && out[0] == UnsetByte, "EucCn U+000E to bytes");
}
}

int main() {
    const void* profile = EucCnProfile();
    FirstBytes(profile);
    C1Reverse(profile);
    LeftOutCells(profile);
    LeftOutReverse(profile);
    ShiftOut(profile);
    return failures == 0 ? 0 : 1;
}
