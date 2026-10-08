#include "prx/libc/include/general/VabiMacros.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
extern "C" {
double APS5_VABI atof_nid_postfix(const char*);
float APS5_VABI strtof_nid_postfix(const char*, char**);
long double APS5_VABI strtold_nid_postfix(const char*, char**);
std::int64_t APS5_VABI strtol_nid_postfix(const char*, char**, int);
std::uint64_t APS5_VABI strtoul_nid_postfix(const char*, char**, int);
std::intmax_t APS5_VABI strtoimax_nid_postfix(const char*, char**, int);
int* APS5_VABI __error_nid_postfix();
struct LibcFloatConstant { std::uint32_t bits[4]; };
extern LibcFloatConstant _FInf_nid_postfix;
extern LibcFloatConstant _FNan_nid_postfix;
short APS5_VABI _FDtest_nid_postfix(const float*);
float APS5_VABI fmodf_nid_postfix(float, float);
float APS5_VABI asinf_nid_postfix(float);
float APS5_VABI acosf_nid_postfix(float);
float APS5_VABI atan2f_nid_postfix(float, float);
float APS5_VABI tanf_nid_postfix(float);
float APS5_VABI log10f_nid_postfix(float);
float APS5_VABI logbf_nid_postfix(float);
double APS5_VABI exp2_nid_postfix(double);
double APS5_VABI ldexp_nid_postfix(double, int);
double APS5_VABI scalbn_nid_postfix(double, int);
float APS5_VABI scalbnf_nid_postfix(float, int);
double APS5_VABI frexp_nid_postfix(double, int*);
float APS5_VABI frexpf_nid_postfix(float, int*);
std::int64_t APS5_VABI lround_nid_postfix(double);
std::div_t APS5_VABI div_nid_postfix(int, int);
std::int64_t APS5_VABI lroundf_nid_postfix(float);
std::int64_t APS5_VABI llround_nid_postfix(double);
int APS5_VABI __isfinitef_nid_postfix(float);
int APS5_VABI __isnormal_nid_postfix(double);
int APS5_VABI __isnormalf_nid_postfix(float);
int APS5_VABI __isinff_nid_postfix(float);
std::lldiv_t APS5_VABI lldiv_nid_postfix(long long, long long);
double APS5_VABI sinh_nid_postfix(double);
float APS5_VABI sinhf_nid_postfix(float);
double APS5_VABI cosh_nid_postfix(double);
float APS5_VABI coshf_nid_postfix(float);
double APS5_VABI asinh_nid_postfix(double);
float APS5_VABI asinhf_nid_postfix(float);
double APS5_VABI acosh_nid_postfix(double);
float APS5_VABI acoshf_nid_postfix(float);
double APS5_VABI atanh_nid_postfix(double);
float APS5_VABI atanhf_nid_postfix(float);
double APS5_VABI expm1_nid_postfix(double);
float APS5_VABI expm1f_nid_postfix(float);
double APS5_VABI log1p_nid_postfix(double);
float APS5_VABI log1pf_nid_postfix(float);
double APS5_VABI logb_nid_postfix(double);
double APS5_VABI erf_nid_postfix(double);
float APS5_VABI erff_nid_postfix(float);
double APS5_VABI erfc_nid_postfix(double);
float APS5_VABI erfcf_nid_postfix(float);
double APS5_VABI tgamma_nid_postfix(double);
float APS5_VABI tgammaf_nid_postfix(float);
double APS5_VABI sqrt_nid_postfix(double);
float APS5_VABI sqrtf_nid_postfix(float);
double APS5_VABI fabs_nid_postfix(double);
float APS5_VABI fabsf_nid_postfix(float);
double APS5_VABI copysign_nid_postfix(double, double);
float APS5_VABI copysignf_nid_postfix(float, float);
double APS5_VABI ceil_nid_postfix(double);
float APS5_VABI ceilf_nid_postfix(float);
double APS5_VABI floor_nid_postfix(double);
float APS5_VABI floorf_nid_postfix(float);
double APS5_VABI trunc_nid_postfix(double);
float APS5_VABI truncf_nid_postfix(float);
double APS5_VABI rint_nid_postfix(double);
float APS5_VABI rintf_nid_postfix(float);
double APS5_VABI nearbyint_nid_postfix(double);
float APS5_VABI nearbyintf_nid_postfix(float);
std::int64_t APS5_VABI lrint_nid_postfix(double);
std::int64_t APS5_VABI lrintf_nid_postfix(float);
std::int64_t APS5_VABI llrint_nid_postfix(double);
std::int64_t APS5_VABI llrintf_nid_postfix(float);
std::int64_t APS5_VABI llroundf_nid_postfix(float);
double APS5_VABI remainder_nid_postfix(double, double);
double APS5_VABI fdim_nid_postfix(double, double);
float APS5_VABI fdimf_nid_postfix(float, float);
double APS5_VABI fmax_nid_postfix(double, double);
float APS5_VABI fmaxf_nid_postfix(float, float);
double APS5_VABI fmin_nid_postfix(double, double);
float APS5_VABI fminf_nid_postfix(float, float);
double APS5_VABI fma_nid_postfix(double, double, double);
float APS5_VABI fmaf_nid_postfix(float, float, float);
double APS5_VABI nextafter_nid_postfix(double, double);
float APS5_VABI nextafterf_nid_postfix(float, float);
double APS5_VABI scalbln_nid_postfix(double, std::int64_t);
double APS5_VABI nan_nid_postfix(const char*);
float APS5_VABI nanf_nid_postfix(const char*);
}
static void Require(bool value) { if (!value) std::abort(); }

