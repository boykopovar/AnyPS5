#include "prx/libc/libc.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

static double D(std::uint64_t bits) { return std::bit_cast<double>(bits); }

static float F(std::uint32_t bits) { return std::bit_cast<float>(bits); }

static long double X(std::uint16_t signExponent, std::uint64_t mantissa) {
    long double value = 0;
    std::memcpy(&value, &mantissa, sizeof(mantissa));
    std::memcpy(reinterpret_cast<unsigned char*>(&value) + 8, &signExponent, sizeof(signExponent));
    return value;
}

static bool SameBits(double left, double right) { return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right); }

static bool SameBits(float left, float right) { return std::bit_cast<std::uint32_t>(left) == std::bit_cast<std::uint32_t>(right); }

static bool SameBits(long double left, long double right) { return std::memcmp(&left, &right, 10) == 0; }

static const double kPoly[] = {0.1, 0.7, 1.3, -2.9};
static const float kFPoly[] = {0.1f, 0.7f, 1.3f, -2.9f};
static const long double kLPoly[] = {0.1L, 0.7L, 1.3L, -2.9L};

template <typename T>
static void CheckInt(short (APS5_VABI* function)(T*, short), T input, short scale, short code, T expected) {
    T value = input;
    Require(function(&value, scale) == code && SameBits(value, expected));
}

template <typename T>
static void CheckScale(short (APS5_VABI* function)(T*, std::int64_t), T input, std::int64_t exponent, short code, T expected) {
    T value = input;
    Require(function(&value, exponent) == code && SameBits(value, expected));
}

template <typename T>
static void CheckUnscale(short (APS5_VABI* function)(short*, T*), T input, short code, short exponent, T expected) {
    T value = input;
    short result = 77;
    Require(function(&result, &value) == code && result == exponent && SameBits(value, expected));
}

template <typename T>
static void CheckNorm(short (APS5_VABI* function)(std::uint16_t*), T input, short code, T expected) {
    T value = input;
    Require(function(reinterpret_cast<std::uint16_t*>(&value)) == code && SameBits(value, expected));
}

