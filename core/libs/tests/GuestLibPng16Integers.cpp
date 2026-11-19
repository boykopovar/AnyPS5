#include "prx/libpng16/libpng16.hpp"
#include <array>
#include <cstdint>
#include <cstdio>

int main() {
    alignas(4) std::array<std::uint8_t, 5> input{};
    auto* bytes = input.data() + 1;
    for (std::uint32_t value = 0; value <= 0xffff; ++value) {
        bytes[0] = static_cast<std::uint8_t>(value >> 8);
        bytes[1] = static_cast<std::uint8_t>(value);
        if (png_get_uint_16_nid_postfix(bytes) != value) {
            std::fputs("16-bit PNG integer mismatch\n", stderr);
            return 1;
        }
    }
    for (const std::uint32_t value : {0u, 1u, 0x12345678u, 0x80000000u, 0x89abcdefu, 0xffffffffu}) {
        bytes[0] = static_cast<std::uint8_t>(value >> 24);
        bytes[1] = static_cast<std::uint8_t>(value >> 16);
        bytes[2] = static_cast<std::uint8_t>(value >> 8);
        bytes[3] = static_cast<std::uint8_t>(value);
        if (png_get_uint_32_nid_postfix(bytes) != value) {
            std::fputs("32-bit PNG integer mismatch\n", stderr);
            return 1;
        }
    }
    return 0;
}
