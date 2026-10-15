#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
extern "C" {
double APS5_VABI atof_nid_postfix(const char*);
double APS5_VABI strtod_nid_postfix(const char*, char**);
double APS5_VABI wcstod_nid_postfix(const char16_t*, char16_t**);
float APS5_VABI wcstof_nid_postfix(const char16_t*, char16_t**);
long double APS5_VABI wcstold_nid_postfix(const char16_t*, char16_t**);
float APS5_VABI strtof_nid_postfix(const char*, char**);
long double APS5_VABI strtold_nid_postfix(const char*, char**);
std::int64_t APS5_VABI strtol_nid_postfix(const char*, char**, int);
std::uint64_t APS5_VABI strtoul_nid_postfix(const char*, char**, int);
std::intmax_t APS5_VABI strtoimax_nid_postfix(const char*, char**, int);
long long APS5_VABI strtoll_nid_postfix(const char*, char**, int);
unsigned long long APS5_VABI strtoull_nid_postfix(const char*, char**, int);
std::uintmax_t APS5_VABI strtoumax_nid_postfix(const char*, char**, int);
unsigned long long APS5_VABI _Stoull_nid_postfix(const char*, char**, int);
std::uint64_t APS5_VABI _Stoul_nid_postfix(const char*, char**, int);
std::int64_t APS5_VABI atol_nid_postfix(const char*);
long long APS5_VABI atoll_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
double APS5_VABI sqrt_nid_postfix(double);
float APS5_VABI sqrtf_nid_postfix(float);
double APS5_VABI fabs_nid_postfix(double);
int APS5_VABI abs_nid_postfix(int);
double APS5_VABI trunc_nid_postfix(double);
double APS5_VABI ceil_nid_postfix(double);
double APS5_VABI nearbyint_nid_postfix(double);
double APS5_VABI fma_nid_postfix(double, double, double);
long double APS5_VABI acosl_nid_postfix(long double);
long double APS5_VABI asinl_nid_postfix(long double);
long double APS5_VABI frexpl_nid_postfix(long double, int*);
struct LibcFloatConstant { std::uint32_t bits[4]; };
extern LibcFloatConstant _FInf_nid_postfix;
extern LibcFloatConstant _FNan_nid_postfix;
short APS5_VABI _FDtest_nid_postfix(const float*);
int APS5_VABI __fpclassifyf_nid_postfix(float);
int APS5_VABI isnanf_nid_postfix(float);
int APS5_VABI __signbitl_nid_postfix(long double);
float APS5_VABI fmodf_nid_postfix(float, float);
double APS5_VABI fmod_nid_postfix(double, double);
float APS5_VABI asinf_nid_postfix(float);
float APS5_VABI acosf_nid_postfix(float);
double APS5_VABI asin_nid_postfix(double);
float APS5_VABI atan2f_nid_postfix(float, float);
float APS5_VABI hypotf_nid_postfix(float, float);
double APS5_VABI hypot_nid_postfix(double, double);
float APS5_VABI fabsf_nid_postfix(float);
long double APS5_VABI fabsl_nid_postfix(long double);
float APS5_VABI tanf_nid_postfix(float);
int APS5_VABI rand_nid_postfix();
void APS5_VABI srand_nid_postfix(unsigned int);
int APS5_VABI rand_r_nid_postfix(unsigned int*);
float APS5_VABI log10f_nid_postfix(float);
double APS5_VABI log1p_nid_postfix(double);
float APS5_VABI log1pf_nid_postfix(float);
double APS5_VABI expm1_nid_postfix(double);
float APS5_VABI expm1f_nid_postfix(float);
float APS5_VABI erff_nid_postfix(float);
double APS5_VABI lgamma_nid_postfix(double);
float APS5_VABI logbf_nid_postfix(float);
double APS5_VABI exp2_nid_postfix(double);
float APS5_VABI __powisf2_nid_postfix(float, int);
double APS5_VABI ldexp_nid_postfix(double, int);
double APS5_VABI scalbn_nid_postfix(double, int);
float APS5_VABI scalbnf_nid_postfix(float, int);
float APS5_VABI nextafterf_nid_postfix(float, float);
double APS5_VABI frexp_nid_postfix(double, int*);
float APS5_VABI frexpf_nid_postfix(float, int*);
std::int64_t APS5_VABI lround_nid_postfix(double);
std::div_t APS5_VABI div_nid_postfix(int, int);
double APS5_VABI sinh_nid_postfix(double);
double APS5_VABI cosh_nid_postfix(double);
float APS5_VABI coshf_nid_postfix(float);
float APS5_VABI sinhf_nid_postfix(float);
double APS5_VABI asinh_nid_postfix(double);
float APS5_VABI asinhf_nid_postfix(float);
double APS5_VABI acosh_nid_postfix(double);
float APS5_VABI acoshf_nid_postfix(float);
double APS5_VABI atanh_nid_postfix(double);
std::int64_t APS5_VABI lroundf_nid_postfix(float);
std::int64_t APS5_VABI llround_nid_postfix(double);
std::int64_t APS5_VABI llroundf_nid_postfix(float);
std::int64_t APS5_VABI llroundl_nid_postfix(long double);
int APS5_VABI __isfinitef_nid_postfix(float);
int APS5_VABI __isnormal_nid_postfix(double);
int APS5_VABI __isnormalf_nid_postfix(float);
int APS5_VABI __isinff_nid_postfix(float);
int APS5_VABI __isinf_nid_postfix(double);
std::lldiv_t APS5_VABI lldiv_nid_postfix(long long, long long);
std::lldiv_t APS5_VABI ldiv_nid_postfix(std::int64_t, std::int64_t);
float APS5_VABI nanf_nid_postfix(const char*);
double APS5_VABI nan_nid_postfix(const char*);
long double APS5_VABI atanl_nid_postfix(long double);
long double APS5_VABI sinl_nid_postfix(long double);
long double APS5_VABI cosl_nid_postfix(long double);
long double APS5_VABI tanl_nid_postfix(long double);
long double APS5_VABI sinhl_nid_postfix(long double);
long double APS5_VABI coshl_nid_postfix(long double);
long double APS5_VABI tanhl_nid_postfix(long double);
long double APS5_VABI expl_nid_postfix(long double);
long double APS5_VABI exp2l_nid_postfix(long double);
long double APS5_VABI logl_nid_postfix(long double);
long double APS5_VABI log10l_nid_postfix(long double);
long double APS5_VABI powl_nid_postfix(long double, long double);
long double APS5_VABI ldexpl_nid_postfix(long double, int);
long double APS5_VABI truncl_nid_postfix(long double);
long double APS5_VABI nearbyintl_nid_postfix(long double);
double APS5_VABI logb_nid_postfix(double);
double APS5_VABI remainder_nid_postfix(double, double);
}
static void Require(bool value) { if (!value) std::abort(); }

