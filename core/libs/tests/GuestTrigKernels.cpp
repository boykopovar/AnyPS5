#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what, int row) {
    if (value) return;
    std::fprintf(stderr, "failed: %s (row %d)\n", what, row);
    std::abort();
}

struct Row {
    std::uint64_t x, y, cos, sin, sinTail;
    std::uint32_t cosdf, sindf;
};

const Row Rows[] = {
    {0x3e10000000000000ull, 0x0000000000000000ull, 0x3ff0000000000000ull, 0x3e10000000000000ull, 0x3e10000000000000ull, 0x3f800000u, 0x30800000u},
    {0xbe40000000000000ull, 0x3af0000000000000ull, 0x3ff0000000000000ull, 0xbe40000000000000ull, 0xbe3fffffffffffffull, 0x3f800000u, 0xb2000000u},
    {0x3fb999999999999aull, 0x3c32725dd1d243acull, 0x3fefd712f9a817c0ull, 0x3fb98eaecb8bcb2cull, 0x3fb98eaecb8bcb2cull, 0x3f7eb898u, 0x3dcc7576u},
    {0xbfd0000000000000ull, 0xbc42725dd1d243acull, 0x3fef01549f7deea1ull, 0xbfcfaaeed4f31577ull, 0xbfcfaaeed4f31577ull, 0x3f780aa5u, 0xbe7d5777u},
    {0x3fe0000000000000ull, 0x3c814b37f4b51f71ull, 0x3fec1528065b7d4full, 0x3fdeaee8744b05f0ull, 0x3fdeaee8744b05f0ull, 0x3f60a940u, 0x3ef57744u},
    {0x3fe921fb54442d18ull, 0xbc870ef54646d497ull, 0x3fe6a09e667f3bcdull, 0x3fe6a09e667f3bccull, 0x3fe6a09e667f3bccull, 0x3f3504f3u, 0x3f3504f3u},
    {0xbfe921fb54442d18ull, 0x3c91a62633145c07ull, 0x3fe6a09e667f3bcdull, 0xbfe6a09e667f3bccull, 0xbfe6a09e667f3bccull, 0x3f3504f3u, 0xbf3504f3u},
    {0x3fd3333333333333ull, 0x0000000000000000ull, 0x3fee921dd42f09baull, 0x3fd2e9cd95baba33ull, 0x3fd2e9cd95baba33ull, 0x3f7490efu, 0x3e974e6du},
    {0xbfe3333333333333ull, 0xbc670ef54646d497ull, 0x3fea69263c485b15ull, 0xbfe2118d17a54159ull, 0xbfe2118d17a54159ull, 0x3f534932u, 0xbf108c69u},
};

}

int main() {
    int row = 0;
    for (const Row& expected : Rows) {
        const double x = std::bit_cast<double>(expected.x), y = std::bit_cast<double>(expected.y);
        Require(std::bit_cast<std::uint64_t>(__kernel_cos_nid_postfix(x, y)) == expected.cos, "__kernel_cos", row);
        Require(std::bit_cast<std::uint64_t>(__kernel_sin_nid_postfix(x, y, 0)) == expected.sin, "__kernel_sin without tail", row);
        Require(std::bit_cast<std::uint64_t>(__kernel_sin_nid_postfix(x, y, 1)) == expected.sinTail, "__kernel_sin with tail", row);
        Require(std::bit_cast<std::uint32_t>(__kernel_cosdf_nid_postfix(x)) == expected.cosdf, "__kernel_cosdf", row);
        Require(std::bit_cast<std::uint32_t>(__kernel_sindf_nid_postfix(x)) == expected.sindf, "__kernel_sindf", row);
        Require(std::fabs(__kernel_cos_nid_postfix(x, y) - std::cos(x + y)) <= 0x1p-52, "__kernel_cos near cos", row);
        Require(std::fabs(__kernel_sin_nid_postfix(x, y, 1) - std::sin(x + y)) <= 0x1p-52, "__kernel_sin near sin", row);
        ++row;
    }
    Require(std::bit_cast<std::uint64_t>(__kernel_sin_nid_postfix(-0.0, 0.0, 0)) == 0, "__kernel_sin of -0 without tail is +0", row);
    Require(std::bit_cast<std::uint64_t>(__kernel_sin_nid_postfix(-0.0, 0.0, 1)) == std::bit_cast<std::uint64_t>(-0.0), "__kernel_sin of -0 with tail is -0", row);
    Require(__kernel_cos_nid_postfix(0.0, 0.0) == 1.0 && __kernel_cosdf_nid_postfix(0.0) == 1.0f, "kernels at 0", row);
    std::puts("trig kernels: ok");
    return 0;
}
