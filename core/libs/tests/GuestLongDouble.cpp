#include "prx/libc/include/GuestLongDouble.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

[[gnu::noinline]] long double MakeLongDoubleValue(double base, double offset) {
    return static_cast<long double>(base) + static_cast<long double>(offset);
}

[[gnu::noinline]] void DirtyStackForLongDouble() {
    volatile unsigned char scrub[4096];
    for (std::size_t index = 0; index < sizeof(scrub); ++index) {
        scrub[index] = 0x55;
    }
}

[[gnu::noinline]] void CallGuestLongDoubleStore(void* target, long double value) {
    LibcDetail::GuestLongDoubleStore(target, value);
}

[[gnu::noinline]] int StoreAfterDirtyStack(void* target, double base, double offset) {
    CallGuestLongDoubleStore(target, MakeLongDoubleValue(base, offset));
    volatile int barrier = 0;
    return barrier;
}

void CheckGuestLongDoublePaddingZeroed() {
    std::array<unsigned char, LibcDetail::GuestLongDoubleBytes> stored{};
    stored.fill(0xAA);
    DirtyStackForLongDouble();
    if (StoreAfterDirtyStack(stored.data(), 1.0, 0.5) != 0) std::abort();
    if (LibcDetail::GuestLongDoubleLoad(stored.data()) != 1.5L) std::abort();
    for (std::size_t index = 10; index < stored.size(); ++index) {
        if (stored[index] != 0x00) std::abort();
    }
}

void CheckBinary128LowPayloadNanPreserved() {
    if constexpr (std::numeric_limits<long double>::digits == 113) {
        struct Binary128Case {
            std::uint64_t low;
            std::uint64_t high;
            bool negative;
            bool expectNan;
            std::uint64_t expectedMantissa;
            std::uint16_t expectedSignExponent;
        };
        const Binary128Case cases[] = {
            {1ull, 0x7FFF'0000'0000'0000ull, false, true, 0xC000'0000'0000'0000ull, 0x7FFF},
            {0x0001'0000'0000'0000ull, 0xFFFF'0000'0000'0000ull, true, true, 0xC000'0000'0000'0000ull, 0xFFFF},
            {0x0001'FFFF'FFFF'FFFFull, 0x7FFF'0000'0000'0000ull, false, true, 0xC000'0000'0000'0000ull, 0x7FFF},
            {0x0002'0000'0000'0000ull, 0x7FFF'0000'0000'0000ull, false, true, 0x8000'0000'0000'0001ull, 0x7FFF},
            {0ull, 0x7FFF'0000'0000'0000ull, false, false, 0x8000'0000'0000'0000ull, 0x7FFF},
            {0ull, 0xFFFF'0000'0000'0000ull, true, false, 0x8000'0000'0000'0000ull, 0xFFFF},
        };
        for (const auto& item : cases) {
            long double input;
            std::memcpy(&input, &item.low, sizeof(item.low));
            std::memcpy(reinterpret_cast<unsigned char*>(&input) + 8, &item.high, sizeof(item.high));
            std::array<unsigned char, LibcDetail::GuestLongDoubleBytes> stored{};
            stored.fill(0xAA);
            LibcDetail::GuestLongDoubleStore(stored.data(), input);
            std::uint64_t mantissa = 0;
            std::uint16_t signExponent = 0;
            std::memcpy(&mantissa, stored.data(), sizeof(mantissa));
            std::memcpy(&signExponent, stored.data() + 8, sizeof(signExponent));
            if (mantissa != item.expectedMantissa || signExponent != item.expectedSignExponent) std::abort();
            for (std::size_t index = 10; index < stored.size(); ++index) {
                if (stored[index] != 0x00) std::abort();
            }
            const long double reloaded = LibcDetail::GuestLongDoubleLoad(stored.data());
            if (item.expectNan) {
                if (!std::isnan(reloaded) || std::isinf(reloaded)) std::abort();
            } else {
                if (!std::isinf(reloaded) || std::isnan(reloaded)) std::abort();
            }
            if (std::signbit(reloaded) != item.negative) std::abort();
        }
    }
}

}

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
    CheckGuestLongDoublePaddingZeroed();
    CheckBinary128LowPayloadNanPreserved();
}