static void CheckSquareRoot() {
    Require(sqrt_nid_postfix(4.) == 2. && sqrtf_nid_postfix(6.25f) == 2.5f);
    Require(std::bit_cast<std::uint64_t>(sqrt_nid_postfix(2.)) == 0x3ff6a09e667f3bcdull);
    Require(std::bit_cast<std::uint32_t>(sqrtf_nid_postfix(2.f)) == 0x3fb504f3u);
    Require(std::bit_cast<std::uint64_t>(sqrt_nid_postfix(0x1p-1074)) == 0x1e60000000000000ull);
    Require(std::bit_cast<std::uint64_t>(sqrt_nid_postfix(-0.)) == 0x8000000000000000ull);
    Require(std::bit_cast<std::uint32_t>(sqrtf_nid_postfix(-0.f)) == 0x80000000u);
    Require(sqrt_nid_postfix(INFINITY) == INFINITY && sqrtf_nid_postfix(INFINITY) == INFINITY);
    Require(std::isnan(sqrt_nid_postfix(-1.)) && std::isnan(sqrtf_nid_postfix(-1.f)));
}

static void CheckExactOperations() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Require(fabs_nid_postfix(-3.5) == 3.5 && fabs_nid_postfix(-INFINITY) == INFINITY);
    Require(std::bit_cast<std::uint64_t>(fabs_nid_postfix(-0.)) == 0);
    Require(std::isnan(fabs_nid_postfix(-nan)) && !std::signbit(fabs_nid_postfix(-nan)));
    Require(fabsf_nid_postfix(-3.5f) == 3.5f && fabsf_nid_postfix(-INFINITY) == INFINITY);
    Require(std::bit_cast<std::uint32_t>(fabsf_nid_postfix(-0.f)) == 0);
    Require(std::isnan(fabsf_nid_postfix(-static_cast<float>(nan))) && !std::signbit(fabsf_nid_postfix(-static_cast<float>(nan))));
    Require(fabsl_nid_postfix(-3.5L) == 3.5L && fabsl_nid_postfix(-INFINITY) == INFINITY);
    Require(std::isnan(fabsl_nid_postfix(-static_cast<long double>(nan))) && !std::signbit(fabsl_nid_postfix(-static_cast<long double>(nan))));
    Require(trunc_nid_postfix(2.7) == 2. && trunc_nid_postfix(-2.7) == -2.);
    Require(std::bit_cast<std::uint64_t>(trunc_nid_postfix(-0.5)) == 0x8000000000000000ull);
    Require(trunc_nid_postfix(4503599627370497.) == 4503599627370497. && trunc_nid_postfix(-1e300) == -1e300);
    Require(trunc_nid_postfix(-INFINITY) == -INFINITY && std::isnan(trunc_nid_postfix(nan)));
    Require(ceil_nid_postfix(2.2) == 3. && ceil_nid_postfix(-2.7) == -2. && ceil_nid_postfix(5.) == 5.);
    Require(std::bit_cast<std::uint64_t>(ceil_nid_postfix(-0.5)) == 0x8000000000000000ull);
    Require(std::bit_cast<std::uint64_t>(ceil_nid_postfix(-0.)) == 0x8000000000000000ull && ceil_nid_postfix(0x1p-1074) == 1.);
    Require(ceil_nid_postfix(4503599627370495.5) == 4503599627370496. && ceil_nid_postfix(-1e300) == -1e300);
    Require(ceil_nid_postfix(INFINITY) == INFINITY && ceil_nid_postfix(-INFINITY) == -INFINITY && std::isnan(ceil_nid_postfix(nan)));
    Require(std::fesetround(FE_DOWNWARD) == 0);
    const bool ceilIgnoresRounding = ceil_nid_postfix(2.2) == 3. && ceil_nid_postfix(-2.7) == -2.;
    Require(std::fesetround(FE_TONEAREST) == 0 && ceilIgnoresRounding);
    Require(fma_nid_postfix(2., 3., 4.) == 10.);
    volatile double tenth = 0.1;
    Require(fma_nid_postfix(tenth, 10., -1.) == 0x1p-54);
    Require(fma_nid_postfix(0x1p1023, 2., -0x1p1023) == 0x1p1023);
    Require(std::isnan(fma_nid_postfix(INFINITY, 0., 1.)));
}

static void CheckNearbyint() {
    std::feclearexcept(FE_ALL_EXCEPT);
    const double zero = nearbyint_nid_postfix(0.5);
    Require(zero == 0.0 && !std::signbit(zero));
    Require(nearbyint_nid_postfix(1.5) == 2.0 && nearbyint_nid_postfix(2.5) == 2.0);
    Require(nearbyint_nid_postfix(2.7) == 3.0 && nearbyint_nid_postfix(-2.7) == -3.0);
    Require(std::bit_cast<std::uint64_t>(nearbyint_nid_postfix(-0.5)) == 0x8000000000000000ull);
    Require(nearbyint_nid_postfix(0x1p53) == 0x1p53);
    Require(nearbyint_nid_postfix(INFINITY) == INFINITY && nearbyint_nid_postfix(-INFINITY) == -INFINITY);
    Require(std::isnan(nearbyint_nid_postfix(NAN)));
    Require(std::fetestexcept(FE_INEXACT) == 0);
}

static bool ExtendedBits(long double value, std::uint64_t mantissa, std::uint16_t signExponent) {
    unsigned char bytes[sizeof(long double)]{};
    std::memcpy(bytes, &value, sizeof(value));
    std::uint64_t actualMantissa = 0;
    std::uint16_t actualSignExponent = 0;
    std::memcpy(&actualMantissa, bytes, 8);
    std::memcpy(&actualSignExponent, bytes + 8, 2);
    return actualMantissa == mantissa && actualSignExponent == signExponent;
}

static void CheckHypotSpecialValue() {
    Require(std::bit_cast<std::uint64_t>(hypot_nid_postfix(INFINITY, std::bit_cast<double>(0x7ff0000000000001ull))) == 0x7ff0000000000000ull);
    Require(std::bit_cast<std::uint64_t>(hypot_nid_postfix(std::bit_cast<double>(0x7ff0000000000001ull), INFINITY)) == 0x7ff0000000000000ull);
    Require(std::bit_cast<std::uint64_t>(hypot_nid_postfix(-INFINITY, std::bit_cast<double>(0x7ff0000000000001ull))) == 0x7ff0000000000000ull);
    Require(std::bit_cast<std::uint64_t>(hypot_nid_postfix(std::bit_cast<double>(0x7ff0000000000001ull), -INFINITY)) == 0x7ff0000000000000ull);
}

static void CheckAsinfErrno() {
    errno = 0;
    (void)asinf_nid_postfix(2.0f);
    Require(errno == 0);
}

static void CheckExp2Errno() {
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    double result = exp2_nid_postfix(1024.0);
    Require(errno == 0);
    Require(std::isinf(result) && result > 0);
    Require(std::fetestexcept(FE_OVERFLOW | FE_INEXACT) == (FE_OVERFLOW | FE_INEXACT));
}

