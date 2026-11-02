#include "prx/libc/include/GuestLongDouble.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

int main() {
    struct Case {
        std::uint64_t mantissa;
        std::uint16_t signExponent;
        long double expected;
    };
    const Case cases[] = {
        {0, 0, 0.0L}, {0, 0x8000, -0.0L},
        {0x8000000000000000ull, 0x3fff, 1.0L}, {0xc000000000000000ull, 0xbfff, -1.5L},
        {0x8000000000000000ull, 1, std::ldexp(1.0L, -16382)},
        {1, 0, std::ldexp(1.0L, -16445)},
    };
    for (const auto& item : cases) {
        std::array<unsigned char, LibcDetail::GuestLongDoubleBytes> guest{};
        std::memcpy(guest.data(), &item.mantissa, sizeof(item.mantissa));
        std::memcpy(guest.data() + 8, &item.signExponent, sizeof(item.signExponent));
        const auto loaded = LibcDetail::GuestLongDoubleLoad(guest.data());
        if (loaded != item.expected || std::signbit(loaded) != std::signbit(item.expected)) std::abort();
        std::array<unsigned char, LibcDetail::GuestLongDoubleBytes> stored{};
        LibcDetail::GuestLongDoubleStore(stored.data(), loaded);
        if (std::memcmp(stored.data(), guest.data(), 10) != 0) std::abort();
    }
    for (const auto mantissa : {0x8000000000000000ull, 0xc000000000000123ull}) {
        std::array<unsigned char, LibcDetail::GuestLongDoubleBytes> guest{};
        const std::uint16_t exponent = 0x7fff;
        std::memcpy(guest.data(), &mantissa, sizeof(mantissa));
        std::memcpy(guest.data() + 8, &exponent, sizeof(exponent));
        const auto loaded = LibcDetail::GuestLongDoubleLoad(guest.data());
        if (mantissa == 0x8000000000000000ull ? !std::isinf(loaded) : !std::isnan(loaded)) std::abort();
        std::array<unsigned char, LibcDetail::GuestLongDoubleBytes> stored{};
        LibcDetail::GuestLongDoubleStore(stored.data(), loaded);
        if (std::memcmp(stored.data(), guest.data(), 10) != 0) std::abort();
    }
}
