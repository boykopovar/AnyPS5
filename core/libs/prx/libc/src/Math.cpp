#include <algorithm>
#include <mutex>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "prx/libc/include/General.hpp"

extern "C" {

std::lldiv_t APS5_VABI lldiv_nid_postfix(long long numerator, long long denominator) {
    return std::lldiv(numerator, denominator);
}

float APS5_VABI fmodf_nid_postfix(float x, float y) { return std::fmod(x, y); }
float APS5_VABI asinf_nid_postfix(float x) { return std::asin(x); }
float APS5_VABI acosf_nid_postfix(float x) { return std::acos(x); }
float APS5_VABI atan2f_nid_postfix(float y, float x) { return std::atan2(y, x); }
float APS5_VABI tanf_nid_postfix(float x) { return std::tan(x); }
float APS5_VABI log10f_nid_postfix(float x) { return std::log10(x); }
float APS5_VABI logbf_nid_postfix(float x) { return std::logb(x); }
double APS5_VABI exp2_nid_postfix(double x) { return std::exp2(x); }
double APS5_VABI ldexp_nid_postfix(double x, int exponent) { return std::ldexp(x, exponent); }
double APS5_VABI scalbn_nid_postfix(double x, int exponent) { return std::scalbn(x, exponent); }
float APS5_VABI scalbnf_nid_postfix(float x, int exponent) { return std::scalbn(x, exponent); }
double APS5_VABI frexp_nid_postfix(double x, int* exponent) { return std::frexp(x, exponent); }
float APS5_VABI frexpf_nid_postfix(float x, int* exponent) { return std::frexp(x, exponent); }
// Guest long is 64-bit, including on Windows where native long is 32-bit.
std::int64_t APS5_VABI lround_nid_postfix(double x) { return std::llround(x); }
std::int64_t APS5_VABI lroundf_nid_postfix(float x) { return std::llround(x); }
std::int64_t APS5_VABI llround_nid_postfix(double x) { return std::llround(x); }
int APS5_VABI __isfinitef_nid_postfix(float x) { return std::isfinite(x) ? 1 : 0; }
int APS5_VABI __isnormal_nid_postfix(double x) { return std::isnormal(x) ? 1 : 0; }
int APS5_VABI __isnormalf_nid_postfix(float x) { return std::isnormal(x) ? 1 : 0; }
int APS5_VABI __isinff_nid_postfix(float x) { return std::isinf(x) ? 1 : 0; }

double APS5_VABI cbrt_nid_postfix(double x) { return std::cbrt(x); }
double APS5_VABI asin_nid_postfix(double x) { return std::asin(x); }
double APS5_VABI acos_nid_postfix(double x) { return std::acos(x); }
double APS5_VABI exp_nid_postfix(double x) { return std::exp(x); }
double APS5_VABI atan_nid_postfix(double x) { return std::atan(x); }
double APS5_VABI tan_nid_postfix(double x) { return std::tan(x); }
double APS5_VABI log2_nid_postfix(double x) { return std::log2(x); }
double APS5_VABI log_nid_postfix(double x) { return std::log(x); }

float APS5_VABI sinf_nid_postfix(float x) { return std::sin(x); }
float APS5_VABI cosf_nid_postfix(float x) { return std::cos(x); }

void APS5_VABI sincosf_nid_postfix(float x, float* sinp, float* cosp) {
    *sinp = std::sin(x);
    *cosp = std::cos(x);
}

double APS5_VABI sin_nid_postfix(double x) { return std::sin(x); }
double APS5_VABI cos_nid_postfix(double x) { return std::cos(x); }

void APS5_VABI sincos_nid_postfix(double x, double* sinp, double* cosp) {
    *sinp = std::sin(x);
    *cosp = std::cos(x);
}

float APS5_VABI atanf_nid_postfix(float x) { return std::atan(x); }
double APS5_VABI atan2_nid_postfix(double y, double x) { return std::atan2(y, x); }
float APS5_VABI powf_nid_postfix(float base, float exp) { return std::pow(base, exp); }
double APS5_VABI pow_nid_postfix(double base, double exp) { return std::pow(base, exp); }
float APS5_VABI expf_nid_postfix(float x) { return std::exp(x); }
float APS5_VABI exp2f_nid_postfix(float x) { return std::exp2(x); }
float APS5_VABI logf_nid_postfix(float x) { return std::log(x); }
float APS5_VABI log2f_nid_postfix(float x) { return std::log2(x); }
double APS5_VABI log10_nid_postfix(double x) { return std::log10(x); }
float APS5_VABI ldexpf_nid_postfix(float x, int exp) { return std::ldexp(x, exp); }
double APS5_VABI fmod_nid_postfix(double x, double y) { return std::fmod(x, y); }
float APS5_VABI roundf_nid_postfix(float x) { return std::round(x); }
double APS5_VABI round_nid_postfix(double x) { return std::round(x); }
float APS5_VABI cbrtf_nid_postfix(float x) { return std::cbrt(x); }
float APS5_VABI remainderf_nid_postfix(float x, float y) { return std::remainder(x, y); }
int APS5_VABI __isfinite_nid_postfix(double x) { return std::isfinite(x) ? 1 : 0; }
int APS5_VABI __isnan_nid_postfix(double x) { return std::isnan(x) ? 1 : 0; }
int APS5_VABI __signbit_nid_postfix(double x) { return std::signbit(x) ? 1 : 0; }

double APS5_VABI modf_nid_postfix(double x, double* integral) { return std::modf(x, integral); }
float APS5_VABI modff_nid_postfix(float x, float* integral) { return std::modf(x, integral); }
double APS5_VABI tanh_nid_postfix(double x) { return std::tanh(x); }
float APS5_VABI tanhf_nid_postfix(float x) { return std::tanh(x); }
double APS5_VABI sinh_nid_postfix(double x) { return std::sinh(x); }
float APS5_VABI sinhf_nid_postfix(float x) { return std::sinh(x); }
double APS5_VABI cosh_nid_postfix(double x) { return std::cosh(x); }
float APS5_VABI coshf_nid_postfix(float x) { return std::cosh(x); }
double APS5_VABI asinh_nid_postfix(double x) { return std::asinh(x); }
float APS5_VABI asinhf_nid_postfix(float x) { return std::asinh(x); }
double APS5_VABI acosh_nid_postfix(double x) { return std::acosh(x); }
float APS5_VABI acoshf_nid_postfix(float x) { return std::acosh(x); }
double APS5_VABI atanh_nid_postfix(double x) { return std::atanh(x); }
float APS5_VABI atanhf_nid_postfix(float x) { return std::atanh(x); }
double APS5_VABI expm1_nid_postfix(double x) { return std::expm1(x); }
float APS5_VABI expm1f_nid_postfix(float x) { return std::expm1(x); }
double APS5_VABI log1p_nid_postfix(double x) { return std::log1p(x); }
float APS5_VABI log1pf_nid_postfix(float x) { return std::log1p(x); }
double APS5_VABI logb_nid_postfix(double x) { return std::logb(x); }
double APS5_VABI erf_nid_postfix(double x) { return std::erf(x); }
float APS5_VABI erff_nid_postfix(float x) { return std::erf(x); }
double APS5_VABI erfc_nid_postfix(double x) { return std::erfc(x); }
float APS5_VABI erfcf_nid_postfix(float x) { return std::erfc(x); }
double APS5_VABI tgamma_nid_postfix(double x) { return std::tgamma(x); }
float APS5_VABI tgammaf_nid_postfix(float x) { return std::tgamma(x); }
double APS5_VABI sqrt_nid_postfix(double x) { return std::sqrt(x); }
float APS5_VABI sqrtf_nid_postfix(float x) { return std::sqrt(x); }
double APS5_VABI fabs_nid_postfix(double x) { return std::fabs(x); }
float APS5_VABI fabsf_nid_postfix(float x) { return std::fabs(x); }
double APS5_VABI copysign_nid_postfix(double x, double y) { return std::copysign(x, y); }
float APS5_VABI copysignf_nid_postfix(float x, float y) { return std::copysign(x, y); }
double APS5_VABI ceil_nid_postfix(double x) { return std::ceil(x); }
float APS5_VABI ceilf_nid_postfix(float x) { return std::ceil(x); }
double APS5_VABI floor_nid_postfix(double x) { return std::floor(x); }
float APS5_VABI floorf_nid_postfix(float x) { return std::floor(x); }
double APS5_VABI trunc_nid_postfix(double x) { return std::trunc(x); }
float APS5_VABI truncf_nid_postfix(float x) { return std::trunc(x); }
double APS5_VABI rint_nid_postfix(double x) { return std::rint(x); }
float APS5_VABI rintf_nid_postfix(float x) { return std::rint(x); }
double APS5_VABI nearbyint_nid_postfix(double x) { return std::nearbyint(x); }
float APS5_VABI nearbyintf_nid_postfix(float x) { return std::nearbyint(x); }
std::int64_t APS5_VABI lrint_nid_postfix(double x) { return std::llrint(x); }
std::int64_t APS5_VABI lrintf_nid_postfix(float x) { return std::llrint(x); }
std::int64_t APS5_VABI llrint_nid_postfix(double x) { return std::llrint(x); }
std::int64_t APS5_VABI llrintf_nid_postfix(float x) { return std::llrint(x); }
std::int64_t APS5_VABI llroundf_nid_postfix(float x) { return std::llround(x); }
double APS5_VABI remainder_nid_postfix(double x, double y) { return std::remainder(x, y); }
double APS5_VABI fdim_nid_postfix(double x, double y) { return std::fdim(x, y); }
float APS5_VABI fdimf_nid_postfix(float x, float y) { return std::fdim(x, y); }
double APS5_VABI fmax_nid_postfix(double x, double y) { return std::fmax(x, y); }
float APS5_VABI fmaxf_nid_postfix(float x, float y) { return std::fmax(x, y); }
double APS5_VABI fmin_nid_postfix(double x, double y) { return std::fmin(x, y); }
float APS5_VABI fminf_nid_postfix(float x, float y) { return std::fmin(x, y); }
double APS5_VABI fma_nid_postfix(double x, double y, double z) { return std::fma(x, y, z); }
float APS5_VABI fmaf_nid_postfix(float x, float y, float z) { return std::fma(x, y, z); }
double APS5_VABI nextafter_nid_postfix(double x, double y) { return std::nextafter(x, y); }
float APS5_VABI nextafterf_nid_postfix(float x, float y) { return std::nextafter(x, y); }
double APS5_VABI scalbln_nid_postfix(double x, std::int64_t exponent) { return std::scalbn(x, static_cast<int>(std::clamp<std::int64_t>(exponent, -65536, 65536))); }
double APS5_VABI nan_nid_postfix(const char* tag) {
    if (tag == nullptr) throw std::invalid_argument("nan: null tag");
    return std::nan(tag);
}
float APS5_VABI nanf_nid_postfix(const char* tag) {
    if (tag == nullptr) throw std::invalid_argument("nanf: null tag");
    return std::nanf(tag);
}
float APS5_VABI _FSinh_nid_postfix(float x, float y) { return y * std::sinh(x); }
float APS5_VABI _FCosh_nid_postfix(float x, float y) { return y * std::cosh(x); }

struct alignas(16) LibcFloatConstant { std::uint32_t bits[4]; };
LibcFloatConstant _FInf_nid_postfix {{0x7f800000u, 0, 0, 0}};
LibcFloatConstant _FNan_nid_postfix {{0x7fc00000u, 0, 0, 0}};

short APS5_VABI _FDtest_nid_postfix(const float* value) {
    constexpr short Denormal = -2, Finite = -1, Zero = 0, Infinite = 1, NotANumber = 2;
    if (value == nullptr) throw std::invalid_argument("_FDtest: null value");
    std::uint32_t bits;
    std::memcpy(&bits, value, sizeof(bits));
    const auto exponent = bits & 0x7f800000u;
    const auto fraction = bits & 0x007fffffu;
    if (exponent == 0x7f800000u) return fraction != 0 ? NotANumber : Infinite;
    if (exponent == 0) return fraction != 0 ? Denormal : Zero;
    return Finite;
}
int APS5_VABI __isnanf_nid_postfix(float x) { return std::isnan(x) ? 1 : 0; }
int APS5_VABI __signbitf_nid_postfix(float x) { return std::signbit(x) ? 1 : 0; }

static std::mutex g_randLock;
static std::uint32_t g_randState = 1;

int APS5_VABI rand_nid_postfix() {
    std::lock_guard lock(g_randLock);
    const std::int64_t x = static_cast<std::int64_t>(g_randState % 0x7ffffffeu) + 1;
    std::int64_t next = 16807 * (x % 127773) - 2836 * (x / 127773);
    if (next < 0) next += 0x7fffffff;
    g_randState = static_cast<std::uint32_t>(next - 1);
    return static_cast<int>(next - 1);
}

void APS5_VABI srand_nid_postfix(unsigned int seed) {
    std::lock_guard lock(g_randLock);
    g_randState = seed;
}

}