static void CheckIntegerConversions() {
    for (const long long numerator : {4294967301LL, -4294967301LL}) {
        for (const long long denominator : {3LL, -3LL}) {
            const auto result = lldiv_nid_postfix(numerator, denominator);
            Require(result.quot == numerator / denominator && result.rem == numerator % denominator);
            Require(result.quot * denominator + result.rem == numerator);
        }
    }
    struct SignedCase {
        const char* text;
        int base;
        std::int64_t value;
        std::size_t consumed;
        int error;
    };
    const SignedCase signedCases[] = {
        {"-42tail", 10, -42, 3, 0},
        {"2147483648!", 10, INT64_C(2147483648), 10, 0},
        {"-2147483649!", 10, -INT64_C(2147483649), 11, 0},
        {"4294967296!", 10, INT64_C(4294967296), 10, 0},
        {"9223372036854775807!", 10, INT64_MAX, 19, 0},
        {"-9223372036854775808!", 10, INT64_MIN, 20, 0},
        {"9223372036854775808!", 10, INT64_MAX, 19, 34},
        {"-9223372036854775809!", 10, INT64_MIN, 20, 34},
        {"18446744073709551616000!", 10, INT64_MAX, 23, 34},
        {" \t+0x100000000z", 0, INT64_C(4294967296), 14, 0},
        {"-0x8000000000000000!", 0, INT64_MIN, 19, 0},
        {"0x8000000000000000!", 16, INT64_MAX, 18, 34},
        {"0100000000000!", 0, INT64_C(8589934592), 13, 0},
        {"100000000000000000000000000000000!", 2, INT64_C(4294967296), 33, 0},
        {"z!", 36, 35, 1, 0},
        {"", 10, 0, 0, 0},
        {" \t+!", 10, 0, 0, 0},
        {"123!", 10, 123, 3, 0}
    };
    for (const auto& test : signedCases) {
        char* end = nullptr;
        *__error_nid_postfix() = 0;
        const auto value = strtol_nid_postfix(test.text, &end, test.base);
        if (value != test.value || end != test.text + test.consumed || *__error_nid_postfix() != test.error) {
            std::fprintf(stderr, "Guest strtol failed for '%s' in base %d\n", test.text, test.base);
            std::abort();
        }
        *__error_nid_postfix() = 0;
        const auto maxValue = strtoimax_nid_postfix(test.text, &end, test.base);
        if (maxValue != test.value || end != test.text + test.consumed || *__error_nid_postfix() != test.error) {
            std::fprintf(stderr, "Guest strtoimax failed for '%s' in base %d\n", test.text, test.base);
            std::abort();
        }
    }
    struct UnsignedCase {
        const char* text;
        int base;
        std::uint64_t value;
        std::size_t consumed;
        int error;
    };
    const UnsignedCase unsignedCases[] = {
        {"4294967296!", 10, UINT64_C(4294967296), 10, 0},
        {"9223372036854775808!", 10, UINT64_C(9223372036854775808), 19, 0},
        {"18446744073709551615!", 10, UINT64_MAX, 20, 0},
        {"18446744073709551616!", 10, UINT64_MAX, 20, 34},
        {"18446744073709551616000!", 10, UINT64_MAX, 23, 34},
        {"-1!", 10, UINT64_MAX, 2, 0},
        {"-4294967296!", 10, UINT64_MAX - UINT64_C(4294967295), 11, 0},
        {"-18446744073709551615!", 10, 1, 21, 0},
        {"-18446744073709551616!", 10, UINT64_MAX, 21, 34},
        {" \t+0xffffffffffffffffz", 0, UINT64_MAX, 21, 0},
        {"0x10000000000000000!", 16, UINT64_MAX, 19, 34},
        {"0100000000000!", 0, UINT64_C(8589934592), 13, 0},
        {"100000000000000000000000000000000!", 2, UINT64_C(4294967296), 33, 0},
        {"z!", 36, 35, 1, 0},
        {"", 10, 0, 0, 0},
        {" \t-!", 10, 0, 0, 0},
        {"123!", 10, 123, 3, 0}
    };
    for (const auto& test : unsignedCases) {
        char* end = nullptr;
        *__error_nid_postfix() = 0;
        const auto value = strtoul_nid_postfix(test.text, &end, test.base);
        if (value != test.value || end != test.text + test.consumed || *__error_nid_postfix() != test.error) {
            std::fprintf(stderr, "Guest strtoul failed for '%s' in base %d\n", test.text, test.base);
            std::abort();
        }
    }
    *__error_nid_postfix() = 13;
    Require(strtol_nid_postfix("-4294967296", nullptr, 10) == -INT64_C(4294967296));
    Require(*__error_nid_postfix() == 13);
    Require(strtoul_nid_postfix("4294967296", nullptr, 10) == UINT64_C(4294967296));
    Require(*__error_nid_postfix() == 13);
    *__error_nid_postfix() = 0;
}