static void CheckLongDouble() {
    Require(ExtendedBits(acosl_nid_postfix(-1.0L), 0xc90fdaa22168c235ull, 0x4000));
    Require(ExtendedBits(acosl_nid_postfix(0.0L), 0xc90fdaa22168c235ull, 0x3fff));
    Require(ExtendedBits(acosl_nid_postfix(1.0L), 0, 0));
    Require(std::isnan(acosl_nid_postfix(1.5L)) && std::isnan(acosl_nid_postfix(-2.0L)));

    int exponent = 77;
    Require(frexpl_nid_postfix(8.0L, &exponent) == 0.5L && exponent == 4);
    Require(frexpl_nid_postfix(-3.0L, &exponent) == -0.75L && exponent == 2);
    Require(frexpl_nid_postfix(0.375L, &exponent) == 0.75L && exponent == -1);
    long double smallest = 0;
    const std::uint64_t one = 1;
    std::memcpy(&smallest, &one, sizeof(one));
    Require(ExtendedBits(frexpl_nid_postfix(smallest, &exponent), 0x8000000000000000ull, 0x3ffe) && exponent == -16444);
    exponent = 77;
    Require(ExtendedBits(frexpl_nid_postfix(-0.0L, &exponent), 0, 0x8000) && exponent == 0);
    exponent = 77;
    Require(frexpl_nid_postfix(INFINITY, &exponent) == INFINITY && exponent == 77);
    Require(frexpl_nid_postfix(-INFINITY, &exponent) == -INFINITY && exponent == 77);
    Require(std::isnan(frexpl_nid_postfix(NAN, &exponent)) && exponent == 77);
}

static void CheckNearbyintl() {
    std::feclearexcept(FE_ALL_EXCEPT);
    const long double zero = nearbyintl_nid_postfix(0.5L);
    Require(zero == 0.0L && !std::signbit(zero));
    Require(nearbyintl_nid_postfix(1.5L) == 2.0L && nearbyintl_nid_postfix(2.5L) == 2.0L);
    Require(nearbyintl_nid_postfix(2.7L) == 3.0L && nearbyintl_nid_postfix(-2.7L) == -3.0L);
    Require(ExtendedBits(nearbyintl_nid_postfix(-0.5L), 0, 0x8000));
    Require(nearbyintl_nid_postfix(0x1p200L) == 0x1p200L);
    Require(nearbyintl_nid_postfix(INFINITY) == INFINITY && nearbyintl_nid_postfix(-INFINITY) == -INFINITY);
    Require(std::isnan(nearbyintl_nid_postfix(NAN)));
    Require(std::fetestexcept(FE_INEXACT) == 0);
}

static void CheckFmodErrno() {
    errno = 0;
    (void)fmod_nid_postfix(1.0, 0.0);
    Require(errno == 0);
    Require(fmod_nid_postfix(1e300, 3.0) == std::fmod(1e300, 3.0));
}

