#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool value, const char* what, int row) {
    if (value) return;
    std::fprintf(stderr, "failed: %s (row %d)\n", what, row);
    std::abort();
}

struct Row {
    int prec, e0, nx, n;
    std::uint64_t x[5], y[3];
};

const Row Rows[] = {
    {1, 6, 1, 6, {0x416f400000000000ull, 0, 0, 0, 0}, {0x3fe59ea068096c50ull, 0x3c6c2f6142477518ull, 0}},
    {1, 50, 3, 3, {0x4160f0cf00000000ull, 0x4149375600000000ull, 0x4162000000000000ull, 0, 0}, {0x3fe19eab99633cd8ull, 0xbc6269e0fc06b508ull, 0}},
    {1, 309, 3, 4, {0x416249ad20000000ull, 0x4146530d80000000ull, 0x416d000000000000ull, 0, 0}, {0xbfd8fdb85583ea1bull, 0xbc6084ae13699e73ull, 0}},
    {1, 973, 3, 7, {0x4167e43c80000000ull, 0x415000eb00000000ull, 0x416c000000000000ull, 0, 0}, {0xbfe39e51e3b9d3d4ull, 0x3c6be718f9f1e074ull, 0}},
    {1, 1000, 3, 2, {0x416fffffe0000000ull, 0x416fffffe0000000ull, 0x416f000000000000ull, 0, 0}, {0xbf7453020ff06b39ull, 0xbc1afad10278554cull, 0}},
    {1, 826, 3, 5, {0x4166ac5b20000000ull, 0x4148b28780000000ull, 0x416f000000000000ull, 0, 0}, {0x3c214ae72e6ba22full, 0xb8973f0853f8ab5full, 0}},
    {1, 826, 3, 5, {0x4166ac5b20000000ull, 0x4148b28780000000ull, 0x416f000000000000ull, 0, 0}, {0x3c214ae72e6ba22full, 0xb8973f0853f8ab5full, 0}},
    {1, 977, 3, 1, {0x416921fb40000000ull, 0x4164442d00000000ull, 0x4168000000000000ull, 0, 0}, {0xbfe1097de15d9c7bull, 0x3c65ce58f183c3e6ull, 0}},
    {1, 1000, 1, 6, {0x4160000000000000ull, 0, 0, 0, 0}, {0xbfe3242cd2724cceull, 0x3c402166d1c1445eull, 0}},
    {1, 478, 1, 5, {0x4168000000000000ull, 0, 0, 0, 0}, {0xbfceb14757285926ull, 0x3c6136c90c9ee26cull, 0}},
    {1, 7, 3, 7, {0x416fffffe0000000ull, 0x416fffffe0000000ull, 0x416f000000000000ull, 0, 0}, {0x3fcebc24b95fe14full, 0xbc66065ceb958c39ull, 0}},
    {1, 3, 1, 5, {0x4168000000000000ull, 0, 0, 0, 0}, {0xbfe4d8779aed6669ull, 0x3c8c19bcae246527ull, 0}},
    {1, 2, 1, 1, {0x4164000000000000ull, 0, 0, 0, 0}, {0xbfe54084d55a568dull, 0xbc539823dd2d7443ull, 0}},
    {0, 76, 1, 7, {0x41693e5940000000ull, 0, 0, 0, 0}, {0xbfe50f0d49e8d06full, 0, 0}},
    {0, 104, 1, 4, {0x416fffffe0000000ull, 0, 0, 0, 0}, {0xbfe191cfe681daf7ull, 0, 0}},
    {0, 4, 1, 6, {0x41699bc5c0000000ull, 0, 0, 0, 0}, {0x3fdd97431a00cb68ull, 0, 0}},
    {0, 77, 1, 7, {0x4160000000000000ull, 0, 0, 0, 0}, {0x3fe05b529eee170cull, 0, 0}},
    {0, 40, 1, 0, {0x41678c29c0000000ull, 0, 0, 0, 0}, {0xbfe450dcaa8dec3aull, 0, 0}},
    {2, 16360, 3, 5, {0x416fffffe0000000ull, 0x416fffffe0000000ull, 0x416fffe000000000ull, 0, 0}, {0x3fc00c38a490fecaull, 0x3c655974b03eb57aull, 0}},
    {2, 15977, 3, 6, {0x416921fb40000000ull, 0x4164442d00000000ull, 0x416846a000000000ull, 0, 0}, {0xbfd4a068d433df9aull, 0xbc7828588d40cb77ull, 0}},
    {2, 13264, 3, 0, {0x416a375060000000ull, 0x4141ff2a80000000ull, 0x41618c2000000000ull, 0, 0}, {0x3fe33f9187d9bca1ull, 0xbc759d6682e36ed0ull, 0}},
    {2, 977, 1, 4, {0x4160000000000000ull, 0, 0, 0, 0}, {0xbfc4770357ba64a4ull, 0xbc60195cad8b15d7ull, 0}},
    {2, 7978, 3, 3, {0x416921fb40000000ull, 0x4164442d00000000ull, 0x4168468000000000ull, 0, 0}, {0xbfad047017948d04ull, 0x3c41de6006899aa0ull, 0}},
    {0, 13, 1, 2, {0x416a7b82c0000000ull, 0, 0, 0, 0}, {0xbfd1a128b92b95afull, 0, 0}},
    {1, -173, 1, 0, {0x416acca380000000ull, 0, 0, 0, 0}, {0x369acca380000000ull, 0xaf2dcbc5a78fb180ull, 0}},
    {2, 481, 1, 7, {0x416843e380000000ull, 0, 0, 0, 0}, {0x3fe5672e9c1220cdull, 0xbc87cf9e6dbf5c8aull, 0}},
    {3, 50, 1, 7, {0x4169a3b9e0000000ull, 0, 0, 0, 0}, {0x3fcf79272a1e256full, 0xbc6c909ad8a4dee8ull, 0x39089c8c1f2cf513ull}},
    {0, 13404, 2, 1, {0x41406d3b80000000ull, 0x4165fca660000000ull, 0, 0, 0}, {0xbfe65f13978ebab8ull, 0, 0}},
    {1, 9651, 2, 3, {0x4162c375e0000000ull, 0x41430f5900000000ull, 0, 0, 0}, {0x3fe7da1ee98a5fa1ull, 0x3c418bd63bd4b72full, 0}},
    {2, 4, 2, 1, {0x415944da40000000ull, 0x415f9c2640000000ull, 0, 0, 0}, {0xbfe8cbb56988e412ull, 0xbc6d461927b28405ull, 0}},
    {3, -189, 2, 0, {0x41676d98c0000000ull, 0x41513f7a00000000ull, 0, 0, 0}, {0x35976d98c89fbd00ull, 0xab1db933fde5b4c0ull, 0x0000000000000000ull}},
    {0, 817, 3, 0, {0x4163fff780000000ull, 0x4156864000000000ull, 0x4156ebcb80000000ull, 0, 0}, {0x3fd868eb22fc89eeull, 0, 0}},
    {1, 146, 3, 2, {0x416543ac40000000ull, 0x4167209800000000ull, 0x416b559cc0000000ull, 0, 0}, {0x3fe7c3d1bf492c08ull, 0x3c872036300fb723ull, 0}},
    {2, 14873, 3, 3, {0x4162c3b300000000ull, 0x4100151000000000ull, 0x41590adf80000000ull, 0, 0}, {0x3fdfb6d8d32c581bull, 0xbc1c1f92f39893f0ull, 0}},
    {3, 2597, 3, 0, {0x41698f1560000000ull, 0x413f730100000000ull, 0x4151ce8800000000ull, 0, 0}, {0xbfe0a0e9e00985b1ull, 0xbc89b976b20f4eadull, 0xb91bfba70ade7fc7ull}},
    {0, 2, 4, 6, {0x41665bf4e0000000ull, 0x4162ebff40000000ull, 0x4154abae40000000ull, 0x4156edee00000000ull, 0}, {0xbfdd56bb759c1f64ull, 0, 0}},
    {1, -67, 4, 0, {0x416f8b2d20000000ull, 0x416d5a3ea0000000ull, 0x41663dd100000000ull, 0x416b472940000000ull, 0}, {0x3d3f8b2d3d5a3eb6ull, 0x39cee88da394a000ull, 0}},
    {2, 889, 4, 7, {0x4169b967e0000000ull, 0x4151920800000000ull, 0x416b32b900000000ull, 0x415ced7600000000ull, 0}, {0xbfe4e3cd31ac2c8bull, 0x3c8cbeb6425487e5ull, 0}},
    {3, 818, 4, 5, {0x415aa24f00000000ull, 0x4153ca77c0000000ull, 0x4160422a00000000ull, 0x4165ffee00000000ull, 0}, {0xbfc0a915f56dca31ull, 0xbc69455db16b448dull, 0x38f7ce53478139f8ull}},
    {0, 16163, 5, 7, {0x4151ed96c0000000ull, 0x4165ec45c0000000ull, 0x41332eef00000000ull, 0x416cdbf5c0000000ull, 0x4122798e00000000ull}, {0x3fe48633a2c18a7dull, 0, 0}},
    {1, 3717, 5, 0, {0x413935ac00000000ull, 0x415cbf4f40000000ull, 0x415192dc40000000ull, 0x414f4a2580000000ull, 0x4166051a80000000ull}, {0xbfb840d058a5ba94ull, 0x3c4966e77d37a30cull, 0}},
    {2, 9, 5, 0, {0x414d04b180000000ull, 0x4143be0100000000ull, 0x4148222980000000ull, 0x412f676600000000ull, 0x415e409a80000000ull}, {0x3fe81c180375dd6full, 0xbc639b38089cfe5full, 0}},
    {3, -105, 5, 0, {0x4166f2daa0000000ull, 0x41690bea20000000ull, 0x4105988800000000ull, 0x415eba8380000000ull, 0x41628db8a0000000ull}, {0x3ad6f2dab90bea20ull, 0x3775988bd75074a3ull, 0x341b8a0000000000ull}},
    {0, 577, 1, 1, {0x416bfa94a0000000ull, 0, 0, 0, 0}, {0xbfa5fd1cd1d0531eull, 0, 0}},
    {1, 146, 1, 3, {0x416d502a20000000ull, 0, 0, 0, 0}, {0xbfe7155b26614f2eull, 0xbc83e087a0b70322ull, 0}},
    {2, 4923, 1, 3, {0x41565a4c80000000ull, 0, 0, 0, 0}, {0x3fe6ed4cd89f5004ull, 0xbc7c2c028f8ac3b0ull, 0}},
    {3, 441, 1, 2, {0x415fb818c0000000ull, 0, 0, 0, 0}, {0xbfdbf45b4b571f20ull, 0xbc63d9c79921a868ull, 0x3905de6d4be3902eull}},
    {3, 16360, 5, 2, {0x4160000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x3ff0000000000000ull}, {0x3fe13d69bcfa30f7ull, 0xbc896f823a097cceull, 0xb9138accc364e123ull}},
    {0, 0, 1, 0, {0x416921fb60000000ull, 0, 0, 0, 0}, {0x3fd777a5cf72ceceull, 0, 0}},
    {1, 24, 3, 4, {0x41645f3060000000ull, 0x415b939100000000ull, 0x413529fc00000000ull, 0, 0}, {0x3fe8e884963bd644ull, 0x3c8c435debf315ecull, 0}},
    {2, 240, 3, 3, {0x41645f3060000000ull, 0x415b939100000000ull, 0x413529fc00000000ull, 0, 0}, {0xbfde69547dc02e13ull, 0xbc67ed46ec56cb72ull, 0}},
    {0, 48, 2, 4, {0x4132345700000000ull, 0x416fdb9740000000ull, 0, 0, 0}, {0x3fe458cb4834bd58ull, 0, 0}},
    {3, 4800, 2, 6, {0x4132345700000000ull, 0x416fdb9740000000ull, 0, 0, 0}, {0xbfd5034aeb5ad195ull, 0x3c721fb73e39c8f0ull, 0xb903348120f350b1ull}},
};

