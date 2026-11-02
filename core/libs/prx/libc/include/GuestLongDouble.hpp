#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTLONGDOUBLE_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTLONGDOUBLE_HPP

#include <cstdint>
#include <cstring>
#include <limits>

namespace LibcDetail {

inline constexpr std::size_t GuestLongDoubleBytes = 16;

static_assert(std::numeric_limits<long double>::digits == 64 || std::numeric_limits<long double>::digits == 113);

inline long double GuestLongDoubleLoad(const void* source) {
    long double value;
    static_assert(sizeof(value) == GuestLongDoubleBytes);
    if constexpr (std::numeric_limits<long double>::digits == 64) {
        std::memcpy(&value, source, sizeof(value));
    } else {
        std::uint64_t mantissa;
        std::uint16_t signExponent;
        std::memcpy(&mantissa, source, sizeof(mantissa));
        std::memcpy(&signExponent, static_cast<const unsigned char*>(source) + 8, sizeof(signExponent));
        const std::uint64_t fraction = mantissa & 0x7FFF'FFFF'FFFF'FFFFull;
        const std::uint64_t low = fraction << 49;
        const std::uint64_t high = (fraction >> 15) | (static_cast<std::uint64_t>(signExponent & 0x7FFF) << 48) | (static_cast<std::uint64_t>(signExponent >> 15) << 63);
        std::memcpy(&value, &low, sizeof(low));
        std::memcpy(reinterpret_cast<unsigned char*>(&value) + 8, &high, sizeof(high));
    }
    return value;
}

inline void GuestLongDoubleStore(void* target, long double value) {
    if constexpr (std::numeric_limits<long double>::digits == 64) {
        std::memcpy(target, &value, sizeof(value));
    } else {
        std::uint64_t low;
        std::uint64_t high;
        std::memcpy(&low, &value, sizeof(low));
        std::memcpy(&high, reinterpret_cast<const unsigned char*>(&value) + 8, sizeof(high));
        const auto exponent = static_cast<std::uint16_t>((high >> 48) & 0x7FFF);
        const std::uint64_t fraction = ((high & 0xFFFF'FFFF'FFFFull) << 15) | (low >> 49);
        const std::uint64_t mantissa = fraction | (exponent != 0 ? 0x8000'0000'0000'0000ull : 0);
        const auto signExponent = static_cast<std::uint16_t>(exponent | ((high >> 63) << 15));
        unsigned char bytes[GuestLongDoubleBytes] = {};
        std::memcpy(bytes, &mantissa, sizeof(mantissa));
        std::memcpy(bytes + 8, &signExponent, sizeof(signExponent));
        std::memcpy(target, bytes, sizeof(bytes));
    }
}

}

#endif