static long double MakeExtended(std::uint64_t mantissa, std::uint16_t signExponent) {
    unsigned char bytes[sizeof(long double)]{};
    std::memcpy(bytes, &mantissa, 8);
    std::memcpy(bytes + 8, &signExponent, 2);
    long double value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

static void CheckAsinl() {
    const struct { std::uint64_t inputMantissa; std::uint16_t inputSignExponent; std::uint64_t outputMantissa; std::uint16_t outputSignExponent; } cases[] = {
        {0, 0x0000, 0, 0x0000}, {0, 0x8000, 0, 0x8000},
        {0x8000000000000000ull, 0x3fd7, 0x8000000000000000ull, 0x3fd7}, {0x8000000000000000ull, 0xbfd7, 0x8000000000000000ull, 0xbfd7},
        {0x8000000000000000ull, 0x3ffd, 0x815f4e705c3e68f3ull, 0x3ffd}, {0x8000000000000000ull, 0xbffd, 0x815f4e705c3e68f3ull, 0xbffd},
        {0xcccccccccccccccdull, 0x3ffb, 0xcd2493b01bc41e02ull, 0x3ffb},
        {0x8000000000000000ull, 0x3ffe, 0x860a91c16b9b2c23ull, 0x3ffe}, {0x8000000000000000ull, 0xbffe, 0x860a91c16b9b2c23ull, 0xbffe},
        {0xc000000000000000ull, 0xbffe, 0xd91a98ae3406e041ull, 0xbffe}, {0xe666666666666666ull, 0x3ffe, 0x8f549b82fd0b8fa3ull, 0x3fff},
        {0xf333333333333333ull, 0x3ffe, 0xa06a08acf9a04f78ull, 0x3fff}, {0xfd70a3d70a3d70a4ull, 0xbffe, 0xb6f1e3799ea8ecc3ull, 0xbfff},
        {0xffffffffffffffffull, 0x3ffe, 0xc90fdaa16c63cf01ull, 0x3fff},
        {0x8000000000000000ull, 0x3fff, 0xc90fdaa22168c235ull, 0x3fff}, {0x8000000000000000ull, 0xbfff, 0xc90fdaa22168c235ull, 0xbfff},
    };
    for (const auto& test : cases) {
        *__error_nid_postfix() = 12345;
        const long double result = asinl_nid_postfix(MakeExtended(test.inputMantissa, test.inputSignExponent));
        if (!ExtendedBits(result, test.outputMantissa, test.outputSignExponent) || *__error_nid_postfix() != 12345) {
            std::fprintf(stderr, "Guest asinl failed for %016llx:%04x\n", static_cast<unsigned long long>(test.inputMantissa), test.inputSignExponent);
            std::abort();
        }
    }
    Require(std::fabs(asinl_nid_postfix(0.5L) - 0.523598775598298873077107230547L) <= 0x1p-64L);
    const long double domainErrors[] = {
        1.5L, -2.0L, MakeExtended(0x8000000000000001ull, 0x3fff), MakeExtended(0x8000000000000001ull, 0xbfff),
        std::numeric_limits<long double>::max(), std::numeric_limits<long double>::infinity(), -std::numeric_limits<long double>::infinity(),
        std::numeric_limits<long double>::quiet_NaN(),
    };
    for (const long double input : domainErrors) {
        std::feclearexcept(FE_ALL_EXCEPT);
        *__error_nid_postfix() = 0;
        Require(std::isnan(asinl_nid_postfix(input)) && *__error_nid_postfix() == EDOM && std::fetestexcept(FE_INVALID) != 0);
    }
    *__error_nid_postfix() = 0;
}

static void CheckIntegerConversions() {
    for (const long long numerator : {4294967301LL, -4294967301LL}) {
        for (const long long denominator : {3LL, -3LL}) {
            const auto result = lldiv_nid_postfix(numerator, denominator);
            Require(result.quot == numerator / denominator && result.rem == numerator % denominator);
            Require(result.quot * denominator + result.rem == numerator);
            const auto wide = ldiv_nid_postfix(numerator, denominator);
            Require(wide.quot == result.quot && wide.rem == result.rem);
        }
    }
    const auto extreme = ldiv_nid_postfix(INT64_MIN, 2);
    Require(extreme.quot == INT64_MIN / 2 && extreme.rem == 0);
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
        {"0b101", 0, 0, 1, 0},
        {"0b101", 2, 0, 1, 0},
        {" -0B1!", 0, 0, 3, 0},
        {"0b2", 0, 0, 1, 0},
        {"0b101", 16, 0xb101, 5, 0},
        {" -0xz!", 0, 0, 3, 0},
        {"0xg", 16, 0, 1, 0},
        {"0x", 16, 0, 1, 0},
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
        {"0b101", 0, 0, 1, 0},
        {"0B11", 2, 0, 1, 0},
        {" +0b1!", 2, 0, 3, 0},
        {"0b101", 16, 0xb101, 5, 0},
        {" +0Xg!", 0, 0, 3, 0},
        {"0x!", 16, 0, 1, 0},
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
    for (const int base : {0, 2}) {
        const char text[] = " -0B11";
        char* end = nullptr;
        Require(strtoll_nid_postfix(text, &end, base) == 0 && end == text + 3);
        Require(strtoull_nid_postfix(text, &end, base) == 0 && end == text + 3);
        Require(strtoumax_nid_postfix(text, &end, base) == 0 && end == text + 3);
        Require(_Stoull_nid_postfix(text, &end, base) == 0 && end == text + 3);
        Require(_Stoul_nid_postfix(text, &end, base) == 0 && end == text + 3);
    }
    Require(atol_nid_postfix(" \t-4294967296tail") == -INT64_C(4294967296));
    Require(atol_nid_postfix("9223372036854775807") == INT64_MAX && atol_nid_postfix("+12") == 12);
    Require(atol_nid_postfix("0x10") == 0 && atol_nid_postfix("010") == 10 && atol_nid_postfix("") == 0);
    Require(atoll_nid_postfix("-9223372036854775808") == INT64_MIN && atoll_nid_postfix("4294967297x") == 4294967297LL);
    Require(atoll_nid_postfix("  -0012") == -12 && atoll_nid_postfix("z1") == 0);
    *__error_nid_postfix() = 13;
    Require(strtol_nid_postfix("-4294967296", nullptr, 10) == -INT64_C(4294967296));
    Require(*__error_nid_postfix() == 13);
    Require(strtoul_nid_postfix("4294967296", nullptr, 10) == UINT64_C(4294967296));
    Require(*__error_nid_postfix() == 13);
    Require(_Stoul_nid_postfix("4294967296", nullptr, 10) == UINT64_C(4294967296));
    Require(*__error_nid_postfix() == 13);
    Require(_Stoul_nid_postfix("18446744073709551615", nullptr, 10) == UINT64_MAX);
    Require(_Stoul_nid_postfix("-1", nullptr, 10) == UINT64_MAX && *__error_nid_postfix() == 13);
    *__error_nid_postfix() = 0;
}

static bool Near(double value, double expected, double tolerance) { return std::abs(value - expected) <= tolerance * std::abs(expected); }

static void CheckAsin() {
    const double pi = 3.14159265358979323846;
    const double pi_2 = pi / 2;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    Require(asin_nid_postfix(0.) == 0.);
    Require(std::signbit(asin_nid_postfix(-0.)));
    Require(Near(asin_nid_postfix(1.), pi_2, 1e-15));
    Require(Near(asin_nid_postfix(-1.), -pi_2, 1e-15));
    Require(std::isnan(asin_nid_postfix(2.0)) && std::signbit(asin_nid_postfix(2.0)));
    Require(std::isnan(asin_nid_postfix(-2.0)) && std::signbit(asin_nid_postfix(-2.0)));
    Require(std::isnan(asin_nid_postfix(infinity)) && std::signbit(asin_nid_postfix(infinity)));
    Require(std::isnan(asin_nid_postfix(-infinity)) && std::signbit(asin_nid_postfix(-infinity)));
    Require(std::isnan(asin_nid_postfix(nan)));
    Require(Near(asin_nid_postfix(0.5), pi/6, 1e-15));
    Require(Near(asin_nid_postfix(-0.5), -pi/6, 1e-15));
    Require(Near(asin_nid_postfix(1./std::sqrt(2)), pi/4, 1e-15));
    Require(asin_nid_postfix(1e-300) == 1e-300);
    Require(asin_nid_postfix(-1e-300) == -1e-300);
    std::feclearexcept(FE_ALL_EXCEPT);
    errno = 0;
    asin_nid_postfix(2.0);
    Require(std::fetestexcept(FE_INVALID));
    Require(errno == 0);
    std::feclearexcept(FE_ALL_EXCEPT);
    asin_nid_postfix(nan);
    Require(!std::fetestexcept(FE_INVALID));
}

static void CheckExtendedMath() {
    Require(ExtendedBits(atanl_nid_postfix(INFINITY), 0xc90fdaa22168c235ull, 0x3fff));
    Require(std::fabs(atanl_nid_postfix(1.0L) - 0.785398163397448309616L) < 1e-18L);
    Require(std::fabs(sinl_nid_postfix(0.5L) - 0.479425538604203000273L) < 1e-18L);
    Require(std::fabs(cosl_nid_postfix(0.5L) - 0.877582561890372716116L) < 1e-18L);
    Require(std::fabs(tanl_nid_postfix(0.5L) - 0.546302489843790513255L) < 1e-18L);
    Require(std::fabs(sinhl_nid_postfix(1.0L) - 1.17520119364380145688L) < 1e-18L);
    Require(std::fabs(coshl_nid_postfix(1.0L) - 1.54308063481524377848L) < 1e-18L);
    Require(std::fabs(tanhl_nid_postfix(0.5L) - 0.462117157260009758502L) < 1e-18L);
    Require(std::fabs(expl_nid_postfix(1.0L) - 2.71828182845904523536L) < 1e-18L);
    Require(std::fabs(logl_nid_postfix(2.0L) - 0.693147180559945309417L) < 1e-18L);
    Require(sinl_nid_postfix(0.0L) == 0.0L && std::signbit(sinl_nid_postfix(-0.0L)) && cosl_nid_postfix(0.0L) == 1.0L);
    Require(std::isnan(sinl_nid_postfix(INFINITY)) && std::isnan(cosl_nid_postfix(-INFINITY)) && std::isnan(tanl_nid_postfix(INFINITY)));
    Require(sinhl_nid_postfix(-INFINITY) == -INFINITY && coshl_nid_postfix(-INFINITY) == INFINITY && tanhl_nid_postfix(INFINITY) == 1.0L);
    Require(sinhl_nid_postfix(11358.0L) == INFINITY && std::isfinite(sinhl_nid_postfix(11356.0L)) && std::isfinite(coshl_nid_postfix(11356.0L)));
    Require(expl_nid_postfix(0.0L) == 1.0L && expl_nid_postfix(-INFINITY) == 0.0L && expl_nid_postfix(11357.0L) == INFINITY);
    Require(exp2l_nid_postfix(10.0L) == 1024.0L && exp2l_nid_postfix(-16446.0L) == 0.0L && exp2l_nid_postfix(16384.0L) == INFINITY);
    Require(ExtendedBits(exp2l_nid_postfix(-16445.0L), 1, 0) && ExtendedBits(exp2l_nid_postfix(-16444.0L), 2, 0));
    Require(logl_nid_postfix(1.0L) == 0.0L && logl_nid_postfix(0.0L) == -INFINITY && std::isnan(logl_nid_postfix(-1.0L)));
    Require(std::fabs(log10l_nid_postfix(1000.0L) - 3.0L) < 1e-18L && log10l_nid_postfix(1.0L) == 0.0L);
    Require(log10l_nid_postfix(0.0L) == -INFINITY && std::isnan(log10l_nid_postfix(-1.0L)));
    Require(powl_nid_postfix(2.0L, 64.0L) == 18446744073709551616.0L && powl_nid_postfix(-2.0L, 3.0L) == -8.0L);
    Require(powl_nid_postfix(0.0L, -1.0L) == INFINITY && powl_nid_postfix(1.0L, NAN) == 1.0L && std::isnan(powl_nid_postfix(-2.0L, 0.5L)));
    Require(ldexpl_nid_postfix(1.0L, 16383) == 0x1p16383L && ldexpl_nid_postfix(1.0L, 16384) == INFINITY && ldexpl_nid_postfix(3.0L, -2) == 0.75L);
    Require(ExtendedBits(ldexpl_nid_postfix(1.0L, -16445), 1, 0) && ldexpl_nid_postfix(1.0L, -16446) == 0.0L);
    Require(truncl_nid_postfix(2.7L) == 2.0L && truncl_nid_postfix(-2.7L) == -2.0L && ExtendedBits(truncl_nid_postfix(-0.5L), 0, 0x8000));
    Require(truncl_nid_postfix(9223372036854775807.5L) == 9223372036854775807.0L && truncl_nid_postfix(-INFINITY) == -INFINITY);
    Require(logb_nid_postfix(8.0) == 3.0 && logb_nid_postfix(0.75) == -1.0 && logb_nid_postfix(0x1p-1074) == -1074.0);
    Require(logb_nid_postfix(0.0) == -INFINITY && logb_nid_postfix(-INFINITY) == INFINITY && std::isnan(logb_nid_postfix(NAN)));
    Require(remainder_nid_postfix(5.0, 3.0) == -1.0 && remainder_nid_postfix(7.0, 2.0) == -1.0 && remainder_nid_postfix(5.0, 2.0) == 1.0);
    Require(std::signbit(remainder_nid_postfix(-4.0, 2.0)) && remainder_nid_postfix(1.0, INFINITY) == 1.0);
    Require(std::isnan(remainder_nid_postfix(1.0, 0.0)) && std::isnan(remainder_nid_postfix(INFINITY, 1.0)));
}

static void CheckHyperbolic() {
    const double infinity = std::numeric_limits<double>::infinity();
    Require(sinh_nid_postfix(0.) == 0. && std::signbit(sinh_nid_postfix(-0.)));
    Require(cosh_nid_postfix(0.) == 1. && coshf_nid_postfix(0.f) == 1.f);
    Require(asinh_nid_postfix(0.) == 0. && std::signbit(asinh_nid_postfix(-0.)) && std::signbit(asinhf_nid_postfix(-0.f)));
    Require(acosh_nid_postfix(1.) == 0. && acoshf_nid_postfix(1.f) == 0.f);
    Require(atanh_nid_postfix(0.) == 0. && atanh_nid_postfix(1.) == infinity && atanh_nid_postfix(-1.) == -infinity);
    Require(std::isnan(acosh_nid_postfix(0.5)) && std::isnan(acoshf_nid_postfix(0.5f)) && std::isnan(atanh_nid_postfix(2.)));
    Require(Near(sinh_nid_postfix(1.), 1.1752011936438014, 1e-15));
    Require(Near(cosh_nid_postfix(1.), 1.5430806348152437, 1e-15));
    Require(Near(coshf_nid_postfix(1.f), 1.5430806348152437, 1e-6));
    Require(Near(asinh_nid_postfix(1.), 0.881373587019543, 1e-15));
    Require(Near(asinhf_nid_postfix(1.f), 0.881373587019543, 1e-6));
    Require(Near(acosh_nid_postfix(2.), 1.3169578969248166, 1e-15));
    Require(Near(acoshf_nid_postfix(2.f), 1.3169578969248166, 1e-6));
    Require(Near(atanh_nid_postfix(0.5), 0.5493061443340549, 1e-15));
    Require(sinh_nid_postfix(1e-300) == 1e-300 && asinh_nid_postfix(1e-300) == 1e-300 && atanh_nid_postfix(1e-300) == 1e-300);
    Require(std::isfinite(cosh_nid_postfix(710.4)) && std::isfinite(sinh_nid_postfix(-710.4)) && sinh_nid_postfix(-710.4) < 0.);
    Require(Near(asinh_nid_postfix(-1e300), -691.4686750787736, 1e-15));
    Require(Near(acosh_nid_postfix(1e300), 691.4686750787736, 1e-15));
}

static void CheckSinhf() {
    const float infinity = std::numeric_limits<float>::infinity();
    const struct { std::uint32_t input; std::uint32_t output; } cases[] = {
        {0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u}, {0x00000001u, 0x00000001u}, {0x39000000u, 0x39000000u},
        {0x3e800000u, 0x3e815667u}, {0xbf000000u, 0xbf056680u}, {0x3f800000u, 0x3f966cfeu}, {0xbf800000u, 0xbf966cfeu},
        {0x40000000u, 0x40681e7bu}, {0x41100000u, 0x457d38acu}, {0xc1a00000u, 0xcd675844u}, {0x42b17217u, 0x7effff84u},
        {0x42b20000u, 0x7f28e166u}, {0xc2b20000u, 0xff28e166u}, {0x42b2d4fcu, 0x7f7fffecu},
        {0x7f800000u, 0x7f800000u}, {0xff800000u, 0xff800000u},
    };
    for (const auto& test : cases) {
        *__error_nid_postfix() = 12345;
        const float result = sinhf_nid_postfix(std::bit_cast<float>(test.input));
        if (std::bit_cast<std::uint32_t>(result) != test.output || *__error_nid_postfix() != 12345) {
            std::fprintf(stderr, "Guest sinhf failed for %08x\n", test.input);
            std::abort();
        }
    }
    Require(std::isnan(sinhf_nid_postfix(std::numeric_limits<float>::quiet_NaN())) && *__error_nid_postfix() == 12345);
    Require(Near(sinhf_nid_postfix(1.f), 1.1752011936438014, 1e-6) && Near(sinhf_nid_postfix(-2.f), -3.626860407847019, 1e-6));
    Require(Near(sinhf_nid_postfix(0.25f), 0.25261231680816831, 1e-6) && Near(sinhf_nid_postfix(20.f), 242582597.70489514, 1e-6));
    for (const std::uint32_t input : {0x42b2d4fdu, 0x42c80000u, 0x7f7fffffu}) {
        for (const std::uint32_t sign : {0u, 0x80000000u}) {
            std::feclearexcept(FE_ALL_EXCEPT);
            *__error_nid_postfix() = 0;
            const float result = sinhf_nid_postfix(std::bit_cast<float>(input | sign));
            Require(result == (sign != 0 ? -infinity : infinity) && *__error_nid_postfix() == ERANGE);
            Require(std::fetestexcept(FE_OVERFLOW) != 0);
        }
    }
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
        const int fpclass = test.code == 0 ? 0x10 : test.code == -2 ? 0x08 : test.code == -1 ? 0x04 : test.code == 1 ? 0x01 : 0x02;
        if (__fpclassifyf_nid_postfix(value) != fpclass) {
            std::fprintf(stderr, "Guest __fpclassifyf failed for %08x\n", test.bits);
            std::abort();
        }
    }
    Require(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FInf_nid_postfix)) == 1);
    Require(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FNan_nid_postfix)) == 2);
}

