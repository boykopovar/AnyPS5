#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>

static_assert(std::is_same_v<decltype(&res_getPublicType_nid_postfix), std::int32_t (APS5_VABI*)(std::uint32_t)>);

namespace {

void Check(std::uint32_t resource, std::int32_t expected) {
    const auto actual = res_getPublicType_nid_postfix(resource);
    if (actual != expected) {
        std::fprintf(stderr, "resource 0x%08x: expected %d, got %d\n", resource, expected, actual);
        std::abort();
    }
}

} // namespace

int main() {
    constexpr std::array<std::int32_t, 16> expectedTypes{0, 1, 2, 3, 2, 2, 0, 7, 8, 8, -1, -1, -1, -1, 14, -1};
    constexpr std::array<std::uint32_t, 6> offsets{0, 1, 0x07ffffff, 0x08000000, 0x0ffffffe, 0x0fffffff};
    std::uint32_t state = 0x62734a91;
    for (std::uint32_t type = 0; type < expectedTypes.size(); ++type) {
        const auto tag = type << 28;
        for (const auto offset : offsets) Check(tag | offset, expectedTypes[type]);
        for (unsigned bit = 0; bit < 28; ++bit) {
            Check(tag | (std::uint32_t{1} << bit), expectedTypes[type]);
            Check(tag | (0x0fffffff ^ (std::uint32_t{1} << bit)), expectedTypes[type]);
        }
        for (unsigned sample = 0; sample < 4096; ++sample) {
            state = state * 1664525u + 1013904223u;
            Check(tag | (state & 0x0fffffff), expectedTypes[type]);
        }
    }
    Check(std::numeric_limits<std::uint32_t>::max(), -1);
    std::puts("ICU resource public type tests passed");
}
