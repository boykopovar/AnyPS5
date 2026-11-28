#include "GuestCpu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using anyps5::experimental::GuestCpu;

#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
static std::uint64_t nativeRotate(std::uint64_t value, unsigned count, bool right, bool wide) {
    const auto c = static_cast<std::uint8_t>(count);
    if (wide) {
        if (right) asm volatile("rorq %%cl,%0" : "+r"(value) : "c"(c) : "cc");
        else asm volatile("rolq %%cl,%0" : "+r"(value) : "c"(c) : "cc");
        return value;
    }
    auto low = static_cast<std::uint32_t>(value);
    if (right) asm volatile("rorl %%cl,%0" : "+r"(low) : "c"(c) : "cc");
    else asm volatile("roll %%cl,%0" : "+r"(low) : "c"(c) : "cc");
    return low;
}

static std::uint64_t guestRotate(std::uint64_t value, unsigned count, bool right, bool wide) {
    std::vector<std::uint8_t> code;
    if (wide) code.push_back(0x48);
    code.push_back(0xB8);
    for (unsigned i = 0; i < (wide ? 8u : 4u); ++i)
        code.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    code.insert(code.end(), {0xB9, static_cast<std::uint8_t>(count), 0, 0, 0});
    if (wide) code.push_back(0x48);
    code.insert(code.end(), {0xD3, static_cast<std::uint8_t>(right ? 0xC8 : 0xC0), 0xC3});
    return GuestCpu(code).run();
}

int main() {
    std::uint64_t seed = UINT64_C(0x6D5A56E9C2B31807);
    const std::array<std::uint64_t, 10> edges{
        0, 1, 2, 0x7F, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF,
        UINT64_C(0x7FFFFFFFFFFFFFFF), UINT64_C(0x8000000000000000), UINT64_MAX
    };
    for (unsigned i = 0; i < 512; ++i) {
        seed = seed * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
        const auto lhs = i < edges.size() ? edges[i] : seed;
        const auto rhs = i < edges.size() ? edges[edges.size() - i - 1] : seed >> 17;
        for (const bool wide : {false, true}) {
            for (const bool right : {false, true}) {
                const auto count = i % 129;
                const auto expected = nativeRotate(lhs, count, right, wide);
                const auto actual = guestRotate(lhs, count, right, wide);
                if (expected != actual) {
                    std::fprintf(stderr, "rotate mismatch i=%u wide=%d right=%d count=%u expected=%llx actual=%llx\n",
                        i, wide, right, count,
                        static_cast<unsigned long long>(expected),
                        static_cast<unsigned long long>(actual));
                    return EXIT_FAILURE;
                }
            }
        }
    }
    std::puts("Rotate differential tests passed (2048 native comparisons)");
    return EXIT_SUCCESS;
}
#else
#error "Native differential tests require x86-64 GCC or Clang"
#endif
