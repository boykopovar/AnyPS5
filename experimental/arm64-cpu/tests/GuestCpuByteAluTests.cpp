#include "GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

using anyps5::experimental::GuestCpu;

static bool runCase(const char* name, const std::vector<std::uint8_t>& code,
                    std::uint64_t expected, std::size_t stackBytes = 64) {
    try {
        const auto actual = GuestCpu(code, stackBytes).run();
        if (actual == expected) return true;
        std::fprintf(stderr, "%s expected=%llu actual=%llu\n", name,
            static_cast<unsigned long long>(expected),
            static_cast<unsigned long long>(actual));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s exception: %s\n", name, e.what());
    }
    return false;
}

int main() {
    // B0..B7, including AH and extended low-byte registers.
    if (!runCase("mov al,imm8", {0xB8, 0, 0, 0, 0, 0xB0, 42, 0xC3}, 42))
        return EXIT_FAILURE;
    if (!runCase("mov ah,imm8", {
        0xB8, 0, 0, 0, 0, 0xB4, 0x12, 0xC3
    }, 0x1200)) return EXIT_FAILURE;
    if (!runCase("66 ignored for byte immediate", {
        0x66, 0xB0, 42, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!runCase("REX selects spl instead of ah", {
        0xB8, 42, 0, 0, 0, 0x40, 0xB4, 0x01, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!runCase("REX.B selects r8b", {
        0x41, 0xB0, 42, 0x44, 0x88, 0xC0, 0xC3
    }, 42)) return EXIT_FAILURE;

    // Byte immediate logic and arithmetic, with SETcc observation.
    if (!runCase("80 add byte", {
        0xB0, 40, 0x80, 0xC0, 2, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!runCase("80 sub byte", {
        0xB0, 50, 0x80, 0xE8, 8, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!runCase("80 or and xor", {
        0xB0, 0x0F,
        0x80, 0xC8, 0x20,
        0x80, 0xE0, 0x2A,
        0x80, 0xF0, 0x00,
        0xC3
    }, 42)) return EXIT_FAILURE;
    if (!runCase("80 cmp equality sets ZF", {
        0xB0, 42, 0x80, 0xF8, 42,
        0x0F, 0x94, 0xC0, 0x0F, 0xB6, 0xC0, 0xC3
    }, 1)) return EXIT_FAILURE;
    if (!runCase("80 cmp unsigned borrow", {
        0xB0, 1, 0x80, 0xF8, 2,
        0x0F, 0x92, 0xC0, 0x0F, 0xB6, 0xC0, 0xC3
    }, 1)) return EXIT_FAILURE;
    if (!runCase("80 add signed overflow", {
        0xB0, 127, 0x80, 0xC0, 1,
        0x0F, 0x90, 0xC0, 0x0F, 0xB6, 0xC0, 0xC3
    }, 1)) return EXIT_FAILURE;
    if (!runCase("80 add parity", {
        0xB0, 1, 0x80, 0xC0, 2,
        0x0F, 0x9A, 0xC0, 0x0F, 0xB6, 0xC0, 0xC3
    }, 1)) return EXIT_FAILURE;

    // TEST byte register and memory; flags change without operand mutation.
    if (!runCase("84 TEST high byte", {
        0xB4, 3, 0xB0, 1, 0x84, 0xE4,
        0x0F, 0x9A, 0xC0, 0x0F, 0xB6, 0xC0, 0xC3
    }, 1)) return EXIT_FAILURE;
    if (!runCase("84 TEST zero", {
        0xB0, 0xF0, 0xB1, 0x0F, 0x84, 0xC8,
        0x0F, 0x94, 0xC0, 0x0F, 0xB6, 0xC0, 0xC3
    }, 1)) return EXIT_FAILURE;
    if (!runCase("80 memory update", {
        0xC6, 0x44, 0x24, 0xF8, 40,
        0x80, 0x44, 0x24, 0xF8, 2,
        0x0F, 0xB6, 0x44, 0x24, 0xF8, 0xC3
    }, 42)) return EXIT_FAILURE;
    if (!runCase("84 memory TEST", {
        0xC6, 0x44, 0x24, 0xF8, 3,
        0xB0, 1, 0x84, 0x44, 0x24, 0xF8,
        0x0F, 0x9A, 0xC0, 0x0F, 0xB6, 0xC0, 0xC3
    }, 0)) return EXIT_FAILURE;

    // Truncated immediate and invalid guest memory must be rejected.
    for (const auto& code : {
        std::vector<std::uint8_t>{0x80, 0xC0},
        std::vector<std::uint8_t>{0xB0},
        std::vector<std::uint8_t>{0x84},
        std::vector<std::uint8_t>{0x80, 0x04, 0x24, 1, 0xC3}
    }) {
        bool rejected = false;
        try { (void)GuestCpu(code, 64).run(); }
        catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) {
            std::fprintf(stderr, "invalid byte instruction accepted\n");
            return EXIT_FAILURE;
        }
    }
    std::puts("Byte ALU instruction tests passed");
    return EXIT_SUCCESS;
}