constexpr std::uint64_t Unwritten = 0x7ff4000000000badull;

int Outputs(int prec) {
    return prec == 0 ? 1 : prec == 3 ? 3 : 2;
}

template <typename Error>
bool Throws(const std::array<double, 5>& chunks, int e0, int nx, int prec) {
    std::array<double, 20> x{};
    for (int i = 0; i < 5; ++i) x[i] = chunks[i];
    std::array<double, 4> y{};
    try {
        __kernel_rem_pio2_nid_postfix(x.data(), y.data(), e0, nx, prec);
    } catch (const Error&) {
        return true;
    }
    return false;
}

}

int main() {
    int row = 0;
    for (const Row& expected : Rows) {
        std::array<double, 5> x{};
        for (int i = 0; i < 5; ++i) x[i] = std::bit_cast<double>(expected.x[i]);
        std::array<double, 4> y;
        y.fill(std::bit_cast<double>(Unwritten));
        const int n = __kernel_rem_pio2_nid_postfix(x.data(), y.data(), expected.e0, expected.nx, expected.prec);
        Require(n == expected.n, "quadrant", row);
        for (int i = 0; i < Outputs(expected.prec); ++i) Require(std::bit_cast<std::uint64_t>(y[i]) == expected.y[i], "remainder term", row);
        for (int i = Outputs(expected.prec); i < 4; ++i) Require(std::bit_cast<std::uint64_t>(y[i]) == Unwritten, "terms past the precision stay unwritten", row);
        for (int i = 0; i < 5; ++i) Require(std::bit_cast<std::uint64_t>(x[i]) == expected.x[i], "input is left unchanged", row);
        ++row;
    }
    const std::array<double, 5> one = {1.0, 0.0, 0.0, 0.0, 0.0};
    Require(Throws<std::invalid_argument>(one, 0, 1, -1), "negative precision", row);
    Require(Throws<std::invalid_argument>(one, 0, 1, 4), "precision past quad", row);
    Require(Throws<std::invalid_argument>(one, 0, 0, 1), "no input chunks", row);
    Require(Throws<std::invalid_argument>(one, 0, 17, 1), "more chunks than the work arrays hold", row);
    Require(!Throws<std::exception>(one, 0, 16, 1), "as many chunks as the work arrays hold", row);
    Require(Throws<std::invalid_argument>(one, std::numeric_limits<int>::min(), 1, 1), "exponent that overflows", row);
    Require(Throws<std::out_of_range>({0.0, 0.0, 0.0, 0.0, 0.0}, 100, 3, 1), "zero input never terminates upstream", row);
    Require(Throws<std::out_of_range>(one, 16600, 1, 3), "exponent past the 2/pi table", row);
    Require(Throws<std::out_of_range>(one, std::numeric_limits<int>::max(), 1, 1), "largest exponent", row);
    Require(Throws<std::out_of_range>(one, 16491, 1, 0), "first exponent past the table for single precision", row);
    Require(!Throws<std::exception>({3.0, 0.0, 0.0, 0.0, 0.0}, 16470, 1, 0), "exponent near the end of the table for single precision", row);
    Require(Throws<std::domain_error>({std::numeric_limits<double>::infinity(), 0.0, 0.0, 0.0, 0.0}, 100, 1, 1), "infinite chunk", row);
    Require(Throws<std::domain_error>({std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0, 0.0}, 100, 1, 1), "NaN chunk", row);
    double y[3];
    bool threw = false;
    try {
        __kernel_rem_pio2_nid_postfix(nullptr, y, 0, 1, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "null input", row);
    std::puts("__kernel_rem_pio2: ok");
    return 0;
}
