#include "GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
using anyps5::experimental::GuestCpu;

static bool verify32(std::uint32_t dividend, std::uint32_t divisor) {
    // mov eax, dividend; xor edx,edx; mov ecx,divisor; div ecx; ret
    std::vector<std::uint8_t> code{0xB8};
    for (unsigned i = 0; i < 4; ++i)
        code.push_back(static_cast<std::uint8_t>(dividend >> (8 * i)));
    code.insert(code.end(), {0x31,0xD2,0xB9});
    for (unsigned i = 0; i < 4; ++i)
        code.push_back(static_cast<std::uint8_t>(divisor >> (8 * i)));
    code.insert(code.end(), {0xF7,0xF1,0xC3});
    GuestCpu cpu(code);
    const auto quotient = cpu.run();
    if (quotient != dividend / divisor || cpu.registerValue(2) != dividend % divisor) {
        std::fprintf(stderr, "DIV32 mismatch %u/%u\n", dividend, divisor);
        return false;
    }
    return true;
}
static bool verify64(std::uint64_t dividend, std::uint64_t divisor) {
    std::vector<std::uint8_t> code{0x48,0xB8};
    for (unsigned i = 0; i < 8; ++i)
        code.push_back(static_cast<std::uint8_t>(dividend >> (8 * i)));
    code.insert(code.end(), {0x31,0xD2,0x48,0xB9});
    for (unsigned i = 0; i < 8; ++i)
        code.push_back(static_cast<std::uint8_t>(divisor >> (8 * i)));
    code.insert(code.end(), {0x48,0xF7,0xF1,0xC3});
    GuestCpu cpu(code);
    const auto quotient = cpu.run();
    if (quotient != dividend / divisor || cpu.registerValue(2) != dividend % divisor) {
        std::fprintf(stderr, "DIV64 mismatch\n");
        return false;
    }
    return true;
}
int main() {
    std::uint64_t seed = UINT64_C(0xD43E1ACB7652098F);
    for (unsigned i = 0; i < 512; ++i) {
        seed = seed * UINT64_C(6364136223846793005) + 1;
        const auto divisor = (seed >> 16) | 1u;
        if (!verify32(static_cast<std::uint32_t>(seed), static_cast<std::uint32_t>(divisor)) ||
            !verify64(seed, divisor)) return EXIT_FAILURE;
    }
    std::puts("DIV 32/64 quotient and remainder tests passed (1024 cases)");
    return EXIT_SUCCESS;
}