static void CheckMissingClassificationExports() {
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Require(isnanf_nid_postfix(nan) == 1);
    Require(isnanf_nid_postfix(infinity) == 0 && isnanf_nid_postfix(-infinity) == 0);
    Require(isnanf_nid_postfix(0.f) == 0 && isnanf_nid_postfix(-1.f) == 0);

    const long double negativeNan = std::copysign(std::numeric_limits<long double>::quiet_NaN(), -1.L);
    Require(__signbitl_nid_postfix(0.L) == 0 && __signbitl_nid_postfix(-0.L) == 1);
    Require(__signbitl_nid_postfix(1.L) == 0 && __signbitl_nid_postfix(-1.L) == 1);
    Require(__signbitl_nid_postfix(std::numeric_limits<long double>::infinity()) == 0);
    Require(__signbitl_nid_postfix(-std::numeric_limits<long double>::infinity()) == 1);
    Require(__signbitl_nid_postfix(negativeNan) == 1);
}

static void CheckDoubleRounding(double value, std::int64_t expected, bool invalid) {
    for (const bool useLlround : {false, true}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        errno = 12345;
        const std::int64_t result = useLlround ? llround_nid_postfix(value) : lround_nid_postfix(value);
        Require(result == expected);
        Require((std::fetestexcept(FE_INVALID) != 0) == invalid);
        Require(errno == 12345);
    }
}

