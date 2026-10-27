#include "prx/libSceFreeType/include/Api.hpp"
#include <cstdint>

extern "C" std::int32_t APS5_VABI FT_Sqrt32_nid_postfix(std::int32_t value) {
    const auto input = static_cast<std::uint32_t>(value);
    std::uint32_t lower = 0;
    std::uint32_t upper = 65536;
    while (upper - lower > 1) {
        const auto candidate = lower + (upper - lower) / 2;
        if (std::uint64_t{candidate} * candidate <= input) lower = candidate;
        else upper = candidate;
    }
    return static_cast<std::int32_t>(lower);
}