static void CheckFloatClassification() {
    Require(_FInf_nid_postfix.bits[0] == 0x7f800000u && _FNan_nid_postfix.bits[0] == 0x7fc00000u);
    for (int word = 1; word < 4; ++word) Require(_FInf_nid_postfix.bits[word] == 0 && _FNan_nid_postfix.bits[word] == 0);
    const struct { std::uint32_t bits; short code; } cases[] = {
        {0x00000000u, 0}, {0x80000000u, 0}, {0x00000001u, -2}, {0x807fffffu, -2}, {0x00800000u, -1},
        {0xbf800000u, -1}, {0x7f7fffffu, -1}, {0x7f800000u, 1}, {0xff800000u, 1}, {0x7f800001u, 2},
        {0x7fc00000u, 2}, {0xff810000u, 2}, {0x7f810000u, 2},
    };
    for (const auto& test : cases) {
        float value;
        std::memcpy(&value, &test.bits, sizeof(value));
        if (_FDtest_nid_postfix(&value) != test.code) {
            std::fprintf(stderr, "Guest _FDtest failed for %08x\n", test.bits);
            std::abort();
        }
    }
    Require(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FInf_nid_postfix)) == 1);
    Require(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FNan_nid_postfix)) == 2);
}

static bool Near(double value, double expected, double tolerance) {
    return std::abs(value - expected) <= tolerance * std::abs(expected);
}

