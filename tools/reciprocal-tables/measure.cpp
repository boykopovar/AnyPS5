#include <immintrin.h>
#include <cpuid.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kEntries = 4096;
constexpr std::array<std::uint32_t, 3> kBases{0x3F800000u, 0x3F800000u, 0x40000000u};
constexpr std::array<bool, 3> kSquareRoot{false, true, true};
constexpr std::array<unsigned, 3> kMxcsr{0x1F80, 0x9FC0, 0x7F80};

std::uint32_t native(const bool squareRoot, const std::uint32_t input) {
    const __m128 value = _mm_castsi128_ps(_mm_cvtsi32_si128(static_cast<int>(input)));
    return static_cast<std::uint32_t>(_mm_cvtsi128_si32(_mm_castps_si128(squareRoot ? _mm_rsqrt_ss(value) : _mm_rcp_ss(value))));
}

void describeCpu() {
    unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
    __get_cpuid(0, &eax, &ebx, &ecx, &edx);
    char vendor[13] = {};
    std::memcpy(vendor, &ebx, 4);
    std::memcpy(vendor + 4, &edx, 4);
    std::memcpy(vendor + 8, &ecx, 4);
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    const unsigned baseFamily = (eax >> 8) & 0xF;
    const unsigned family = baseFamily == 0xF ? baseFamily + ((eax >> 20) & 0xFF) : baseFamily;
    const unsigned model = baseFamily >= 0x6 ? ((eax >> 4) & 0xF) | (((eax >> 16) & 0xF) << 4) : (eax >> 4) & 0xF;
    char brand[49] = {};
    for (unsigned leaf = 0; leaf < 3; ++leaf) {
        unsigned regs[4] = {};
        __get_cpuid(0x80000002 + leaf, &regs[0], &regs[1], &regs[2], &regs[3]);
        std::memcpy(brand + leaf * 16, regs, 16);
    }
    std::fprintf(stderr, "cpu: %s family 0x%X model 0x%X stepping %u, %s\n", vendor, family, model, eax & 0xF, brand);
}

bool measure(std::vector<std::uint16_t>& tables) {
    for (std::size_t table = 0; table < kBases.size(); ++table) {
        for (std::uint32_t index = 0; index < kEntries; ++index) {
            const auto result = native(kSquareRoot[table], kBases[table] | (index << 11));
            if (((result >> 23) & 0xFF) != 126 || (result & 0x7FF) != 0) {
                std::fprintf(stderr, "table %zu entry %u is 0x%08X: not exponent 126 with 12 mantissa bits, the stub cannot represent this CPU\n", table, index, result);
                return false;
            }
            tables.push_back(static_cast<std::uint16_t>((result >> 11) & 0xFFF));
        }
    }
    return true;
}

std::uint32_t model(const std::vector<std::uint16_t>& tables, const bool squareRoot, const std::uint32_t input) {
    const std::uint32_t sign = input & 0x80000000u;
    const std::uint32_t exponent = (input >> 23) & 0xFF;
    const std::uint32_t mantissa = input & 0x7FFFFFu;
    if (exponent == 255) {
        if (mantissa != 0) return input | 0x400000u;
        return squareRoot ? (sign != 0 ? 0xFFC00000u : 0) : sign;
    }
    if (exponent == 0) return squareRoot && sign != 0 ? 0xFF800000u : sign | 0x7F800000u;
    if (squareRoot) {
        if (sign != 0) return 0xFFC00000u;
        const std::uint32_t index = ((input >> 11) & 0x1FFF) ^ 0x1000;
        return (((380 - exponent) >> 1) << 23) | (static_cast<std::uint32_t>(tables[kEntries + index]) << 11);
    }
    if (exponent >= 253) return sign;
    return sign | ((253 - exponent) << 23) | (static_cast<std::uint32_t>(tables[(input >> 11) & 0xFFF]) << 11);
}

bool verify(const std::vector<std::uint16_t>& tables) {
    bool same = true;
    for (const auto mxcsr : kMxcsr) {
        _mm_setcsr(mxcsr);
        for (const bool squareRoot : {false, true}) {
            std::uint64_t mismatches = 0;
            std::uint32_t first = 0;
            for (std::uint64_t input = 0; input <= 0xFFFFFFFFull; ++input) {
                const auto value = static_cast<std::uint32_t>(input);
                if (native(squareRoot, value) != model(tables, squareRoot, value) && mismatches++ == 0) first = value;
            }
            std::fprintf(stderr, "%s, MXCSR 0x%04X: %llu of 4294967296 inputs differ from the table model", squareRoot ? "RSQRTSS" : "RCPSS", mxcsr, static_cast<unsigned long long>(mismatches));
            if (mismatches != 0) std::fprintf(stderr, ", first 0x%08X", first);
            std::fprintf(stderr, "\n");
            same = same && mismatches == 0;
        }
    }
    _mm_setcsr(0x1F80);
    return same;
}

bool check(const std::vector<std::uint16_t>& tables, const char* path) {
    std::ifstream file(path);
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto start = text.find("kReciprocalMantissas{");
    const auto end = text.find("};", start);
    if (!file || start == std::string::npos || end == std::string::npos) {
        std::fprintf(stderr, "%s has no kReciprocalMantissas table\n", path);
        return false;
    }
    std::vector<std::uint16_t> stored;
    for (auto position = text.find("0x", start); position < end; position = text.find("0x", position + 2))
        stored.push_back(static_cast<std::uint16_t>(std::stoul(text.substr(position, 5), nullptr, 16)));
    std::size_t differences = 0;
    for (std::size_t index = 0; index < tables.size(); ++index)
        if (index >= stored.size() || stored[index] != tables[index]) ++differences;
    std::fprintf(stderr, "%s: %zu entries, %zu differ from this CPU\n", path, stored.size(), differences);
    return stored.size() == tables.size() && differences == 0;
}

}

int main(const int argc, char** argv) {
    describeCpu();
    std::vector<std::uint16_t> tables;
    if (!measure(tables)) return 1;
    bool ok = true;
    bool printed = false;
    for (int arg = 1; arg < argc; ++arg) {
        if (std::strcmp(argv[arg], "--verify") == 0) {
            ok = verify(tables) && ok;
        } else if (std::strcmp(argv[arg], "--check") == 0 && arg + 1 < argc) {
            ok = check(tables, argv[++arg]) && ok;
            printed = true;
        } else {
            std::fprintf(stderr, "usage: %s [--verify] [--check SharedStubData.cpp]\n", argv[0]);
            return 2;
        }
    }
    if (!printed)
        for (std::size_t index = 0; index < tables.size(); ++index)
            std::printf("%s0x%03X,%s", index % 16 == 0 ? "    " : " ", tables[index], index % 16 == 15 ? "\n" : "");
    return ok ? 0 : 1;
}
