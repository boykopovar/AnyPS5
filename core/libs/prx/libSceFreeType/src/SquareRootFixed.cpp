#include "prx/libSceFreeType/include/SquareRootFixed.hpp"
#include <cstdint>

extern "C" std::int32_t APS5_VABI FT_SqrtFixed_nid_postfix(std::int32_t value) {
    if (value <= 0) return 0;
    const auto scaled = static_cast<std::uint64_t>(value) << 16;
    std::uint32_t lower = 0;
    std::uint32_t upper = 1u << 24;
    while (upper - lower > 1) {
        const auto candidate = lower + (upper - lower) / 2;
        if (std::uint64_t{candidate} * candidate <= scaled) lower = candidate;
        else upper = candidate;
    }
    return static_cast<std::int32_t>(lower);
}