static bool SameBits(double lhs, double rhs) {
    std::uint64_t left;
    std::uint64_t right;
    std::memcpy(&left, &lhs, sizeof(left));
    std::memcpy(&right, &rhs, sizeof(right));
    return left == right;
}

static void CheckC99Math() {
    const auto infinity = std::numeric_limits<double>::infinity();
    Require(SameBits(sinh_nid_postfix(-0.), -0.) && Near(sinh_nid_postfix(1.), 1.1752011936438014, 1e-15));
    Require(Near(sinhf_nid_postfix(-2.f), -3.6268604f, 1e-6));
    Require(cosh_nid_postfix(0.) == 1. && Near(cosh_nid_postfix(-1.), 1.5430806348152437, 1e-15));
    Require(Near(coshf_nid_postfix(2.f), 3.7621957f, 1e-6) && coshf_nid_postfix(1000.f) == std::numeric_limits<float>::infinity());
    Require(Near(asinh_nid_postfix(1.), 0.881373587019543, 1e-15) && Near(asinhf_nid_postfix(-1.f), -0.8813736f, 1e-6));
    Require(acosh_nid_postfix(1.) == 0. && Near(acosh_nid_postfix(2.), 1.3169578969248166, 1e-15));
    Require(Near(acoshf_nid_postfix(2.f), 1.3169579f, 1e-6) && std::isnan(acosh_nid_postfix(0.5)));
    Require(Near(atanh_nid_postfix(0.5), 0.5493061443340549, 1e-15) && atanh_nid_postfix(1.) == infinity);
    Require(Near(atanhf_nid_postfix(-0.5f), -0.54930615f, 1e-6));
    Require(Near(expm1_nid_postfix(1e-10), 1.00000000005e-10, 1e-15) && expm1_nid_postfix(-infinity) == -1.);
    Require(Near(expm1f_nid_postfix(1.f), 1.7182817f, 1e-6));
    Require(Near(log1p_nid_postfix(1e-10), 9.9999999995e-11, 1e-15) && log1p_nid_postfix(-1.) == -infinity);
    Require(Near(log1pf_nid_postfix(1.f), 0.6931472f, 1e-6) && std::isnan(log1p_nid_postfix(-2.)));
    Require(logb_nid_postfix(8.) == 3. && logb_nid_postfix(-0.75) == -1. && logb_nid_postfix(0.) == -infinity);
    Require(logb_nid_postfix(std::numeric_limits<double>::denorm_min()) == -1074.);
    Require(erf_nid_postfix(0.) == 0. && Near(erf_nid_postfix(1.), 0.8427007929497149, 1e-15) && erf_nid_postfix(infinity) == 1.);
    Require(Near(erff_nid_postfix(-1.f), -0.8427008f, 1e-6));
    Require(erfc_nid_postfix(0.) == 1. && Near(erfc_nid_postfix(2.), 0.004677734981047266, 1e-14));
    Require(Near(erfcf_nid_postfix(1.f), 0.15729921f, 1e-6));
    Require(Near(tgamma_nid_postfix(5.), 24., 1e-15) && Near(tgamma_nid_postfix(0.5), 1.7724538509055159, 1e-15));
    Require(Near(tgammaf_nid_postfix(4.f), 6.f, 1e-6) && std::isnan(tgamma_nid_postfix(-1.)));
    Require(sqrt_nid_postfix(2.) == 1.4142135623730951 && sqrtf_nid_postfix(2.f) == 1.41421356f);
    Require(SameBits(sqrt_nid_postfix(-0.), -0.) && std::isnan(sqrt_nid_postfix(-1.)) && std::isnan(sqrtf_nid_postfix(-1.f)));
    Require(fabs_nid_postfix(-3.5) == 3.5 && SameBits(fabs_nid_postfix(-0.), 0.) && fabsf_nid_postfix(-infinity) == infinity);
    Require(copysign_nid_postfix(3., -0.) == -3. && copysignf_nid_postfix(-2.f, 1.f) == 2.f);
    Require(ceil_nid_postfix(-1.5) == -1. && SameBits(ceil_nid_postfix(-0.5), -0.) && ceil_nid_postfix(4503599627370495.5) == 4503599627370496.);
    Require(ceilf_nid_postfix(1.25f) == 2.f && floorf_nid_postfix(-1.25f) == -2.f);
    Require(floor_nid_postfix(-1.5) == -2. && floor_nid_postfix(1.9999999999999998) == 1.);
    Require(trunc_nid_postfix(-1.75) == -1. && SameBits(trunc_nid_postfix(-0.25), -0.) && truncf_nid_postfix(2.75f) == 2.f);
    Require(rint_nid_postfix(2.5) == 2. && rint_nid_postfix(3.5) == 4. && rint_nid_postfix(-2.5) == -2.);
    Require(rintf_nid_postfix(0.5f) == 0.f && nearbyint_nid_postfix(-3.5) == -4. && nearbyintf_nid_postfix(1.5f) == 2.f);
    Require(lrint_nid_postfix(4294967296.5) == INT64_C(4294967296) && lrintf_nid_postfix(-2.5f) == -2);
    Require(llrint_nid_postfix(-4294967297.5) == -INT64_C(4294967298) && llrintf_nid_postfix(3.5f) == 4);
    Require(llroundf_nid_postfix(-2.5f) == -3 && llroundf_nid_postfix(8589934592.f) == INT64_C(8589934592));
    Require(remainder_nid_postfix(5., 2.) == 1. && remainder_nid_postfix(7., 2.) == -1. && std::isnan(remainder_nid_postfix(1., 0.)));
    Require(fdim_nid_postfix(5., 3.) == 2. && fdim_nid_postfix(3., 5.) == 0. && fdimf_nid_postfix(1.f, -1.f) == 2.f);
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    Require(fmax_nid_postfix(nan, 1.) == 1. && fmax_nid_postfix(-1., 2.) == 2. && fmaxf_nid_postfix(1.f, std::numeric_limits<float>::quiet_NaN()) == 1.f);
    Require(fmin_nid_postfix(1., nan) == 1. && fmin_nid_postfix(-1., 2.) == -1. && fminf_nid_postfix(3.f, -3.f) == -3.f);
    const double epsilon = std::ldexp(1., -52);
    Require(fma_nid_postfix(1. + epsilon, 1. - epsilon, -1.) == -std::ldexp(1., -104));
    const float epsilonF = std::ldexp(1.f, -23);
    Require(fmaf_nid_postfix(1.f + epsilonF, 1.f - epsilonF, -1.f) == -std::ldexp(1.f, -46));
    Require(nextafter_nid_postfix(1., 2.) == 1. + epsilon && nextafter_nid_postfix(0., -1.) == -std::numeric_limits<double>::denorm_min());
    Require(nextafterf_nid_postfix(1.f, 0.f) == 1.f - std::ldexp(1.f, -24));
    Require(scalbln_nid_postfix(0.75, 4) == 12. && scalbln_nid_postfix(1., INT64_C(4294967296)) == infinity);
    Require(scalbln_nid_postfix(1., -INT64_C(4294967296)) == 0. && scalbln_nid_postfix(0.5, -1074) == 0.);
    const auto quiet = nan_nid_postfix("");
    std::uint64_t bits;
    std::memcpy(&bits, &quiet, sizeof(bits));
    Require(std::isnan(quiet) && (bits & 0x0008000000000000ull) != 0 && !std::signbit(quiet));
    Require(std::isnan(nanf_nid_postfix("")) && !std::signbit(nanf_nid_postfix("")));
}

