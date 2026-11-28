#include "prx/libc/include/Api.hpp"

#include <bit>
#include <cstdint>

namespace {

constexpr std::uint64_t LargestPrime = 18'446'744'073'709'551'557ull;
constexpr std::uint64_t PrimeBases[]{2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};

std::uint64_t MultiplyModulo(std::uint64_t left, std::uint64_t right, std::uint64_t modulus) {
    return static_cast<std::uint64_t>((static_cast<unsigned __int128>(left) * right) % modulus);
}

std::uint64_t PowerModulo(std::uint64_t base, std::uint64_t exponent, std::uint64_t modulus) {
    std::uint64_t result = 1;
    for (; exponent != 0; exponent >>= 1) {
        if ((exponent & 1) != 0) result = MultiplyModulo(result, base, modulus);
        base = MultiplyModulo(base, base, modulus);
    }
    return result;
}

bool IsPrime(std::uint64_t value) {
    for (const auto prime : PrimeBases) {
        if (value % prime == 0) return value == prime;
        if (value < prime * prime) return true;
    }
    const auto minusOne = value - 1;
    const int shifts = std::countr_zero(minusOne);
    const auto oddPart = minusOne >> shifts;
    for (const auto base : PrimeBases) {
        auto power = PowerModulo(base, oddPart, value);
        if (power == 1 || power == minusOne) continue;
        for (int round = 1; round < shifts && power != minusOne; ++round) {
            power = MultiplyModulo(power, power, value);
        }
        if (power != minusOne) return false;
    }
    return true;
}

} // namespace

extern "C" std::uint64_t APS5_VABI _ZNSt3__112__next_primeEm_nid_postfix(std::uint64_t minimum) {
    if (minimum == 0) return 0;
    if (minimum <= 2) return 2;
    if (minimum > LargestPrime) LibcThrowOverflowError_nid_no_patch("__next_prime overflow");
    auto candidate = minimum | 1;
    while (!IsPrime(candidate)) candidate += 2;
    return candidate;
}