static void CheckFloatRounding(float value, std::int64_t expected, bool invalid) {
    for (const bool useLlround : {false, true}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        errno = 12345;
        const std::int64_t result = useLlround ? llroundf_nid_postfix(value) : lroundf_nid_postfix(value);
        Require(result == expected);
        Require((std::fetestexcept(FE_INVALID) != 0) == invalid);
        Require(errno == 12345);
    }
}

static void CheckNoConversionIsPositiveZero() {
    for (const char* text : {"", " ", "-", "+", "-.", "+.", "-x", "-.e1", "-e5", "- 1", "  -."}) {
        char* end = nullptr;
        const double parsed = strtod_nid_postfix(text, &end);
        Require(parsed == 0. && !std::signbit(parsed) && end == text);
        const float parsedFloat = strtof_nid_postfix(text, &end);
        Require(parsedFloat == 0.f && !std::signbit(parsedFloat) && end == text);
        const long double parsedLong = strtold_nid_postfix(text, &end);
        Require(parsedLong == 0.L && !std::signbit(parsedLong) && end == text);
        Require(!std::signbit(atof_nid_postfix(text)));
    }
    char* end = nullptr;
    const char negativeZero[] = "-0.0x";
    const double parsed = strtod_nid_postfix(negativeZero, &end);
    Require(parsed == 0. && std::signbit(parsed) && end == negativeZero + 4);
    for (const char* text : {"-0", "-.0", "-0e5", "-0x0p0", "-0x"}) {
        const double keptDouble = strtod_nid_postfix(text, &end);
        Require(keptDouble == 0. && std::signbit(keptDouble) && end != text);
        const float keptFloat = strtof_nid_postfix(text, &end);
        Require(keptFloat == 0.f && std::signbit(keptFloat) && end != text);
        const long double keptLong = strtold_nid_postfix(text, nullptr);
        Require(keptLong == 0.L && std::signbit(keptLong));
    }
    Require(strtod_nid_postfix("-inf", nullptr) == -std::numeric_limits<double>::infinity());
    Require(std::isnan(atof_nid_postfix("nan")));
    const double noEndPointer = strtod_nid_postfix("-.", nullptr);
    Require(noEndPointer == 0. && !std::signbit(noEndPointer));
    const char16_t wide[] = u"-.";
    char16_t* wideEnd = nullptr;
    const char16_t wideKept[] = u" -0.0x";
    const double keptWide = wcstod_nid_postfix(wideKept, &wideEnd);
    Require(keptWide == 0. && std::signbit(keptWide) && wideEnd == wideKept + 5);
    const double parsedWide = wcstod_nid_postfix(wide, &wideEnd);
    Require(parsedWide == 0. && !std::signbit(parsedWide) && wideEnd == wide);
    const float parsedWideFloat = wcstof_nid_postfix(wide, &wideEnd);
    Require(parsedWideFloat == 0.f && !std::signbit(parsedWideFloat) && wideEnd == wide);
    const long double parsedWideLong = wcstold_nid_postfix(wide, &wideEnd);
    Require(parsedWideLong == 0.L && !std::signbit(parsedWideLong) && wideEnd == wide);
}