int main() {
    CheckFloatClassification();
    CheckC99Math();
    CheckIntegerConversions();
    Require(atof_nid_postfix(" -12.5tail") == -12.5);
    char* end = nullptr;
    const char input[] = "0x1.8p+2 remainder";
    Require(strtof_nid_postfix(input, &end) == 6.f && end == input + 8);
    const char invalid[] = "invalid";
    Require(strtof_nid_postfix(invalid, &end) == 0.f && end == invalid);
    *__error_nid_postfix() = 0;
    Require(std::isinf(strtof_nid_postfix("1e1000", nullptr)));
    Require(*__error_nid_postfix() == 34);
    Require(strtold_nid_postfix("1.0000000000000000001!", &end) > 1.L && *end == '!');
    Require(fmodf_nid_postfix(5.5f, 2.f) == 1.5f);
    Require(fmodf_nid_postfix(-5.5f, 2.f) == -1.5f);
    Require(std::signbit(fmodf_nid_postfix(-4.f, 2.f)));
    Require(std::isnan(fmodf_nid_postfix(1.f, 0.f)));
    Require(std::abs(asinf_nid_postfix(0.5f) - 0.5235988f) < 0.000001f);
    Require(std::abs(acosf_nid_postfix(0.5f) - 1.0471976f) < 0.000001f);
    Require(std::abs(atan2f_nid_postfix(1.f, -1.f) - 2.3561945f) < 0.000001f);
    Require(tanf_nid_postfix(0.f) == 0.f);
    Require(log10f_nid_postfix(100.f) == 2.f);
    Require(logbf_nid_postfix(8.f) == 3.f && logbf_nid_postfix(-0.75f) == -1.f);
    Require(logbf_nid_postfix(std::numeric_limits<float>::denorm_min()) == -149.f);
    Require(logbf_nid_postfix(0.f) == -std::numeric_limits<float>::infinity());
    Require(logbf_nid_postfix(-std::numeric_limits<float>::infinity()) == std::numeric_limits<float>::infinity());
    Require(std::isnan(logbf_nid_postfix(std::numeric_limits<float>::quiet_NaN())));
    Require(exp2_nid_postfix(-3.) == 0.125);
    Require(ldexp_nid_postfix(0.75, 4) == 12.);
    Require(scalbn_nid_postfix(0.75, -2) == 0.1875);
    Require(scalbnf_nid_postfix(0.75f, 4) == 12.f);
    int exponent = 0;
    Require(frexp_nid_postfix(12., &exponent) == 0.75 && exponent == 4);
    Require(frexpf_nid_postfix(-12.f, &exponent) == -0.75f && exponent == 4);
    Require(lround_nid_postfix(4294967296.5) == INT64_C(4294967297));
    Require(lround_nid_postfix(-2.5) == -3);
    Require(lroundf_nid_postfix(4294967296.f) == INT64_C(4294967296));
    Require(lroundf_nid_postfix(2.5f) == 3);
    Require(llround_nid_postfix(-4294967296.5) == -INT64_C(4294967297));
    const auto infinity = std::numeric_limits<float>::infinity();
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    Require(__isinff_nid_postfix(infinity) == 1 && __isinff_nid_postfix(-infinity) == 1);
    Require(__isinff_nid_postfix(nan) == 0 && __isinff_nid_postfix(1.f) == 0);
    Require(__isfinitef_nid_postfix(0.f) == 1 && __isfinitef_nid_postfix(infinity) == 0);
    Require(__isfinitef_nid_postfix(nan) == 0);
    Require(__isnormalf_nid_postfix(1.f) == 1 && __isnormalf_nid_postfix(0.f) == 0);
    Require(__isnormalf_nid_postfix(std::numeric_limits<float>::denorm_min()) == 0);
    Require(__isnormal_nid_postfix(1.) == 1 && __isnormal_nid_postfix(0.) == 0);
    Require(__isnormal_nid_postfix(std::numeric_limits<double>::denorm_min()) == 0);
    const auto quotient = div_nid_postfix(7, 2);
    Require(quotient.quot == 3 && quotient.rem == 1);
    const auto negativeNumerator = div_nid_postfix(-7, 2);
    Require(negativeNumerator.quot == -3 && negativeNumerator.rem == -1);
    const auto negativeDenominator = div_nid_postfix(7, -2);
    Require(negativeDenominator.quot == -3 && negativeDenominator.rem == 1);
    const auto minimum = div_nid_postfix(std::numeric_limits<int>::min(), 10);
    Require(minimum.quot == -214748364 && minimum.rem == -8);
}
