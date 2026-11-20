#include "prx/libSceLibcInternal/include/MathInternals.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

constexpr double Infinity = std::numeric_limits<double>::infinity();
constexpr double NotANumber = std::numeric_limits<double>::quiet_NaN();

struct DoubleCase {
    double x;
    std::uint64_t bits;
    int error;
};

struct FloatCase {
    double x;
    std::uint32_t bits;
    int error;
};

struct ExtendedCase {
    double x;
    std::uint16_t top;
    std::uint64_t bits;
    int error;
};

const DoubleCase DoubleCases[] = {
    {0x1p+0, 0xbe205057c0000000ull, 0},
    {0x1p-1, 0x3fe250d048244940ull, 0},
    {0x1.4p+3, 0x40299a8921824b1cull, 0},
    {0x1p-20, 0x402bb9d3ac3864daull, 0},
    {0x0p+0, 0x7ff0000000000000ull, 0},
    {-0x1p+0, 0xfff8000000000000ull, EDOM},
    {Infinity, 0xfff8000000000000ull, ERANGE},
    {NotANumber, 0x7ff8000000000000ull, 0},
};

const FloatCase FloatCases[] = {
    {0x1p+0, 0x34800000u, 0},
    {0x1p-1, 0x3f128688u, 0},
    {0x1.4p+3, 0x414cd449u, 0},
    {0x1.9p+6, 0x43b3912du, 0},
    {0x1.7e43c8800759cp+996, 0xffc00000u, ERANGE},
    {0x0p+0, 0x7f800000u, 0},
    {-0x1.4p+1, 0xffc00000u, EDOM},
    {NotANumber, 0x7fc00000u, 0},
};

const ExtendedCase ExtendedCases[] = {
    {0x1p-1, 0x3ffe, 0x92868241224a1078ull, 0},
    {0x1.4p+3, 0x4002, 0xccd4490c1258e380ull, 0},
    {0x1.8p+0, 0xbffb, 0xf75cadd4ae643600ull, 0},
    {0x0p+0, 0x7fff, 0x8000000000000000ull, 0},
    {-0x0p+0, 0x7fff, 0xc000000000000000ull, EDOM},
    {-Infinity, 0x7fff, 0xc000000000000000ull, ERANGE},
};

void Require(bool condition, const char* label, double x) {
    if (condition) return;
    std::fprintf(stderr, "libc internal log gamma: %s x=%a\n", label, x);
    std::abort();
}

}

int main() {
    for (const auto& entry : DoubleCases) {
        errno = 0;
        const double result = _ZSt8_XLgammad_nid_postfix(entry.x);
        std::uint64_t bits;
        std::memcpy(&bits, &result, sizeof(bits));
        Require(bits == entry.bits && errno == entry.error, "double", entry.x);
    }
    for (const auto& entry : FloatCases) {
        errno = 0;
        const float result = _ZSt8_XLgammaf_nid_postfix(static_cast<float>(entry.x));
        std::uint32_t bits;
        std::memcpy(&bits, &result, sizeof(bits));
        Require(bits == entry.bits && errno == entry.error, "float", entry.x);
    }
    for (const auto& entry : ExtendedCases) {
        errno = 0;
        const long double result = _ZSt8_XLgammae_nid_postfix(static_cast<long double>(entry.x));
        std::uint64_t bits;
        std::uint16_t top;
        std::memcpy(&bits, &result, sizeof(bits));
        std::memcpy(&top, reinterpret_cast<const unsigned char*>(&result) + sizeof(bits), sizeof(top));
        Require(bits == entry.bits && top == entry.top && errno == entry.error, "long double", entry.x);
    }
    return 0;
}
