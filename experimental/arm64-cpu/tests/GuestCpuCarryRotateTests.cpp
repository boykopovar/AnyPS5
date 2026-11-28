#include "GuestCpu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using anyps5::experimental::GuestCpu;
static bool expect(const char* name, const std::vector<std::uint8_t>& bytes,
                   std::uint64_t expected) {
    try {
        const auto actual = GuestCpu(bytes).run();
        if (actual == expected) return true;
        std::fprintf(stderr, "%s expected %llu got %llu\n", name,
            static_cast<unsigned long long>(expected),
            static_cast<unsigned long long>(actual));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s exception %s\n", name, e.what());
    }
    return false;
}
int main() {
    // CMP 0,1 sets CF. ADC adds source and carry-in.
    if (!expect("ADC r32,imm8 carry", {
        0xB8, 0,0,0,0, 0x83,0xF8,1,
        0x83,0xD0,41, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("SBB r32,imm8 borrow", {
        0xB8, 50,0,0,0, 0x83,0xF8,51,
        0x83,0xD8,7, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("ADC reg-reg", {
        0xB8, 40,0,0,0, 0xB9,1,0,0,0,
        0x83,0xF8,41, 0x13,0xC1, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("SBB reg-reg", {
        0xB8, 50,0,0,0, 0xB9,7,0,0,0,
        0x83,0xF8,51, 0x1B,0xC1, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("ADC imm32", {
        0xB8,0,0,0,0, 0x83,0xF8,1,
        0x81,0xD0,41,0,0,0, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("SBB imm32", {
        0xB8,50,0,0,0, 0x83,0xF8,51,
        0x81,0xD8,7,0,0,0, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("ADC 64-bit carry propagation", {
        0x48,0xB8,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0x83,0xF8,0, // CMP eax,0 clears CF (eax is -1)
        0x48,0x83,0xD0,1, 0xC3
    }, 0)) return EXIT_FAILURE;
    if (!expect("ROL 32-bit", {
        0xB8,0x15,0,0,0, 0xC1,0xC0,1, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("ROR 32-bit", {
        0xB8,84,0,0,0, 0xD1,0xC8, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("ROL 64-bit", {
        0x48,0xB8,21,0,0,0,0,0,0,0,
        0x48,0xD1,0xC0, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("ROR CL", {
        0xB8,84,0,0,0, 0xB9,1,0,0,0,
        0xD3,0xC8, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!expect("ROL zero count preserves carry", {
        0xB8,1,0,0,0, 0x83,0xF8,2,
        0xC1,0xC0,0, 0x0F,0x92,0xC0,
        0x0F,0xB6,0xC0, 0xC3
    }, 1)) return EXIT_FAILURE;
    if (!expect("ROR sets carry", {
        0xB8,1,0,0,0, 0xD1,0xC8,
        0x0F,0x92,0xC0, 0x0F,0xB6,0xC0, 0xC3
    }, 1)) return EXIT_FAILURE;
    if (!expect("ROL preserves zero flag", {
        0xB8,1,0,0,0, 0x83,0xF8,1,
        0xD1,0xC0, 0x0F,0x94,0xC0,
        0x0F,0xB6,0xC0, 0xC3
    }, 1)) return EXIT_FAILURE;
    std::puts("ADC SBB and rotate tests passed");
    return EXIT_SUCCESS;
}
