#include "prx/libc/include/general/VabiMacros.hpp"

#include <bit>
#include <cstdint>

namespace {

std::uint64_t Magnitude(std::int64_t value) {
    const auto bits = static_cast<std::uint64_t>(value);
    return value < 0 ? 0 - bits : bits;
}

std::int64_t SignedResult(std::uint64_t magnitude, bool negative) {
    return std::bit_cast<std::int64_t>(negative ? 0 - magnitude : magnitude);
}

}

extern "C" {

std::int64_t APS5_VABI FT_MulDiv_nid_postfix(std::int64_t a, std::int64_t b, std::int64_t c) {
    const auto divisor = Magnitude(c);
    const auto result = divisor ? (Magnitude(a) * Magnitude(b) + divisor / 2) / divisor : 0x7FFFFFFFULL;
    return SignedResult(result, (a < 0) ^ (b < 0) ^ (c < 0));
}

std::int64_t APS5_VABI FT_MulFix_nid_postfix(std::int64_t a, std::int64_t b) {
    const auto first = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a));
    const auto second = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(b));
    const auto product = static_cast<std::int64_t>(first) * second;
    const auto rounded = (product + 0x8000 - (product < 0)) >> 16;
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(rounded));
}

std::int64_t APS5_VABI FT_DivFix_nid_postfix(std::int64_t a, std::int64_t b) {
    const auto divisor = Magnitude(b);
    const auto result = divisor ? ((Magnitude(a) << 16) + divisor / 2) / divisor : 0x7FFFFFFFULL;
    return SignedResult(result, (a < 0) ^ (b < 0));
}

std::int64_t APS5_VABI FT_RoundFix_nid_postfix(std::int64_t a) {
    const auto rounded = (static_cast<std::uint64_t>(a) + 0x8000 - (a < 0)) & ~std::uint64_t{0xFFFF};
    return std::bit_cast<std::int64_t>(rounded);
}

std::int64_t APS5_VABI FT_CeilFix_nid_postfix(std::int64_t a) {
    const auto rounded = (static_cast<std::uint64_t>(a) + 0xFFFF) & ~std::uint64_t{0xFFFF};
    return std::bit_cast<std::int64_t>(rounded);
}

std::int64_t APS5_VABI FT_FloorFix_nid_postfix(std::int64_t a) {
    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(a) & ~std::uint64_t{0xFFFF});
}

}