int main() {
    CheckInt(_Dint_nid_postfix, D(0x4006000000000000ull), 0, -1, D(0x4000000000000000ull));
    CheckInt(_Dint_nid_postfix, D(0xc006000000000000ull), 1, -1, D(0xc004000000000000ull));
    CheckInt(_Dint_nid_postfix, D(0x4008000000000000ull), 0, 0, D(0x4008000000000000ull));
    CheckInt(_Dint_nid_postfix, D(0x3fd0000000000000ull), 0, -1, D(0x0000000000000000ull));
    CheckInt(_Dint_nid_postfix, D(0x8000000000000000ull), 5, 0, D(0x8000000000000000ull));
    CheckInt(_Dint_nid_postfix, D(0x7ff0000000000000ull), 0, 1, D(0x7ff0000000000000ull));
    CheckInt(_Dint_nid_postfix, D(0x7ff8000000000000ull), 0, 2, D(0x7ff8000000000000ull));
    CheckInt(_Dint_nid_postfix, D(0x7e37e43c8800759cull), 0, 0, D(0x7e37e43c8800759cull));
    CheckInt(_Dint_nid_postfix, D(0x0000000000000001ull), 1100, 0, D(0x0000000000000001ull));
    CheckInt(_Dint_nid_postfix, D(0x3ff8000000000000ull), -32000, -1, D(0x0000000000000000ull));
    CheckInt(_Dint_nid_postfix, D(0x3ff8000000000000ull), 32000, 0, D(0x3ff8000000000000ull));
    CheckInt(_Dint_nid_postfix, D(0x405edd2f1a9fbe77ull), 3, -1, D(0x405ed80000000000ull));
    CheckInt(_FDint_nid_postfix, F(0x40300000u), 0, -1, F(0x40000000u));
    CheckInt(_FDint_nid_postfix, F(0xc0300000u), 1, -1, F(0xc0200000u));
    CheckInt(_FDint_nid_postfix, F(0x3e800000u), 0, -1, F(0x00000000u));
    CheckInt(_FDint_nid_postfix, F(0x7f800000u), 0, 1, F(0x7f800000u));
    CheckInt(_FDint_nid_postfix, F(0x3fc00000u), -32700, -1, F(0x00000000u));
    CheckInt(_FDint_nid_postfix, F(0x42f6e979u), 3, -1, F(0x42f6c000u));
    CheckInt(_LDint_nid_postfix, X(0x4000, 0xb000000000000000ull), 0, -1, X(0x4000, 0x8000000000000000ull));
    CheckInt(_LDint_nid_postfix, X(0xc000, 0xb000000000000000ull), 1, -1, X(0xc000, 0xa000000000000000ull));
    CheckInt(_LDint_nid_postfix, X(0x3ffd, 0x8000000000000000ull), 0, -1, X(0x0000, 0x0000000000000000ull));
    CheckInt(_LDint_nid_postfix, X(0x7fff, 0x8000000000000000ull), 0, 1, X(0x7fff, 0x8000000000000000ull));
    CheckInt(_LDint_nid_postfix, X(0x7fff, 0xc000000000000000ull), 0, 2, X(0x7fff, 0xc000000000000000ull));
    CheckInt(_LDint_nid_postfix, X(0x1234, 0x0000000000000000ull), 3, 0, X(0x1234, 0x0000000000000000ull));
    CheckInt(_LDint_nid_postfix, X(0x0000, 0x8e5f1900ace5e5bfull), -16411, 0, X(0x0000, 0x8e5f1900ace5e5bfull));
    CheckInt(_LDint_nid_postfix, X(0x7f96, 0xbe8ad47f0a38a106ull), 16769, -1, X(0x0000, 0x0000000000000000ull));
    CheckInt(_LDint_nid_postfix, X(0x4000, 0x4000000000000001ull), 0, -1, X(0x4000, 0x4000000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0x3ff8000000000000ull), std::int64_t{3}, -1, D(0x4028000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0xbff8000000000000ull), std::int64_t{-1080}, 0, D(0x8000000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0x3ff8000000000000ull), std::int64_t{-1075}, -1, D(0x0000000000000001ull));
    CheckScale(_Dscale_nid_postfix, D(0x7fe1ccf385ebc8a0ull), std::int64_t{10}, 1, D(0x7ff0000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0xffe1ccf385ebc8a0ull), std::int64_t{10}, 1, D(0xfff0000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0x0000000000000001ull), std::int64_t{60}, -1, D(0x0090000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0x0000000000000000ull), std::int64_t{10}, 0, D(0x0000000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0x7ff8000000000000ull), std::int64_t{1}, 2, D(0x7ff8000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0x3ff0000000000000ull), std::int64_t{1} << 40, 1, D(0x7ff0000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0x3ff0000000000000ull), std::int64_t{1} << 31, 1, D(0x7ff0000000000000ull));
    CheckScale(_Dscale_nid_postfix, D(0x3ff0000000000000ull), -(std::int64_t{1} << 32) + 3, 0, D(0x0000000000000000ull));
    CheckScale(_FDscale_nid_postfix, F(0x3fc00000u), std::int64_t{3}, -1, F(0x41400000u));
    CheckScale(_FDscale_nid_postfix, F(0xbfc00000u), std::int64_t{-160}, 0, F(0x80000000u));
    CheckScale(_FDscale_nid_postfix, F(0x7f61b1e6u), std::int64_t{2}, 1, F(0x7f800000u));
    CheckScale(_FDscale_nid_postfix, F(0x00000001u), std::int64_t{30}, -1, F(0x04000000u));
    CheckScale(_LDscale_nid_postfix, X(0x3fff, 0xc000000000000000ull), std::int64_t{3}, -1, X(0x4002, 0xc000000000000000ull));
    CheckScale(_LDscale_nid_postfix, X(0xbfff, 0xc000000000000000ull), std::int64_t{-17000}, 0, X(0x0000, 0x0000000000000000ull));
    CheckScale(_LDscale_nid_postfix, X(0x8000, 0xc388414f03e224f9ull), std::int64_t{-17526}, 0, X(0x0000, 0x0000000000000000ull));
    CheckScale(_LDscale_nid_postfix, X(0x7f94, 0xae9204275937a4c1ull), std::int64_t{100}, -1, X(0x7ff8, 0xae9204275937a4c1ull));
    CheckScale(_LDscale_nid_postfix, X(0x7f94, 0xae9204275937a4c1ull), std::int64_t{200}, 1, X(0x7fff, 0x8000000000000000ull));
    CheckScale(_LDscale_nid_postfix, X(0x4000, 0x4000000000000000ull), std::int64_t{1}, -1, X(0x4000, 0x8000000000000000ull));
    CheckScale(_LDscale_nid_postfix, X(0xec64, 0x0000000000000000ull), std::int64_t{9362}, 0, X(0x8000, 0x0000000000000000ull));
    CheckScale(_LDscale_nid_postfix, X(0x0000, 0x8000000000000000ull), std::int64_t{1}, -1, X(0x0002, 0x8000000000000000ull));
    CheckUnscale(_Dunscale_nid_postfix, D(0x4028000000000000ull), -1, 4, D(0x3fe8000000000000ull));
    CheckUnscale(_Dunscale_nid_postfix, D(0xbfd8000000000000ull), -1, -1, D(0xbfe8000000000000ull));
    CheckUnscale(_Dunscale_nid_postfix, D(0x0000000000000001ull), -1, -1073, D(0x3fe0000000000000ull));
    CheckUnscale(_Dunscale_nid_postfix, D(0x0000000000000000ull), 0, 0, D(0x0000000000000000ull));
    CheckUnscale(_Dunscale_nid_postfix, D(0x7ff0000000000000ull), 1, 0, D(0x7ff0000000000000ull));
    CheckUnscale(_Dunscale_nid_postfix, D(0x7ff8000000000000ull), 2, 0, D(0x7ff8000000000000ull));
    CheckUnscale(_FDunscale_nid_postfix, F(0x41400000u), -1, 4, F(0x3f400000u));
    CheckUnscale(_FDunscale_nid_postfix, F(0x00000001u), -1, -148, F(0x3f000000u));
    CheckUnscale(_FDunscale_nid_postfix, F(0xff800000u), 1, 0, F(0xff800000u));
    CheckUnscale(_LDunscale_nid_postfix, X(0x4002, 0xc000000000000000ull), -1, 4, X(0x3ffe, 0xc000000000000000ull));
    CheckUnscale(_LDunscale_nid_postfix, X(0x4724, 0x4e343e19658fad25ull), -1, 1829, X(0x3ffe, 0x9c687c32cb1f5a4aull));
    CheckUnscale(_LDunscale_nid_postfix, X(0x0000, 0x0000000000000001ull), -1, -16444, X(0x3ffe, 0x8000000000000000ull));
    CheckUnscale(_LDunscale_nid_postfix, X(0x3fff, 0x0000000000000000ull), 0, 0, X(0x3fff, 0x0000000000000000ull));
    CheckNorm(_Dnorm_nid_postfix, D(0x0000000000000001ull), -51, D(0x0000000000000000ull));
    CheckNorm(_Dnorm_nid_postfix, D(0x800fffffffffffffull), 0, D(0x800ffffffffffffeull));
    CheckNorm(_Dnorm_nid_postfix, D(0x3ff0000000000000ull), 1, D(0x0000000000000000ull));
    CheckNorm(_Dnorm_nid_postfix, D(0x8000000000000000ull), 1, D(0x8000000000000000ull));
    CheckNorm(_FDnorm_nid_postfix, F(0x00000001u), -22, F(0x00000000u));
    CheckNorm(_FDnorm_nid_postfix, F(0x80400000u), 0, F(0x80000000u));
    CheckNorm(_LDnorm_nid_postfix, X(0x0000, 0x0000000000000001ull), -63, X(0x0000, 0x8000000000000000ull));
    CheckNorm(_LDnorm_nid_postfix, X(0xfcb6, 0xdf75917a3eb7b900ull), 0, X(0xfcb6, 0xdf75917a3eb7b900ull));
    CheckNorm(_LDnorm_nid_postfix, X(0x1234, 0x0000000100000000ull), -31, X(0x1234, 0x8000000000000000ull));
    Require(_Fpcomp_nid_postfix(1.0, 2.0) == 1 && _Fpcomp_nid_postfix(2.0, 1.0) == 4 && _Fpcomp_nid_postfix(1.0, 1.0) == 2 && _Fpcomp_nid_postfix(NAN, 1.0) == 0);
    Require(_FFpcomp_nid_postfix(-1.0f, 2.0f) == 1 && _FFpcomp_nid_postfix(1.0f, NAN) == 0);
    Require(_LFpcomp_nid_postfix(X(0x4000, 0x4000000000000000ull), 1.0L) == 2 && _LFpcomp_nid_postfix(X(0x7fff, 0x0000000000000000ull), 1.0L) == 2 && _LFpcomp_nid_postfix(X(0x7fff, 0x4000000000000000ull), 1.0L) == 0 && _LFpcomp_nid_postfix(3.0L, 1.0L) == 4);
    Require(_LFpcomp_nid_postfix(X(0x4000, 0x4000000000000000ull), 3.0L) == 2 && _LFpcomp_nid_postfix(3.0L, X(0x4000, 0x4000000000000000ull)) == 2);
    Require(_LFpcomp_nid_postfix(X(0x4001, 0x6000000000000000ull), 1.0L) == 2 && _LFpcomp_nid_postfix(1.0L, X(0x4001, 0x6000000000000000ull)) == 2);
    Require(_LFpcomp_nid_postfix(X(0x4000, 0x4000000000000000ull), X(0x4001, 0x6000000000000000ull)) == 2 && _LFpcomp_nid_postfix(X(0x4001, 0x6000000000000000ull), X(0x4000, 0x4000000000000000ull)) == 2);
    Require(_LFpcomp_nid_postfix(X(0xc000, 0x4000000000000000ull), 3.0L) == 2 && _LFpcomp_nid_postfix(X(0x4000, 0x0000000000000000ull), 1.0L) == 2 && _LFpcomp_nid_postfix(X(0x0000, 0x8000000000000000ull), 1.0L) == 1);
    Require(SameBits(_Poly_nid_postfix(0.3, kPoly, 3), D(0xc0038ded288ce703ull)) && SameBits(_Poly_nid_postfix(0.3, kPoly, 0), 0.1) && SameBits(_Poly_nid_postfix(0.3, kPoly, -1), 0.1));
    Require(SameBits(_FPoly_nid_postfix(0.3f, kFPoly, 3), F(0xc01c6f6au)));
    Require(SameBits(_LPoly_nid_postfix(0.3L, kLPoly, 3), X(0xc000, 0x9c6f694467381d7eull)));
    long double zero = X(0x1234, 0);
    bool threw = false;
    try {
        _LDnorm_nid_postfix(reinterpret_cast<std::uint16_t*>(&zero));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    return 0;
}