int main() {
    CheckFloatClassification();
    CheckNoConversionIsPositiveZero();
    CheckMissingClassificationExports();
    CheckIntegerConversions();
    CheckExtendedMath();
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
    Require(std::bit_cast<std::uint32_t>(nanf_nid_postfix(nullptr)) == 0x7fc00000u);
    Require(std::bit_cast<std::uint32_t>(nanf_nid_postfix("")) == 0x7fc00000u);
    Require(std::bit_cast<std::uint32_t>(nanf_nid_postfix("123")) == 0x7fc00000u);
    Require(!std::signbit(nanf_nid_postfix("")));
    Require(std::bit_cast<std::uint64_t>(nan_nid_postfix(nullptr)) == 0x7ff8000000000000ull);
    Require(std::bit_cast<std::uint64_t>(nan_nid_postfix("")) == 0x7ff8000000000000ull);
    Require(std::bit_cast<std::uint64_t>(nan_nid_postfix("123")) == 0x7ff8000000000000ull);
    Require(!std::signbit(nan_nid_postfix("")));
    Require(std::abs(asinf_nid_postfix(0.5f) - 0.5235988f) < 0.000001f);
    Require(std::abs(acosf_nid_postfix(0.5f) - 1.0471976f) < 0.000001f);
    Require(std::abs(atan2f_nid_postfix(1.f, -1.f) - 2.3561945f) < 0.000001f);
    Require(tanf_nid_postfix(0.f) == 0.f);
    unsigned int randomState = 1;
    Require(rand_r_nid_postfix(&randomState) == 33613 && randomState == 33613u);
    randomState = 0;
    Require(rand_r_nid_postfix(&randomState) == 16806 && randomState == 16806u);
    for (const unsigned int seed : {0u, 1u, 12345u, 0x7ffffffeu, 0xffffffffu}) {
        srand_nid_postfix(seed);
        randomState = seed;
        for (int step = 0; step < 8; ++step) Require(rand_r_nid_postfix(&randomState) == rand_nid_postfix());
    }
    bool threw = false;
    try { rand_r_nid_postfix(nullptr); } catch (...) { threw = true; }
    Require(threw);
    Require(hypot_nid_postfix(3.0, 4.0) == 5.0 && hypot_nid_postfix(-3.0, -4.0) == 5.0 && hypotf_nid_postfix(3.f, -4.f) == 5.f);
    Require(std::abs(hypot_nid_postfix(1e308, 1e308) / 1.4142135623730951e308 - 1.0) < 1e-15);
    Require(std::abs(hypotf_nid_postfix(2e38f, 2e38f) / 2.8284271e38f - 1.f) < 1e-6f);
    Require(std::isinf(hypot_nid_postfix(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN())));
    Require(std::isinf(hypotf_nid_postfix(std::numeric_limits<float>::quiet_NaN(), -std::numeric_limits<float>::infinity())));
    Require(std::isnan(hypot_nid_postfix(1.0, std::numeric_limits<double>::quiet_NaN())));
    for (const bool signalingFirst : {true, false}) {
        const float signaling = std::bit_cast<float>(std::uint32_t{0x7f800001});
        const float infinity = std::numeric_limits<float>::infinity();
        std::feclearexcept(FE_ALL_EXCEPT);
        const float result = signalingFirst ? hypotf_nid_postfix(signaling, infinity) : hypotf_nid_postfix(infinity, signaling);
        Require(std::isinf(result) && result > 0.f && std::fetestexcept(FE_INVALID) != 0);
    }
    Require(log10f_nid_postfix(100.f) == 2.f);
    Require(log1p_nid_postfix(0.) == 0. && std::signbit(log1p_nid_postfix(-0.)));
    Require(log1p_nid_postfix(-1.) == -std::numeric_limits<double>::infinity() && std::isnan(log1p_nid_postfix(-2.)));
    Require(log1p_nid_postfix(std::numeric_limits<double>::infinity()) == std::numeric_limits<double>::infinity());
    Require(log1p_nid_postfix(1e-300) == 1e-300 && log1pf_nid_postfix(1e-30f) == 1e-30f);
    Require(std::abs(log1p_nid_postfix(1.) - 0.6931471805599453) < 1e-16);
    Require(std::abs(log1p_nid_postfix(1e-10) - 9.9999999995e-11) < 1e-25);
    Require(std::abs(log1pf_nid_postfix(1.f) - 0.6931472f) < 1e-7f && std::isnan(log1pf_nid_postfix(-2.f)));
    Require(expm1_nid_postfix(0.) == 0. && std::signbit(expm1_nid_postfix(-0.)));
    Require(expm1_nid_postfix(1e-300) == 1e-300 && expm1_nid_postfix(-1000.) == -1.);
    Require(expm1_nid_postfix(-std::numeric_limits<double>::infinity()) == -1.);
    Require(std::abs(expm1_nid_postfix(1.) - 1.718281828459045) < 1e-15);
    Require(std::abs(expm1_nid_postfix(1e-10) - 1.00000000005e-10) < 1e-25);
    Require(expm1f_nid_postfix(0.f) == 0.f && std::signbit(expm1f_nid_postfix(-0.f)) && expm1f_nid_postfix(-1000.f) == -1.f);
    Require(std::abs(expm1f_nid_postfix(1.f) - 1.7182818f) < 1e-6f && std::isinf(expm1f_nid_postfix(std::numeric_limits<float>::infinity())));
    Require(erff_nid_postfix(0.f) == 0.f && std::signbit(erff_nid_postfix(-0.f)) && erff_nid_postfix(10.f) == 1.f && erff_nid_postfix(-10.f) == -1.f);
    Require(std::abs(erff_nid_postfix(0.5f) - 0.5204999f) < 1e-6f && std::isnan(erff_nid_postfix(std::numeric_limits<float>::quiet_NaN())));
    Require(lgamma_nid_postfix(1.) == 0. && lgamma_nid_postfix(2.) == 0. && std::isinf(lgamma_nid_postfix(0.)));
    Require(std::abs(lgamma_nid_postfix(0.5) - 0.5723649429247001) < 1e-15 && std::abs(lgamma_nid_postfix(10.) - 12.801827480081469) < 1e-13);
    Require(std::abs(lgamma_nid_postfix(-0.5) - 1.2655121234846454) < 1e-15);
    Require(llroundl_nid_postfix(2.5L) == 3 && llroundl_nid_postfix(-2.5L) == -3 && llroundl_nid_postfix(0.49999999999999999L) == 0);
    Require(llroundl_nid_postfix(9007199254740993.0L) == 9007199254740993LL && llroundl_nid_postfix(-1e30L) == INT64_MIN);
    Require(logbf_nid_postfix(8.f) == 3.f && logbf_nid_postfix(-0.75f) == -1.f);
    Require(logbf_nid_postfix(std::numeric_limits<float>::denorm_min()) == -149.f);
    Require(logbf_nid_postfix(0.f) == -std::numeric_limits<float>::infinity());
    Require(logbf_nid_postfix(-std::numeric_limits<float>::infinity()) == std::numeric_limits<float>::infinity());
    Require(std::isnan(logbf_nid_postfix(std::numeric_limits<float>::quiet_NaN())));
    Require(exp2_nid_postfix(-3.) == 0.125);
    Require(ldexp_nid_postfix(0.75, 4) == 12.);
    CheckSquareRoot();
    CheckExp2Errno();
    CheckExactOperations();
    CheckNearbyint();
    CheckLongDouble();
    CheckNearbyintl();
    CheckAsinl();
    Require(scalbn_nid_postfix(0.75, -2) == 0.1875);
    Require(scalbnf_nid_postfix(0.75f, 4) == 12.f);
    Require(nextafterf_nid_postfix(1.f, 2.f) == 0x1.000002p0f && nextafterf_nid_postfix(1.f, 0.f) == 0x1.fffffep-1f);
    Require(nextafterf_nid_postfix(0.f, 1.f) == std::numeric_limits<float>::denorm_min());
    Require(nextafterf_nid_postfix(0.f, -1.f) == -std::numeric_limits<float>::denorm_min());
    Require(nextafterf_nid_postfix(std::numeric_limits<float>::denorm_min(), -1.f) == 0.f);
    Require(nextafterf_nid_postfix(1.f, 1.f) == 1.f && !std::signbit(nextafterf_nid_postfix(-0.f, 0.f)));
    Require(nextafterf_nid_postfix(std::numeric_limits<float>::max(), std::numeric_limits<float>::infinity()) == std::numeric_limits<float>::infinity());
    Require(std::isnan(nextafterf_nid_postfix(std::numeric_limits<float>::quiet_NaN(), 1.f)) && std::isnan(nextafterf_nid_postfix(1.f, std::numeric_limits<float>::quiet_NaN())));
    int exponent = 0;
    Require(frexp_nid_postfix(12., &exponent) == 0.75 && exponent == 4);
    Require(frexpf_nid_postfix(-12.f, &exponent) == -0.75f && exponent == 4);
    Require(lround_nid_postfix(4294967296.5) == INT64_C(4294967297));
    Require(lround_nid_postfix(-2.5) == -3);
    Require(lroundf_nid_postfix(4294967296.f) == INT64_C(4294967296));
    Require(lroundf_nid_postfix(2.5f) == 3);
    Require(llround_nid_postfix(-4294967296.5) == -INT64_C(4294967297));
    Require(llroundf_nid_postfix(2.5f) == 3 && llroundf_nid_postfix(-2.5f) == -3 && llroundf_nid_postfix(-0.4f) == 0);
    Require(llroundf_nid_postfix(8589934592.f) == INT64_C(8589934592) && llroundf_nid_postfix(-0x1p62f) == -(INT64_C(1) << 62));
    Require(llroundf_nid_postfix(0.49999997f) == 0 && llroundf_nid_postfix(16777215.f) == 16777215);
    Require(lround_nid_postfix(0.49999999999999994) == 0 && lround_nid_postfix(-0.49999999999999994) == 0);
    constexpr double kTwo63 = 9223372036854775808.0;
    constexpr double kTwo63Below = 9223372036854774784.0;
    constexpr float kTwo63f = 9223372036854775808.f;
    constexpr float kTwo63fBelow = 9223371487098961920.f;
    CheckDoubleRounding(-kTwo63, INT64_MIN, false);
    CheckDoubleRounding(kTwo63Below, INT64_C(9223372036854774784), false);
    CheckDoubleRounding(-kTwo63Below, -INT64_C(9223372036854774784), false);
    CheckDoubleRounding(4503599627370497.0, INT64_C(4503599627370497), false);
    CheckFloatRounding(-kTwo63f, INT64_MIN, false);
    CheckFloatRounding(kTwo63fBelow, INT64_C(9223371487098961920), false);
    CheckFloatRounding(-kTwo63fBelow, -INT64_C(9223371487098961920), false);
    for (const double value : {kTwo63, -9223374235878031360.0, 1e30, -1e30, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        CheckDoubleRounding(value, INT64_MIN, true);
        CheckFloatRounding(static_cast<float>(value), INT64_MIN, true);
    }
    CheckFloatRounding(kTwo63f, INT64_MIN, true);
    std::feclearexcept(FE_ALL_EXCEPT);
    const auto infinity = std::numeric_limits<float>::infinity();
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    Require(__isinff_nid_postfix(infinity) == 1 && __isinff_nid_postfix(-infinity) == 1);
    Require(__isinff_nid_postfix(nan) == 0 && __isinff_nid_postfix(1.f) == 0);
    Require(__isinf_nid_postfix(std::numeric_limits<double>::infinity()) == 1 && __isinf_nid_postfix(-std::numeric_limits<double>::infinity()) == 1);
    Require(__isinf_nid_postfix(std::numeric_limits<double>::quiet_NaN()) == 0 && __isinf_nid_postfix(std::numeric_limits<double>::max()) == 0);
    Require(__isinf_nid_postfix(0.) == 0 && __isinf_nid_postfix(1e308 * 10) == 1);
    Require(__isfinitef_nid_postfix(0.f) == 1 && __isfinitef_nid_postfix(infinity) == 0);
    Require(__isfinitef_nid_postfix(nan) == 0);
    Require(__isnormalf_nid_postfix(1.f) == 1 && __isnormalf_nid_postfix(0.f) == 0);
    Require(__isnormalf_nid_postfix(std::numeric_limits<float>::denorm_min()) == 0);
    Require(__isnormal_nid_postfix(1.) == 1 && __isnormal_nid_postfix(0.) == 0);
    Require(__isnormal_nid_postfix(std::numeric_limits<double>::denorm_min()) == 0);
    CheckAsin();
    const auto quotient = div_nid_postfix(7, 2);
    CheckAsinfErrno();
    Require(quotient.quot == 3 && quotient.rem == 1);
    const auto negativeNumerator = div_nid_postfix(-7, 2);
    Require(negativeNumerator.quot == -3 && negativeNumerator.rem == -1);
    const auto negativeDenominator = div_nid_postfix(7, -2);
    Require(negativeDenominator.quot == -3 && negativeDenominator.rem == 1);
    const auto minimum = div_nid_postfix(std::numeric_limits<int>::min(), 10);
    Require(minimum.quot == -214748364 && minimum.rem == -8);
    Require(abs_nid_postfix(0) == 0);
    Require(abs_nid_postfix(42) == 42);
    Require(abs_nid_postfix(-42) == 42);
    Require(abs_nid_postfix(std::numeric_limits<int>::max()) == std::numeric_limits<int>::max());
    Require(abs_nid_postfix(std::numeric_limits<int>::min()) == std::numeric_limits<int>::min());
    Require(__powisf2_nid_postfix(2.0f, 10) == 1024.0f);
    Require(__powisf2_nid_postfix(2.0f, -10) == 0.0009765625f);
    Require(__powisf2_nid_postfix(-2.0f, 3) == -8.0f);
    Require(__powisf2_nid_postfix(-2.0f, -3) == -0.125f);
    Require(__powisf2_nid_postfix(-2.0f, 4) == 16.0f);
    Require(__powisf2_nid_postfix(0.0f, 0) == 1.0f);
    Require(__powisf2_nid_postfix(std::numeric_limits<float>::quiet_NaN(), 0) == 1.0f);
    Require(std::isnan(__powisf2_nid_postfix(std::numeric_limits<float>::quiet_NaN(), 1)));
    Require(__powisf2_nid_postfix(-1.0f, std::numeric_limits<int>::min()) == 1.0f);
    Require(__powisf2_nid_postfix(-1.0f, std::numeric_limits<int>::max()) == -1.0f);
    Require(__powisf2_nid_postfix(2.0f, std::numeric_limits<int>::min()) == 0.0f);
    Require(std::isinf(__powisf2_nid_postfix(0.0f, -1)));
    Require(std::signbit(__powisf2_nid_postfix(-0.0f, 3)));
    Require(std::isinf(__powisf2_nid_postfix(-0.0f, -3)) && std::signbit(__powisf2_nid_postfix(-0.0f, -3)));
    Require(__powisf2_nid_postfix(2.0f, -127) == 0x1p-127f);
    CheckHyperbolic();
    CheckSinhf();
    CheckHypotSpecialValue();
    CheckFmodErrno();
}
