#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
#include <source_location>
#include <xmmintrin.h>

static_assert(sizeof(GuestFloatingPointEnvironment) == 32);
extern "C" {

struct LibcFloatConstant { std::uint32_t bits[4]; };
extern LibcFloatConstant _FInf_nid_postfix;
extern LibcFloatConstant _FNan_nid_postfix;
extern LibcFloatConstant _FDenorm_nid_postfix;
extern LibcFloatConstant _FSnan_nid_postfix;
union SonyDconst { unsigned short _Word[8]; float _Float; double _Double; long double _Long_double; };
extern SonyDconst _Inf_nid_postfix;
extern SonyDconst _Denorm_nid_postfix;
extern SonyDconst _Hugeval_nid_postfix;
extern SonyDconst _Nan_nid_postfix;
extern SonyDconst _Snan_nid_postfix;
extern SonyDconst _LDenorm_nid_postfix;
extern SonyDconst _LInf_nid_postfix;
extern SonyDconst _LNan_nid_postfix;
extern SonyDconst _LSnan_nid_postfix;

extern int signgam_nid_postfix;

struct DinkumwareFloatCharacteristics {
    int digits;
    int mantissaDigits;
    int maxExponent10;
    int maxExponent;
    int minExponent10;
    int minExponent;
    alignas(16) unsigned char epsilon[16];
    alignas(16) unsigned char max[16];
    alignas(16) unsigned char min[16];
};
extern DinkumwareFloatCharacteristics _Flt_nid_postfix;
extern DinkumwareFloatCharacteristics _Dbl_nid_postfix;
extern DinkumwareFloatCharacteristics _Ldbl_nid_postfix;
}
static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), location.line());
    std::abort();
}
constexpr int StandardExceptions = FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT;

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
    Require(floor_nid_postfix(2.7) == 2. && floor_nid_postfix(-2.2) == -3. && floor_nid_postfix(5.) == 5.);
    Require(std::bit_cast<std::uint64_t>(floor_nid_postfix(-0.5)) == 0xbff0000000000000ull);
    Require(std::bit_cast<std::uint64_t>(floor_nid_postfix(-0.)) == 0x8000000000000000ull && floor_nid_postfix(-0x1p-1074) == -1.);
    Require(std::bit_cast<std::uint64_t>(floor_nid_postfix(0.5)) == 0);
    Require(floor_nid_postfix(4503599627370495.5) == 4503599627370495. && floor_nid_postfix(-1e300) == -1e300);
    Require(floor_nid_postfix(INFINITY) == INFINITY && floor_nid_postfix(-INFINITY) == -INFINITY && std::isnan(floor_nid_postfix(nan)));
    Require(std::fesetround(FE_UPWARD) == 0);
    const bool floorIgnoresRounding = floor_nid_postfix(2.7) == 2. && floor_nid_postfix(-2.2) == -3.;
    Require(std::fesetround(FE_TONEAREST) == 0 && floorIgnoresRounding);
    Require(fma_nid_postfix(2., 3., 4.) == 10.);
    volatile double tenth = 0.1;
    Require(fma_nid_postfix(tenth, 10., -1.) == 0x1p-54);
    Require(fma_nid_postfix(0x1p1023, 2., -0x1p1023) == 0x1p1023);
    Require(std::isnan(fma_nid_postfix(INFINITY, 0., 1.)));
    Require(fdim_nid_postfix(4.0, 1.0) == 3.0 && fdim_nid_postfix(1.0, 4.0) == 0.0);
    Require(fdim_nid_postfix(4.0, 4.0) == 0.0 && !std::signbit(fdim_nid_postfix(1.0, 4.0)));
    Require(std::isnan(fdim_nid_postfix(std::numeric_limits<double>::quiet_NaN(), 1.0)));
    Require(std::isnan(fdim_nid_postfix(1.0, std::numeric_limits<double>::quiet_NaN())));
    Require(fdimf_nid_postfix(4.0f, 1.0f) == 3.0f && fdimf_nid_postfix(1.0f, 4.0f) == 0.0f);
    Require(fdimf_nid_postfix(4.0f, 4.0f) == 0.0f && !std::signbit(fdimf_nid_postfix(1.0f, 4.0f)));
    Require(std::isnan(fdimf_nid_postfix(std::numeric_limits<float>::quiet_NaN(), 1.0f)));
    Require(fmaf_nid_postfix(2.f, 3.f, 4.f) == 10.f);
    volatile float tenthFloat = 0.1f;
    Require(fmaf_nid_postfix(tenthFloat, 10.f, -1.f) != 0.f);
    Require(std::isnan(fmaf_nid_postfix(INFINITY, 0.f, 1.f)));
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
    std::feclearexcept(FE_ALL_EXCEPT);
    std::uint16_t savedControl;
    asm volatile("fnstcw %0" : "=m"(savedControl));
    const unsigned savedMxcsr = _mm_getcsr();
    Require(std::fesetround(FE_UPWARD) == 0);
    _mm_setcsr((_mm_getcsr() & ~0x6000u) | 0x0000u);
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(nearbyintf_nid_postfix(2.5f) == 2.0f);
    Require(std::fetestexcept(FE_INEXACT) != 0);
    Require(std::fesetround(FE_TONEAREST) == 0);
    _mm_setcsr((_mm_getcsr() & ~0x6000u) | 0x4000u);
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(nearbyintf_nid_postfix(2.5f) == 3.0f);
    Require(std::fetestexcept(FE_INEXACT) != 0);
    struct RoundingCase {
        unsigned mode;
        std::uint32_t positiveHalf;
        std::uint32_t negativeHalf;
        std::uint32_t positiveTwoHalf;
        std::uint32_t negativeTwoHalf;
    };
    struct RoundingValue {
        float input;
        std::uint32_t expected;
    };
    for (const auto& test : {RoundingCase{0x0000u, 0x00000000u, 0x80000000u, 0x40000000u, 0xc0000000u},
                             RoundingCase{0x2000u, 0x00000000u, 0xbf800000u, 0x40000000u, 0xc0400000u},
                             RoundingCase{0x4000u, 0x3f800000u, 0x80000000u, 0x40400000u, 0xc0000000u},
                             RoundingCase{0x6000u, 0x00000000u, 0x80000000u, 0x40000000u, 0xc0000000u}}) {
        Require(std::fesetround(FE_TONEAREST) == 0);
        _mm_setcsr((_mm_getcsr() & ~0x6000u) | test.mode);
        for (const auto& value : {RoundingValue{0.5f, test.positiveHalf}, RoundingValue{-0.5f, test.negativeHalf},
                                  RoundingValue{2.5f, test.positiveTwoHalf}, RoundingValue{-2.5f, test.negativeTwoHalf}}) {
            std::feclearexcept(FE_ALL_EXCEPT);
            Require(std::bit_cast<std::uint32_t>(nearbyintf_nid_postfix(value.input)) == value.expected);
            Require(std::fetestexcept(FE_INEXACT) != 0);
        }
        for (const float zeroValue : {0.0f, -0.0f}) {
            std::feclearexcept(FE_ALL_EXCEPT);
            Require(std::bit_cast<std::uint32_t>(nearbyintf_nid_postfix(zeroValue)) == std::bit_cast<std::uint32_t>(zeroValue));
        }
    }
    asm volatile("fldcw %0" :: "m"(savedControl));
    _mm_setcsr(savedMxcsr);
}

static std::uint16_t X87ControlWord() {
    std::uint16_t control;
    asm volatile("fnstcw %0" : "=m"(control));
    return control;
}

static std::uint16_t X87StatusWord() {
    std::uint16_t status;
    asm volatile("fnstsw %0" : "=m"(status));
    return status;
}

static void CheckFegetenv() {
    const std::uint16_t savedControl = X87ControlWord();
    const unsigned savedCsr = _mm_getcsr();
    const std::uint16_t control = 0x0f7e;
    const unsigned csr = 0x7fa0;
    asm volatile("fnclex");
    asm volatile("fldcw %0" : : "m"(control));
    _mm_setcsr(csr);
    volatile long double zero = 0.0L;
    volatile long double quotient = 1.0L / zero;
    (void)quotient;
    const std::uint16_t status = X87StatusWord();
    Require((status & 0x04) != 0 && (status & 0x01) == 0);

    GuestFloatingPointEnvironment environment;
    std::memset(&environment, 0xcc, sizeof(environment));
    Require(fegetenv_nid_postfix(&environment) == 0);
    const std::uint16_t controlAfter = X87ControlWord();
    const unsigned csrAfter = _mm_getcsr();

    asm volatile("fnclex");
    asm volatile("fldcw %0" : : "m"(savedControl));
    _mm_setcsr(savedCsr);

    Require((environment.control & 0xffff) == control);
    Require((environment.status & 0xffff) == status);
    Require(environment.mxcsr == csr);
    Require(controlAfter == control);
    Require(csrAfter == csr);
    bool threw = false;
    try { fegetenv_nid_postfix(nullptr); } catch (...) { threw = true; }
    Require(threw);
}

template<typename Bits>
struct ModfCase { Bits input, fraction, integral; unsigned flags; };

template<typename Float, typename Bits, std::size_t Count, typename Call>
static void CheckModfCases(const ModfCase<Bits> (&cases)[Count], Call call) {
    std::fenv_t saved;
    std::fegetenv(&saved);
    const auto savedCsr = _mm_getcsr();
    int* error = __error_nid_postfix();
    const int savedError = *error;
    const int rounding[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    for (unsigned mode = 0; mode < 4; ++mode) {
        for (unsigned denorm = 0; denorm < 4; ++denorm) {
            const unsigned control = 0x1f80u | (mode << 13u) | ((denorm & 1u) != 0 ? 0x40u : 0u) | ((denorm & 2u) != 0 ? 0x8000u : 0u);
            for (const unsigned sticky : {0u, 5u, 32u}) {
                for (const auto& item : cases) {
                    std::fesetenv(FE_DFL_ENV);
                    std::fesetround(rounding[mode]);
                    std::uint32_t environment[7];
                    asm volatile("fnstenv %0" : "=m"(environment));
                    environment[1] = (environment[1] & ~63u) | sticky;
                    asm volatile("fldenv %0" : : "m"(environment));
                    _mm_setcsr(control | sticky);
                    const auto beforeControl = X87ControlWord();
                    *error = 97;
                    Float integral = 77;
                    const auto fraction = std::bit_cast<Bits>(call(std::bit_cast<Float>(item.input), &integral));
                    const auto integralBits = std::bit_cast<Bits>(integral);
                    const auto csr = _mm_getcsr();
                    const auto x87Status = X87StatusWord();
                    const auto x87Control = X87ControlWord();
                    if (fraction != item.fraction || integralBits != item.integral || csr != (control | sticky | item.flags) || (x87Status & 63u) != sticky || x87Control != beforeControl || *error != 97) {
                        std::fprintf(stderr, "modf width=%zu input=%llx round=%u denorm=%u sticky=%u fraction=%llx integral=%llx csr=%x x87=%x\n", sizeof(Float), static_cast<unsigned long long>(item.input), mode, denorm, sticky, static_cast<unsigned long long>(fraction), static_cast<unsigned long long>(integralBits), csr, x87Status);
                        Require(false);
                    }
                }
            }
        }
    }
    std::fesetenv(&saved);
    _mm_setcsr(savedCsr);
    *error = savedError;
}

static void CheckModfDecomposition() {
    constexpr ModfCase<std::uint64_t> doubles[] = {
        {0x00000000ull, 0x00000000ull, 0x0000000000000000ull, 0u},
        {0x8000000000000000ull, 0x8000000000000000ull, 0x8000000000000000ull, 0u},
        {0x00000001ull, 0x00000001ull, 0x0000000000000000ull, 0u},
        {0x8000000000000001ull, 0x8000000000000001ull, 0x8000000000000000ull, 0u},
        {0xfffffffffffffull, 0xfffffffffffffull, 0x0000000000000000ull, 0u},
        {0x800fffffffffffffull, 0x800fffffffffffffull, 0x8000000000000000ull, 0u},
        {0x10000000000000ull, 0x10000000000000ull, 0x0000000000000000ull, 0u},
        {0x8010000000000000ull, 0x8010000000000000ull, 0x8000000000000000ull, 0u},
        {0x3fe0000000000000ull, 0x3fe0000000000000ull, 0x0000000000000000ull, 0u},
        {0x3ff0000000000000ull, 0x00000000ull, 0x3ff0000000000000ull, 0u},
        {0xbff0000000000000ull, 0x8000000000000000ull, 0xbff0000000000000ull, 0u},
        {0x4028000000000000ull, 0x00000000ull, 0x4028000000000000ull, 0u},
        {0x7fefffffffffffffull, 0x00000000ull, 0x7fefffffffffffffull, 0u},
        {0xffefffffffffffffull, 0x8000000000000000ull, 0xffefffffffffffffull, 0u},
        {0x7ff0000000000000ull, 0x00000000ull, 0x7ff0000000000000ull, 0u},
        {0xfff0000000000000ull, 0x8000000000000000ull, 0xfff0000000000000ull, 0u},
        {0x7ff0000000000001ull, 0x7ff0000000000001ull, 0x7ff0000000000001ull, 0u},
        {0xfff0000000000001ull, 0xfff0000000000001ull, 0xfff0000000000001ull, 0u},
        {0x7ff8000000001234ull, 0x7ff8000000001234ull, 0x7ff8000000001234ull, 0u},
        {0xfff8000000001234ull, 0xfff8000000001234ull, 0xfff8000000001234ull, 0u},
        {0x3ff4000000000000ull, 0x3fd0000000000000ull, 0x3ff0000000000000ull, 0u},
        {0xbff4000000000000ull, 0xbfd0000000000000ull, 0xbff0000000000000ull, 0u},
        {0x3ff8000000000000ull, 0x3fe0000000000000ull, 0x3ff0000000000000ull, 0u},
        {0xbff8000000000000ull, 0xbfe0000000000000ull, 0xbff0000000000000ull, 0u},
        {0x4006000000000000ull, 0x3fe8000000000000ull, 0x4000000000000000ull, 0u},
        {0xc006000000000000ull, 0xbfe8000000000000ull, 0xc000000000000000ull, 0u},
        {0x3fe8000000000000ull, 0x3fe8000000000000ull, 0x0000000000000000ull, 0u},
        {0xbfe8000000000000ull, 0xbfe8000000000000ull, 0x8000000000000000ull, 0u},
        {0x3fffffffffffffffull, 0x3feffffffffffffeull, 0x3ff0000000000000ull, 0u},
        {0xbfffffffffffffffull, 0xbfeffffffffffffeull, 0xbff0000000000000ull, 0u},
        {0x4320000000000001ull, 0x3fe0000000000000ull, 0x4320000000000000ull, 0u},
        {0xc320000000000001ull, 0xbfe0000000000000ull, 0xc320000000000000ull, 0u},
    };
    constexpr ModfCase<std::uint32_t> floats[] = {
        {0x00000000u, 0x00000000u, 0x00000000u, 0u},
        {0x80000000u, 0x80000000u, 0x80000000u, 0u},
        {0x00000001u, 0x00000001u, 0x00000000u, 0u},
        {0x80000001u, 0x80000001u, 0x80000000u, 0u},
        {0x007fffffu, 0x007fffffu, 0x00000000u, 0u},
        {0x807fffffu, 0x807fffffu, 0x80000000u, 0u},
        {0x00800000u, 0x00800000u, 0x00000000u, 0u},
        {0x80800000u, 0x80800000u, 0x80000000u, 0u},
        {0x3f000000u, 0x3f000000u, 0x00000000u, 0u},
        {0x3f800000u, 0x00000000u, 0x3f800000u, 0u},
        {0xbf800000u, 0x80000000u, 0xbf800000u, 0u},
        {0x41400000u, 0x00000000u, 0x41400000u, 0u},
        {0x7f7fffffu, 0x00000000u, 0x7f7fffffu, 0u},
        {0xff7fffffu, 0x80000000u, 0xff7fffffu, 0u},
        {0x7f800000u, 0x00000000u, 0x7f800000u, 0u},
        {0xff800000u, 0x80000000u, 0xff800000u, 0u},
        {0x7f800001u, 0x7f800001u, 0x7f800001u, 1u},
        {0xff800001u, 0xff800001u, 0xff800001u, 1u},
        {0x7fc01234u, 0x7fc01234u, 0x7fc01234u, 0u},
        {0xffc01234u, 0xffc01234u, 0xffc01234u, 0u},
        {0x3fa00000u, 0x3e800000u, 0x3f800000u, 0u},
        {0xbfa00000u, 0xbe800000u, 0xbf800000u, 0u},
        {0x3fc00000u, 0x3f000000u, 0x3f800000u, 0u},
        {0xbfc00000u, 0xbf000000u, 0xbf800000u, 0u},
        {0x40300000u, 0x3f400000u, 0x40000000u, 0u},
        {0xc0300000u, 0xbf400000u, 0xc0000000u, 0u},
        {0x3f400000u, 0x3f400000u, 0x00000000u, 0u},
        {0xbf400000u, 0xbf400000u, 0x80000000u, 0u},
        {0x3fffffffu, 0x3f7ffffeu, 0x3f800000u, 0u},
        {0xbfffffffu, 0xbf7ffffeu, 0xbf800000u, 0u},
        {0x4a800001u, 0x3f000000u, 0x4a800000u, 0u},
        {0xca800001u, 0xbf000000u, 0xca800000u, 0u},
    };
    CheckModfCases<double>(doubles, modf_nid_postfix);
    CheckModfCases<float>(floats, modff_nid_postfix);
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

static void CheckNanConversions() {
    struct MeasuredNan {
        const char* text;
        std::uint64_t bits;
        std::size_t used;
        std::size_t sequenceUsed;
        bool extendedMeasured;
        bool wideDoubleMeasured;
        bool wideOtherMeasured;
        std::uint32_t singleBits;
        std::size_t singleUsed;
    };
    static constexpr MeasuredNan cases[] = {
        {"nan", 0x7ff8000000000000ull, 3, 3, true, true, true, 0x7fc00000u, 3},
        {"NAN", 0x7ff8000000000000ull, 3, 3, false, false, false, 0x7fc00000u, 3},
        {"nan()", 0x7ff8000000000000ull, 5, 5, false, false, false, 0x7fc00000u, 5},
        {"nan(1)", 0x7ff8000000000001ull, 6, 6, true, true, true, 0x7fc00001u, 6},
        {"nan(0x10)", 0x7ff8000000000010ull, 9, 9, false, false, false, 0x7fc00010u, 9},
        {"nan(abc)", 0x7ff8000000000abcull, 8, 8, false, false, false, 0x7fc00abcu, 8},
        {"nan(", 0x7ff8000000000000ull, 3, 3, true, true, true, 0x7fc00000u, 3},
        {"-nan", 0xfff8000000000000ull, 4, 4, true, true, true, 0xffc00000u, 4},
        {"+nan(5)", 0x7ff8000000000005ull, 7, 7, false, false, false, 0x7fc00005u, 7},
        {"nan(10)", 0x7ff8000000000010ull, 7, 7, false, false, false, 0x7fc00010u, 7},
        {"nan(xyz)", 0x7ff8000000000000ull, 8, 8, false, false, false, 0x7fc00000u, 8},
        {"nan(_)", 0x7ff8000000000000ull, 6, 6, false, false, false, 0x7fc00000u, 6},
        {"nan(12", 0x7ff8000000000012ull, 3, 3, false, false, false, 0x7fc00012u, 3},
        {"-nan(5)", 0xfff8000000000005ull, 7, 7, false, false, false, 0xffc00005u, 7},
        {"nan(0x8000000000000)", 0x7ff8000000000000ull, 20, 20, false, false, false, 0x7fc00001u, 20},
        {"nan(0x10000000000000)", 0x7ff8000000000001ull, 21, 21, true, true, false, 0x7fc00001u, 21},
        {"nan(0x400000)", 0x7ff8000000400000ull, 13, 13, false, false, false, 0x7fc00000u, 13},
        {"NaN(0X1F)", 0x7ff800000000001full, 9, 9, false, false, false, 0x7fc0001fu, 9},
        {"nan(0x)", 0x7ff8000000000000ull, 7, 7, false, false, false, 0x7fc00000u, 7},
        {"nan(123abc)", 0x7ff8000000123abcull, 11, 11, true, true, false, 0x7fd23abcu, 11},
        {"nan(1x2)", 0x7ff8000000000000ull, 8, 8, true, true, false, 0x7fc00000u, 8},
        {"nan(0x1g)", 0x7ff8000000000000ull, 9, 9, true, true, false, 0x7fc00000u, 9},
        {"nan(g1)", 0x7ff8000000000000ull, 7, 7, true, true, false, 0x7fc00000u, 7},
        {"nan(0x0x1)", 0x7ff8000000000000ull, 10, 10, true, true, false, 0x7fc00000u, 10},
        {"nan(fffffffffffff)", 0x7fffffffffffffffull, 18, 18, true, true, false, 0x7fffffffu, 18},
        {"nan(1fffffffffffff)", 0x7fffffffffffffffull, 19, 19, true, true, false, 0x7fffffffu, 19},
        {"nan(10000000000000)", 0x7ff8000000000001ull, 19, 19, true, true, false, 0x7fc00001u, 19},
        {"nan(20000000000000)", 0x7ff8000000000001ull, 19, 19, true, true, false, 0x7fc00001u, 19},
        {"nan(0x20000000000000)", 0x7ff8000000000001ull, 21, 21, true, true, false, 0x7fc00001u, 21},
        {"nan(ffffffffffffffff)", 0x7fffffffffffffffull, 21, 21, true, true, false, 0x7fffffffu, 21},
        {"nan(10000000000000000)", 0x7ff8000000000001ull, 22, 22, true, true, false, 0x7fc00001u, 22},
        {"nan(0X)", 0x7ff8000000000000ull, 7, 7, true, true, false, 0x7fc00000u, 7},
        {"nan(-1)", 0x7ff8000000000000ull, 7, 3, true, true, false, 0x7fc00000u, 7},
        {"nan(+1)", 0x7ff8000000000000ull, 7, 3, true, true, false, 0x7fc00000u, 7},
        {"nan( 1)", 0x7ff8000000000001ull, 7, 3, true, true, false, 0x7fc00001u, 7},
        {"nan(1 )", 0x7ff8000100000000ull, 7, 3, true, true, false, 0x7fc00001u, 7},
        {"nan(0)", 0x7ff8000000000001ull, 6, 6, true, true, false, 0x7fc00001u, 6},
        {"nan(00001)", 0x7ff8000000000001ull, 10, 10, true, true, false, 0x7fc00001u, 10},
        {"nan(0x0001)", 0x7ff8000000000001ull, 11, 11, true, true, false, 0x7fc00001u, 11},
        {"nan(8000000000000)", 0x7ff8000000000000ull, 18, 18, true, true, false, 0x7fc00001u, 18},
        {"nan(7ffffffffffff)", 0x7fffffffffffffffull, 18, 18, true, true, false, 0x7fffffffu, 18},
        {"nan(123", 0x7ff8000000000123ull, 3, 3, true, true, false, 0x7fc00123u, 3},
        {"nan(1)x", 0x7ff8000000000001ull, 6, 6, true, true, false, 0x7fc00001u, 6},
        {"-nan(1)", 0xfff8000000000001ull, 7, 7, true, true, false, 0xffc00001u, 7},
        {"nan(abcdefABCDEF)", 0x7ff8abcdefabcdefull, 17, 17, true, true, false, 0x7fcdefabu, 17},
        {"nan(0xABCDEF)", 0x7ff8000000abcdefull, 13, 13, true, true, false, 0x7febcdefu, 13},
        {"nan(9)", 0x7ff8000000000009ull, 6, 6, true, true, false, 0x7fc00009u, 6},
        {"nan(1_2)", 0x7ff8000000000000ull, 8, 8, true, true, false, 0x7fc00000u, 8},
        {"nan(_1)", 0x7ff8000000000000ull, 7, 7, true, true, false, 0x7fc00000u, 7},
        {"nan(1)(2)", 0x7ff8000000000001ull, 6, 6, true, true, false, 0x7fc00001u, 6},
        {"+nan", 0x7ff8000000000000ull, 4, 4, true, true, true, 0x7fc00000u, 4},
        {"NaN", 0x7ff8000000000000ull, 3, 3, true, true, true, 0x7fc00000u, 3},
        {"-NAN(5)", 0xfff8000000000005ull, 7, 7, true, true, true, 0xffc00005u, 7},
        {"nan(5)", 0x7ff8000000000005ull, 6, 6, true, true, true, 0x7fc00005u, 6},
        {"-nan(abc)", 0xfff8000000000abcull, 9, 9, true, true, true, 0xffc00abcu, 9},
        {"  -nan", 0xfff8000000000000ull, 6, 6, true, true, true, 0xffc00000u, 6},
        {"nan(12 x", 0x7ff8000000000000ull, 3, 3, true, true, true, 0x7fc00000u, 3},
        {"nan(12x", 0x7ff8000000000000ull, 3, 3, true, true, true, 0x7fc00000u, 3},
        {"nan(1", 0x7ff8000000000001ull, 3, 3, true, true, true, 0x7fc00001u, 3},
        {"nan(a b)", 0x7ff8000a0000000bull, 8, 3, true, true, true, 0x7fc0000au, 8},
        {"nan(ab)", 0x7ff80000000000abull, 7, 7, true, true, true, 0x7fc000abu, 7},
        {"nanx", 0x7ff8000000000000ull, 3, 3, true, true, true, 0x7fc00000u, 3},
        {"-nan()", 0xfff8000000000000ull, 6, 6, true, true, true, 0xffc00000u, 6},
        {"nan(0x8000000000001)", 0x7ff8000000000001ull, 20, 20, true, true, true, 0x7fc00001u, 20},
    };
    bool correct = true;
    for (const auto& test : cases) {
        char* end = nullptr;
        const auto value = std::bit_cast<std::uint64_t>(strtod_nid_postfix(test.text, &end));
        if (value != test.bits || end != test.text + test.used) {
            std::fprintf(stderr, "strtod(\"%s\") = 0x%016llx\n", test.text, static_cast<unsigned long long>(value));
            correct = false;
        }
        if (std::bit_cast<std::uint64_t>(atof_nid_postfix(test.text)) != test.bits) {
            std::fprintf(stderr, "atof(\"%s\")\n", test.text);
            correct = false;
        }
        const auto single = std::bit_cast<std::uint32_t>(strtof_nid_postfix(test.text, &end));
        if (single != test.singleBits || end != test.text + test.singleUsed) {
            std::fprintf(stderr, "strtof(\"%s\") = 0x%08x\n", test.text, static_cast<unsigned>(single));
            correct = false;
        }
        if (test.extendedMeasured) {
            const long double extended = strtold_nid_postfix(test.text, &end);
            if (!ExtendedBits(extended, 0xc000000000000000ull, 0x7fff) || end != test.text + test.sequenceUsed) {
                std::fprintf(stderr, "strtold(\"%s\")\n", test.text);
                correct = false;
            }
        }
        char16_t wide[64] = {};
        for (std::size_t i = 0; test.text[i] != 0; ++i) wide[i] = static_cast<unsigned char>(test.text[i]);
        char16_t* wideEnd = nullptr;
        const auto wideSingle = std::bit_cast<std::uint32_t>(wcstof_nid_postfix(wide, &wideEnd));
        if (wideSingle != test.singleBits || wideEnd != wide + test.singleUsed) {
            std::fprintf(stderr, "wcstof(\"%s\") = 0x%08x\n", test.text, static_cast<unsigned>(wideSingle));
            correct = false;
        }
        if (test.wideDoubleMeasured) {
            const auto wideValue = std::bit_cast<std::uint64_t>(wcstod_nid_postfix(wide, &wideEnd));
            if (wideValue != test.bits || wideEnd != wide + test.used) {
                std::fprintf(stderr, "wcstod(\"%s\") = 0x%016llx\n", test.text, static_cast<unsigned long long>(wideValue));
                correct = false;
            }
        }
        if (test.wideOtherMeasured) {
            const long double wideExtended = wcstold_nid_postfix(wide, &wideEnd);
            if (!ExtendedBits(wideExtended, 0xc000000000000000ull, 0x7fff) || wideEnd != wide + test.sequenceUsed) {
                std::fprintf(stderr, "wcstold(\"%s\")\n", test.text);
                correct = false;
            }
        }
    }
    Require(correct);
    for (const char* text : {"nan(1 ", "nan(12 \t"}) {
        bool threw = false;
        try { strtod_nid_postfix(text, nullptr); } catch (...) { threw = true; }
        Require(threw);
    }
    struct PlainNan { const char* text; std::uint64_t bits; std::size_t used; };
    for (const PlainNan& plain : {PlainNan{"nan(1 2)", 0x7ff8000100000002ull, 8}, PlainNan{"nan( \0)", 0x7ff8000000000000ull, 3}}) {
        char* end = nullptr;
        Require(std::bit_cast<std::uint64_t>(strtod_nid_postfix(plain.text, &end)) == plain.bits && end == plain.text + plain.used);
    }
}

static void CheckFloatUnderflowErrno() {
    char* end = nullptr;
    errno = 7;
    Require(strtod_nid_postfix("1e-310", &end) == 1e-310 && errno == ERANGE);
    Require(strtod_nid_postfix("-4.9406564584124654e-324", &end) == -0x1p-1074 && errno == ERANGE);
    errno = 7;
    Require(strtof_nid_postfix("1e-40", &end) == 1e-40f && errno == ERANGE);
    errno = 7;
    Require(strtold_nid_postfix("1e-4940", &end) == 1e-4940L && errno == 7);
    Require(strtod_nid_postfix("1e-330", &end) == 0.0 && errno == ERANGE);
    errno = 0;
    Require(strtof_nid_postfix("1e-46", &end) == 0.0f && errno == ERANGE);
    errno = 0;
    Require(strtod_nid_postfix("1e400", &end) == HUGE_VAL && errno == ERANGE);
    errno = 0;
    Require(strtod_nid_postfix("2.2250738585072014e-308", &end) == 0x1p-1022 && errno == 0);
    Require(strtod_nid_postfix("0x1.8p-1075", &end) == 0x1p-1074 && errno == ERANGE);
    errno = 0;
    Require(strtod_nid_postfix("-0x1.000001p-1075", &end) == -0x1p-1074 && errno == ERANGE);
    errno = 0;
    Require(strtod_nid_postfix("0x1p-1074", &end) == 0x1p-1074 && strtod_nid_postfix("0x1.8p-1074", &end) == 0x1p-1073 && errno == 0);
    Require(strtod_nid_postfix("0x1p-1076", &end) == 0.0 && errno == ERANGE);
    errno = 0;
    Require(strtod_nid_postfix("0x1p+1100", &end) == HUGE_VAL && errno == ERANGE);
    errno = 7;
    Require(strtold_nid_postfix("0x1p-16446", &end) == 0.0L && strtold_nid_postfix("0x1p+20000", &end) == HUGE_VALL && errno == 7);
    Require(strtof_nid_postfix("0x1p-149", &end) == 0x1p-149f && strtof_nid_postfix("0x1.8p-149", &end) == 0x1p-148f && strtof_nid_postfix("0x1.000001p-149", &end) == 0x1p-149f && errno == 7);
    Require(strtof_nid_postfix("1.40129846432481707092372958328991613128026194187651577175706828388979108268586060148663818836212158203125e-45", &end) == 0x1p-149f && strtof_nid_postfix("1.17549435e-38", &end) == 0x1p-126f && errno == 7);
    Require(strtof_nid_postfix("0x1p-150", &end) == 0.0f && errno == ERANGE);
    errno = 7;
    Require(strtof_nid_postfix("0x1.8p-150", &end) == 0x1p-149f && errno == ERANGE);
    errno = 7;
    Require(strtof_nid_postfix("-0x0.0000018p-126", &end) == -0x1p-149f && errno == ERANGE);
    errno = 7;
    Require(strtof_nid_postfix("0x1.4p-149", &end) == 0x1p-149f && strtof_nid_postfix("0x1.cp-149", &end) == 0x1p-148f && errno == 7);
    errno = 7;
    Require(strtof_nid_postfix("0x1p+200", &end) == HUGE_VALF && errno == ERANGE);
    errno = 7;
    Require(strtof_nid_postfix("1e-40", &end) == 1e-40f && errno == ERANGE);
    errno = 7;
    Require(strtof_nid_postfix("-1e-45", &end) == -0x1p-149f && errno == ERANGE);
    errno = 7;
    Require(strtof_nid_postfix("3e-45", &end) == 0x1p-148f && errno == ERANGE);
    Require(strtof_nid_postfix("1e39", &end) == HUGE_VALF && errno == ERANGE);
    errno = 0;
}

static void CheckLdexpfErrno() {
    constexpr struct {
        std::uint32_t input;
        int exponent;
        std::uint32_t expected;
    } cases[] = {
        {0x3f800000u, 200, 0x7f800000u},
        {0x101e8e30u, 1262, 0x7f800000u},
        {0xbf800000u, 128, 0xff800000u},
        {0x3f800000u, -200, 0x00000000u},
        {0x3f800000u, -2000, 0x00000000u},
        {0x3f800000u, -149, 0x00000001u},
        {0x3fc00000u, -149, 0x00000002u},
        {0x00000200u, 5, 0x00004000u},
    };
    for (const auto& item : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(ldexpf_nid_postfix(std::bit_cast<float>(item.input), item.exponent)) == item.expected);
        Require(errno == 1234);
    }
}

static void CheckAtanfConsoleBits() {
    constexpr std::uint32_t cases[][2] = {
        {0x3e80000bu, 0x3e7adbc5u}, {0x3e0007c9u, 0x3dfebd2au}, {0xbdccce4cu, 0xbdcc2090u},
        {0x3ef00334u, 0x3ee07046u}, {0x3f40077eu, 0x3f24c148u}, {0x3fa009a5u, 0x3f656b6au},
        {0x4080055cu, 0x3fa9b5a8u}, {0xbf12345eu, 0xbf04d718u}, {0x3f800000u, 0x3f490fdbu},
        {0x4c800000u, 0x3fc90fdbu}, {0xcc800000u, 0xbfc90fdbu}, {0x7f800000u, 0x3fc90fdbu},
        {0xff800000u, 0xbfc90fdbu}, {0x7fc00001u, 0x7fc00001u}, {0x80000000u, 0x80000000u},
        {0x00000001u, 0x00000001u}, {0x39000000u, 0x39000000u},
    };
    for (const auto& [input, expected] : cases) {
        Require(std::bit_cast<std::uint32_t>(atanf_nid_postfix(std::bit_cast<float>(input))) == expected);
    }
}

static void CheckAtan2fConsoleBits() {
    constexpr std::uint32_t cases[][3] = {
        {0x3d6f53e1u, 0x3fb6a73eu, 0x3d279f68u}, {0x3e587cf9u, 0x3f8f2054u, 0x3e3f5992u}, {0xbe8d427du, 0x3e57a3bau, 0xbf6b3a09u},
        {0x40162421u, 0xbd5afe7bu, 0x3fcbfa85u}, {0xc0d9c130u, 0xbd9ebbffu, 0xbfca8510u}, {0xbe077b06u, 0x40786368u, 0xbd0b9407u},
        {0xbe51a574u, 0xbdf4773fu, 0xc006505au}, {0x3fe6cdabu, 0xbf7fff9bu, 0x4004f006u}, {0x3f800000u, 0x3f800000u, 0x3f490fdbu},
        {0x40000000u, 0x3f800000u, 0x3f8db70du}, {0x00000000u, 0xbf800000u, 0x40490fdbu}, {0x80000000u, 0xbf800000u, 0xc0490fdbu},
        {0x3f800000u, 0x00000000u, 0x3fc90fdbu}, {0xbf800000u, 0x80000000u, 0xbfc90fdbu}, {0x7f800000u, 0x7f800000u, 0x3f490fdbu},
        {0xff800000u, 0xff800000u, 0xc016cbe4u}, {0x3f800000u, 0xff800000u, 0x40490fdbu}, {0xbf800000u, 0x7f800000u, 0x80000000u},
        {0x7f800000u, 0x3f000000u, 0x3fc90fdbu}, {0x4e000000u, 0x3f800001u, 0x3fc90fdbu}, {0x30000000u, 0xc0000000u, 0x40490fdbu},
        {0x7fc00000u, 0xffc00001u, 0x7fc00000u}, {0x3f800000u, 0x7fc00002u, 0x7fc00002u},
    };
    for (const auto& [y, x, expected] : cases) {
        Require(std::bit_cast<std::uint32_t>(atan2f_nid_postfix(std::bit_cast<float>(y), std::bit_cast<float>(x))) == expected);
    }
}

static void CheckHypotSpecialValue() {
    Require(std::bit_cast<std::uint64_t>(hypot_nid_postfix(INFINITY, std::bit_cast<double>(0x7ff0000000000001ull))) == 0x7ff0000000000000ull);
    Require(std::bit_cast<std::uint64_t>(hypot_nid_postfix(std::bit_cast<double>(0x7ff0000000000001ull), INFINITY)) == 0x7ff0000000000000ull);
    Require(std::bit_cast<std::uint64_t>(hypot_nid_postfix(-INFINITY, std::bit_cast<double>(0x7ff0000000000001ull))) == 0x7ff0000000000000ull);
    Require(std::bit_cast<std::uint64_t>(hypot_nid_postfix(std::bit_cast<double>(0x7ff0000000000001ull), -INFINITY)) == 0x7ff0000000000000ull);
}

static void CheckAsinfErrno() {
    for (const float input : {2.0f, -2.0f, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        errno = 0;
        std::feclearexcept(FE_ALL_EXCEPT);
        (void)asinf_nid_postfix(input);
        Require(errno == EDOM && std::fetestexcept(FE_INVALID) != 0);
    }
    for (const float input : {1.0f, -1.0f, 0.5f, 0.0f}) {
        errno = 1234;
        (void)asinf_nid_postfix(input);
        Require(errno == 1234);
    }
}

static void CheckAsinfConsoleBits() {
    constexpr std::uint32_t cases[][2] = {
        {0x3f000000u, 0x3f060a92u}, {0x3f800000u, 0x3fc90fdbu}, {0xbf800000u, 0xbfc90fdbu},
        {0x3b3a82dau, 0x3b3a82eau}, {0xbb753808u, 0xbb75382du}, {0x3f0008fbu, 0x3f0614f1u},
        {0xbf001216u, 0xbf061f75u}, {0x3f7fffffu, 0x3fc9048au}, {0x39000000u, 0x39000000u},
        {0x80000000u, 0x80000000u}, {0x00000001u, 0x00000001u}, {0x40000000u, 0xffc00000u},
        {0x7fc00001u, 0x7fc00001u},
    };
    for (const auto& [input, expected] : cases) {
        Require(std::bit_cast<std::uint32_t>(asinf_nid_postfix(std::bit_cast<float>(input))) == expected);
    }
}

static void CheckExp2Errno() {
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    double result = exp2_nid_postfix(1024.0);
    Require(errno == 0);
    Require(std::isinf(result) && result > 0);
    Require(std::fetestexcept(FE_OVERFLOW | FE_INEXACT) == (FE_OVERFLOW | FE_INEXACT));
}

static void CheckPowfUnderflowFlag() {
    constexpr struct {
        float x;
        float y;
        std::uint32_t expected;
    } cases[] = {{2.0f, -1075.0f, 0x00000000u}, {0.5f, 1024.0f, 0x00000000u}, {-2.0f, -1075.0f, 0x80000000u}, {1.5f, -1075.0f, 0x00000000u}};
    for (const auto& item : cases) {
        errno = 0;
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::bit_cast<std::uint32_t>(powf_nid_postfix(item.x, item.y)) == item.expected);
        Require(errno == ERANGE && std::fetestexcept(FE_UNDERFLOW) != 0);
    }
    errno = 1234;
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(std::bit_cast<std::uint32_t>(powf_nid_postfix(2.0f, -140.0f)) == 0x00000200u);
    Require(errno == 1234 && std::fetestexcept(FE_UNDERFLOW) == 0);
    constexpr struct {
        float x;
        float y;
        std::uint32_t expected;
        int flags;
    } subnormals[] = {{2.0f, -149.0f, 0x00000001u, 0}, {-2.0f, -141.0f, 0x80000100u, 0}, {3.0f, -90.0f, 0x00000052u, FE_UNDERFLOW | FE_INEXACT}};
    for (const auto& item : subnormals) {
        errno = 1234;
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::bit_cast<std::uint32_t>(powf_nid_postfix(item.x, item.y)) == item.expected);
        Require(errno == 1234 && std::fetestexcept(FE_UNDERFLOW | FE_INEXACT) == item.flags);
    }
    constexpr struct {
        float x;
        float y;
        std::uint32_t expected;
        int error;
        int flags;
    } largeExponents[] = {
        {0.5f, 3.0e7f, 0x00000000u, ERANGE, FE_UNDERFLOW | FE_INEXACT},
        {0.5f, 0x1p27f, 0x00000000u, ERANGE, FE_UNDERFLOW | FE_INEXACT},
        {0.5f, 0x1.000002p27f, 0x00000000u, 1234, FE_UNDERFLOW | FE_INEXACT},
        {0.5f, 3.0e8f, 0x00000000u, 1234, FE_UNDERFLOW | FE_INEXACT},
        {2.0f, -3.0e8f, 0x00000000u, 1234, FE_UNDERFLOW | FE_INEXACT},
        {2.0f, 3.0e8f, 0x7f800000u, 1234, FE_OVERFLOW | FE_INEXACT},
        {0.5f, -3.0e8f, 0x7f800000u, 1234, FE_OVERFLOW | FE_INEXACT},
        {0.5f, 0x1.fffffep127f, 0x00000000u, 1234, FE_UNDERFLOW | FE_INEXACT},
        {-2.0f, -3.0e8f, 0x00000000u, 1234, FE_UNDERFLOW | FE_INEXACT},
    };
    for (const auto& item : largeExponents) {
        errno = 1234;
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::bit_cast<std::uint32_t>(powf_nid_postfix(item.x, item.y)) == item.expected);
        Require(errno == item.error && std::fetestexcept(FE_ALL_EXCEPT) == item.flags);
    }
}

static void CheckPowfSpecialValue() {
    constexpr struct {
        std::uint32_t x;
        std::uint32_t y;
        std::uint32_t expected;
        int error;
    } cases[] = {
        {0x3f9e2cf9u, 0xbf5a98dcu, 0x3f55ab48u, 0}, {0x3fad4946u, 0x40c1ad7au, 0x40c82840u, 0}, {0x3f1467ddu, 0x40b9b6f1u, 0x3d2d08eeu, 0},
        {0x3fda2303u, 0x3f002254u, 0x3fa724dau, 0}, {0x3ff3e78du, 0xc1db422eu, 0x32b5d534u, 0}, {0x3f8d3e01u, 0x3f9f7952u, 0x3f90b3f4u, 0},
        {0x3f800000u, 0x7f800001u, 0x7fc00001u, 0}, {0x3f800000u, 0x7fc00000u, 0x7fc00000u, 0}, {0x3f800000u, 0x7f800000u, 0xffc00000u, 0},
        {0xbf800000u, 0xff800000u, 0xffc00000u, 0}, {0x3f800000u, 0x40000000u, 0x3f800000u, 0}, {0x40000000u, 0x00000000u, 0x3f800000u, 0},
        {0x7fc00001u, 0x00000000u, 0x3f800000u, 0}, {0x40000000u, 0x3f000000u, 0x3fb504f3u, 0}, {0x40800000u, 0x3f000000u, 0x40000000u, 0},
        {0xc0000000u, 0x40400000u, 0xc1000000u, 0}, {0xc0000000u, 0x3fc00000u, 0xffc00000u, EDOM}, {0xbf800000u, 0x3fc00000u, 0xffc00000u, EDOM},
        {0x00000000u, 0xc0000000u, 0x7f800000u, 0}, {0x80000000u, 0xc0400000u, 0xff800000u, 0}, {0x42c80000u, 0x42c80000u, 0x7f800000u, ERANGE},
        {0x3c23d70au, 0x42c80000u, 0x00000000u, ERANGE}, {0x40000000u, 0x43000000u, 0x7f800000u, ERANGE}, {0x40000000u, 0xc3160000u, 0x00000000u, ERANGE},
        {0x40000000u, 0xc3150000u, 0x00000001u, 0}, {0x40000000u, 0x4e000000u, 0x7f800000u, 0}, {0x3f000000u, 0x4e000000u, 0x00000000u, 0},
        {0xc0000000u, 0x4f000000u, 0x7f800000u, 0}, {0x7f800000u, 0xbf800000u, 0x00000000u, 0}, {0xff800000u, 0x40400000u, 0xff800000u, 0},
        {0x7fc00001u, 0xffc00002u, 0x7fc00001u, 0}, {0xffc00002u, 0x7fc00001u, 0xffc00002u, 0}, {0x7f800001u, 0x7fc00003u, 0x7fc00001u, 0},
        {0x7fc00003u, 0x7f800001u, 0x7fc00003u, 0}, {0xff800002u, 0x7fc00003u, 0xffc00002u, 0}, {0x7fc00003u, 0xff800002u, 0x7fc00003u, 0},
        {0x7f800001u, 0xff800002u, 0x7fc00001u, 0}, {0xff800002u, 0x7f800001u, 0xffc00002u, 0}, {0x7fffffffu, 0xffffffffu, 0x7fffffffu, 0},
        {0x7f800001u, 0x00000000u, 0x3f800000u, 0}, {0xff800002u, 0x80000000u, 0x3f800000u, 0}, {0xff800002u, 0x7f800000u, 0xffc00002u, 0},
        {0xff800000u, 0xff800002u, 0xffc00002u, 0},
    };
    for (const auto& item : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(powf_nid_postfix(std::bit_cast<float>(item.x), std::bit_cast<float>(item.y))) == item.expected);
        Require(errno == (item.error != 0 ? item.error : 1234));
    }
}

struct RestoreFloatingPointState {
    std::fenv_t environment;
    int error = errno;
    RestoreFloatingPointState() { Require(std::fegetenv(&environment) == 0); }
    ~RestoreFloatingPointState() { Require(std::fesetenv(&environment) == 0); errno = error; }
};

static void CheckAcosfErrno() {
    constexpr std::uint32_t domain[][2] = {
        {0x40000000u, 0xffc00000u}, {0xc0000000u, 0xffc00000u}, {0x7f800000u, 0xffc00000u}, {0xff800000u, 0xffc00000u},
        {0x3f800001u, 0xffc00000u}, {0x7fc00000u, 0x7fc00000u}, {0xffc00000u, 0xffc00000u},
    };
    for (const auto& [input, expected] : domain) {
        errno = 0;
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::bit_cast<std::uint32_t>(acosf_nid_postfix(std::bit_cast<float>(input))) == expected);
        Require(errno == EDOM && std::fetestexcept(FE_INVALID) != 0);
    }
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(std::bit_cast<std::uint32_t>(acosf_nid_postfix(0.5f)) == 0x3f860a92u);
    Require(errno == 0 && std::fetestexcept(FE_INVALID) == 0);
}

static void CheckFmodfErrno() {
    const RestoreFloatingPointState saved;
    const struct { std::uint32_t dividend; std::uint32_t divisor; std::uint32_t expected; bool invalid; bool nan; } cases[] = {
        {0x3f800000u, 0x00000000u, 0u, true, true}, {0x3f800000u, 0x80000000u, 0u, true, true},
        {0x7f800000u, 0x40000000u, 0u, true, true}, {0x7fc00000u, 0x40000000u, 0u, false, true},
        {0x3f800000u, 0x7f800000u, 0x3f800000u, false, false}, {0xbf800000u, 0x7f800000u, 0xbf800000u, false, false},
        {0x41280000u, 0x40000000u, 0x3f000000u, false, false}, {0xc1280000u, 0x40000000u, 0xbf000000u, false, false},
        {0x41280000u, 0xc0000000u, 0x3f000000u, false, false}, {0xc1280000u, 0xc0000000u, 0xbf000000u, false, false},
        {0x00000000u, 0x40000000u, 0x00000000u, false, false}, {0x80000000u, 0x40000000u, 0x80000000u, false, false},
        {0x00000000u, 0x80000000u, 0u, true, true},
        {0x3f800000u, 0x7fc00001u, 0u, false, true},
        {0x00000000u, 0x7fc00001u, 0u, false, true},
        {0x7f800001u, 0x40000000u, 0u, true, true},
        {0x3f800000u, 0x7f800001u, 0u, true, true},
        {0x00000003u, 0x00000002u, 0x00000001u, false, false},
        {0x00800001u, 0x00800000u, 0x00000001u, false, false},
        {0x80000001u, 0x3f800000u, 0x80000001u, false, false},
    };
    for (const auto& test : cases) {
        std::feclearexcept(FE_ALL_EXCEPT);
        *__error_nid_postfix() = 73;
        const float result = fmodf_nid_postfix(std::bit_cast<float>(test.dividend), std::bit_cast<float>(test.divisor));
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(result);
        const int invalid = std::fetestexcept(FE_INVALID) != 0;
        const bool nan = std::isnan(result);
        if (nan != test.nan || (!nan && bits != test.expected) || invalid != test.invalid || *__error_nid_postfix() != 73) {
            std::fprintf(stderr, "Guest fmodf failed for %08x and %08x: got %08x, invalid %d, errno %d\n",
                test.dividend, test.divisor, bits, invalid, *__error_nid_postfix());
            std::abort();
        }
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    std::feraiseexcept(FE_DIVBYZERO);
    *__error_nid_postfix() = 73;
    (void)fmodf_nid_postfix(1.0f, 0.0f);
    if (*__error_nid_postfix() != 73 || std::fetestexcept(FE_DIVBYZERO) == 0) {
        std::fprintf(stderr, "Guest fmodf dropped the floating-point environment\n");
        std::abort();
    }
}

static void CheckScalbnErrno() {
    constexpr struct {
        int exponent;
        std::uint64_t expected;
    } doubles[] = {
        {2000, 0x7ff0000000000000ull},
        {-2000, 0x0000000000000000ull},
        {-1074, 0x0000000000000001ull},
        {-1075, 0x0000000000000000ull},
        {200, 0x4c70000000000000ull},
    };
    for (const auto& item : doubles) {
        errno = 1234;
        Require(std::bit_cast<std::uint64_t>(scalbn_nid_postfix(1.0, item.exponent)) == item.expected);
        Require(errno == 1234);
    }
    constexpr struct {
        int exponent;
        std::uint32_t expected;
    } floats[] = {
        {2000, 0x7f800000u},
        {200, 0x7f800000u},
        {-200, 0x00000000u},
        {-2000, 0x00000000u},
        {10, 0x44800000u},
    };
    for (const auto& item : floats) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(scalbnf_nid_postfix(1.0f, item.exponent)) == item.expected);
        Require(errno == 1234);
    }
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
    Require(ceill_nid_postfix(2.2L) == 3.0L && ceill_nid_postfix(-2.7L) == -2.0L);
    Require(floorl_nid_postfix(2.7L) == 2.0L && floorl_nid_postfix(-2.2L) == -3.0L);
    Require(ceill_nid_postfix(2.0L) == 2.0L && floorl_nid_postfix(-2.0L) == -2.0L);
    Require(std::isnan(ceill_nid_postfix(NAN)) && std::isnan(floorl_nid_postfix(NAN)));
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

static void CheckExpm1ConsoleBits() {
    constexpr struct {
        std::uint64_t input;
        std::uint64_t expected;
        int error;
    } cases[] = {
        {0x0000000000000000ull, 0x0000000000000000ull, 0}, {0x8000000000000000ull, 0x8000000000000000ull, 0},
        {0x3c8fffffffffffffull, 0x3c8fffffffffffffull, 0}, {0x3c90000000000000ull, 0x3c90000000000000ull, 0},
        {0x3fd62e42fefa39efull, 0x3fda827999fcef32ull, 0}, {0x3fd62e4300000000ull, 0x3fda82799b6f2366ull, 0},
        {0x3ff0a2b1ffffffffull, 0x3ffd413c1a24d411ull, 0}, {0x3ff0a2b200000000ull, 0x3ffd413c1a24d414ull, 0},
        {0xbfd0000000000000ull, 0xbfcc5041854df7d4ull, 0}, {0xbfe0000000000000ull, 0xbfd92e9a0720d3ecull, 0},
        {0x3ff0000000000000ull, 0x3ffb7e151628aed2ull, 0}, {0xbff0000000000000ull, 0xbfe43a54e4e98864ull, 0},
        {0x4043687a00000000ull, 0x436fffd8395c0c41ull, 0}, {0xc043687a00000000ull, 0xbff0000000000000ull, 0},
        {0x4049000000000000ull, 0x44719103e4080b45ull, 0}, {0x40862e42fefa39efull, 0x7fefffffffffff2aull, 0},
        {0x40862e42fefa39f0ull, 0x7ff0000000000000ull, ERANGE}, {0xc08f400000000000ull, 0xbff0000000000000ull, 0},
        {0x7fefffffffffffffull, 0x7ff0000000000000ull, ERANGE}, {0xffefffffffffffffull, 0xbff0000000000000ull, 0},
        {0x7ff0000000000000ull, 0x7ff0000000000000ull, 0}, {0xfff0000000000000ull, 0xbff0000000000000ull, 0},
        {0x7ff8000000000001ull, 0x7ff8000000000001ull, 0}, {0xfff8000000000002ull, 0xfff8000000000002ull, 0},
        {0x3fc78b9e474afed8ull, 0x3fc9d99e60806b35ull, 0}, {0xbf887a99bea50fb1ull, 0xbf88554c5b9d0cbbull, 0},
        {0x3ff790fb0ae85390ull, 0x400ae5103402894bull, 0}, {0xbf840fe48e07e54dull, 0xbf83f6d1bcda4943ull, 0},
        {0xbfbb1276066a31c7ull, 0xbfb9b098289dab68ull, 0}, {0x3fc07bc20a0c887eull, 0x3fc197828317c86cull, 0},
    };
    for (const auto& item : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint64_t>(expm1_nid_postfix(std::bit_cast<double>(item.input))) == item.expected);
        Require(errno == (item.error != 0 ? item.error : 1234));
    }
}

static void CheckNextafterConsoleBits() {
    constexpr struct {
        std::uint64_t x;
        std::uint64_t y;
        std::uint64_t expected;
        int expectedErrno;
    } cases[] = {
        {0x0000000000000000ull, 0x3ff0000000000000ull, 0x0000000000000001ull, 0},
        {0x8000000000000000ull, 0xbff0000000000000ull, 0x8000000000000001ull, 0},
        {0x0000000000000001ull, 0x0000000000000000ull, 0x0000000000000000ull, ERANGE},
        {0x8000000000000001ull, 0x3ff0000000000000ull, 0x8000000000000000ull, ERANGE},
        {0x0010000000000000ull, 0x0000000000000000ull, 0x000fffffffffffffull, ERANGE},
        {0x000fffffffffffffull, 0x3ff0000000000000ull, 0x0010000000000000ull, 0},
        {0x7fefffffffffffffull, 0x7ff0000000000000ull, 0x7ff0000000000000ull, ERANGE},
        {0xffefffffffffffffull, 0xfff0000000000000ull, 0xfff0000000000000ull, ERANGE},
        {0x7ff0000000000000ull, 0x0000000000000000ull, 0x7fefffffffffffffull, 0},
        {0x3ff0000000000000ull, 0x4000000000000000ull, 0x3ff0000000000001ull, 0},
        {0x3ff0000000000000ull, 0x3ff0000000000000ull, 0x3ff0000000000000ull, 0},
        {0x8000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0},
        {0x7ff0000000000001ull, 0x3ff0000000000000ull, 0x7ff8000000000001ull, 0},
        {0x3ff0000000000000ull, 0xfff8000000000123ull, 0xfff8000000000123ull, 0},
        {0x7ff8000000000123ull, 0xfff8000000000456ull, 0x7ff8000000000123ull, 0},
        {0xfff8000000000456ull, 0x7ff0000000000001ull, 0xfff8000000000456ull, 0},
    };
    for (const auto& item : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint64_t>(nextafter_nid_postfix(std::bit_cast<double>(item.x), std::bit_cast<double>(item.y))) == item.expected);
        Require(errno == (item.expectedErrno != 0 ? item.expectedErrno : 1234));
    }
    constexpr struct {
        std::uint32_t x;
        std::uint32_t y;
        std::uint32_t expected;
        int expectedErrno;
    } floatCases[] = {
        {0x00000000u, 0x3f800000u, 0x00000001u, 0},
        {0x80000000u, 0xbf800000u, 0x80000001u, 0},
        {0x00000001u, 0x00000000u, 0x00000000u, ERANGE},
        {0x80000001u, 0x3f800000u, 0x80000000u, ERANGE},
        {0x00800000u, 0x00000000u, 0x007fffffu, ERANGE},
        {0x007fffffu, 0x3f800000u, 0x00800000u, 0},
        {0x7f7fffffu, 0x7f800000u, 0x7f800000u, ERANGE},
        {0xff7fffffu, 0xff800000u, 0xff800000u, ERANGE},
        {0x7f800000u, 0x00000000u, 0x7f7fffffu, 0},
        {0x3f800000u, 0x40000000u, 0x3f800001u, 0},
        {0x3f800000u, 0x3f800000u, 0x3f800000u, 0},
        {0x80000000u, 0x00000000u, 0x00000000u, 0},
        {0x7f800001u, 0x3f800000u, 0x7fc00001u, 0},
        {0x3f800000u, 0xffc00123u, 0xffc00123u, 0},
        {0x7fc00123u, 0xffc00456u, 0x7fc00123u, 0},
        {0xffc00456u, 0x7f800001u, 0xffc00456u, 0},
    };
    for (const auto& item : floatCases) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(nextafterf_nid_postfix(std::bit_cast<float>(item.x), std::bit_cast<float>(item.y))) == item.expected);
        Require(errno == (item.expectedErrno != 0 ? item.expectedErrno : 1234));
    }
}

static void CheckLog1pConsoleBits() {
    constexpr struct {
        std::uint64_t input;
        std::uint64_t expected;
        int error;
    } cases[] = {
        {0x0000000000000000ull, 0x0000000000000000ull, 0}, {0x8000000000000000ull, 0x8000000000000000ull, 0},
        {0x3c8fffffffffffffull, 0x3c8fffffffffffffull, 0}, {0x3c90000000000000ull, 0x3c90000000000000ull, 0},
        {0x3e1fffffffffffffull, 0x3e1fffffff7fffffull, 0}, {0x3e20000000000000ull, 0x3e1fffffff800000ull, 0},
        {0xbe20000000000000ull, 0xbe20000000400000ull, 0}, {0x3fda8279ffffffffull, 0x3fd62e43471c5d9dull, 0},
        {0x3fda827a00000000ull, 0x3fd62e43471c5d9eull, 0}, {0xbfd2bec400000000ull, 0xbfd62e4420e1ff10ull, 0},
        {0xbfd2bec3ffffffffull, 0xbfd62e4420e1ff0full, 0}, {0x3ff0000000000000ull, 0x3fe62e42fefa39efull, 0},
        {0xbfe0000000000000ull, 0xbfe62e42fefa39efull, 0}, {0xbfefffffffffffffull, 0xc0425e4f7b2737faull, 0},
        {0xbff0000000000000ull, 0xfff0000000000000ull, 0}, {0xbff0000000000001ull, 0xfff8000000000000ull, EDOM},
        {0xc000000000000000ull, 0xfff8000000000000ull, EDOM}, {0x433fffffffffffffull, 0x40425e4f7b2737faull, 0},
        {0x4340000000000000ull, 0x40425e4f7b2737faull, 0}, {0x7fefffffffffffffull, 0x40862e42fefa39efull, 0},
        {0x7ff0000000000000ull, 0x7ff0000000000000ull, 0}, {0xfff0000000000000ull, 0xfff8000000000000ull, EDOM},
        {0x7ff8000000000001ull, 0x7ff8000000000001ull, 0}, {0xfff8000000000002ull, 0xfff8000000000002ull, EDOM},
        {0x3f0f5ab6d3865ccaull, 0x3f0f5a7962a0216cull, 0}, {0x3fe8683f0ace287cull, 0x3fe223b9ede79242ull, 0},
        {0x3f5f8cd247124706ull, 0x3f5f850e039f6187ull, 0}, {0x40298faede87074bull, 0x4004fc71ddb00cc0ull, 0},
        {0x404b621796f08648ull, 0x401015adbb851c70ull, 0}, {0x3f21ecf9339d7420ull, 0x3f21eca8e0601790ull, 0},
    };
    for (const auto& item : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint64_t>(log1p_nid_postfix(std::bit_cast<double>(item.input))) == item.expected);
        Require(errno == (item.error != 0 ? item.error : 1234));
    }
}

static void CheckTanhConsoleBits() {
    constexpr std::uint64_t cases[][2] = {
        {0x0000000000000000ull, 0x0000000000000000ull}, {0x8000000000000000ull, 0x8000000000000000ull},
        {0x3e2fffffffffffffull, 0x3e2fffffffffffffull}, {0x3e30000000000000ull, 0x3e30000000000000ull},
        {0xbe30000000000000ull, 0xbe30000000000000ull}, {0x3fefffffffffffffull, 0x3fe85efab514f394ull},
        {0x3ff0000000000000ull, 0x3fe85efab514f394ull}, {0xbff0000000000000ull, 0xbfe85efab514f394ull},
        {0x3fe0000000000000ull, 0x3fdd9353d7568af3ull}, {0x4035ffffffffffffull, 0x3ff0000000000000ull},
        {0x4036000000000000ull, 0x3ff0000000000000ull}, {0xc036000000000000ull, 0xbff0000000000000ull},
        {0x4049000000000000ull, 0x3ff0000000000000ull}, {0x7fefffffffffffffull, 0x3ff0000000000000ull},
        {0x7ff0000000000000ull, 0x3ff0000000000000ull}, {0xfff0000000000000ull, 0xbff0000000000000ull},
        {0x7ff8000000000001ull, 0x7ff8000000000001ull}, {0xfff8000000000002ull, 0xfff8000000000002ull},
        {0x3f927ed0db8370b1ull, 0x3f927e4d1092a580ull}, {0x3f95d74e4cee78bfull, 0x3f95d67547d523a0ull},
        {0x3f18f1b062cb523cull, 0x3f18f1b06187faaeull}, {0x3f937129f25ae835ull, 0x3f937090dd8c9cb3ull},
        {0x3facb0d89f8aa91cull, 0x3faca92afe31a32bull}, {0x3f07f462f020acdeull, 0x3f07f462efd91530ull},
    };
    for (const auto& [input, expected] : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint64_t>(tanh_nid_postfix(std::bit_cast<double>(input))) == expected && errno == 1234);
    }
}

static void CheckSinhMsunBits() {
    constexpr std::uint64_t cases[][2] = {
        {0x0000000000000000ull, 0x0000000000000000ull}, {0x8000000000000000ull, 0x8000000000000000ull},
        {0x3e2fffffffffffffull, 0x3e2fffffffffffffull}, {0x3e30000000000000ull, 0x3e30000000000000ull},
        {0xbe30000000000000ull, 0xbe30000000000000ull}, {0x3ff0000000000000ull, 0x3ff2cd9fc44eb982ull},
        {0xbff0000000000000ull, 0xbff2cd9fc44eb982ull}, {0x3fefffffffffffffull, 0x3ff2cd9fc44eb982ull},
        {0x4035ffffffffffffull, 0x41dab5adb9c435e5ull}, {0xc035ffffffffffffull, 0xc1dab5adb9c435e5ull},
        {0x0000000000000001ull, 0x0000000000000001ull}, {0x7ff0000000000000ull, 0x7ff0000000000000ull},
        {0xfff0000000000000ull, 0xfff0000000000000ull},
        {0xbfd6a0128184d680ull, 0xbfd719794d320780ull}, {0xbfbdff8696a49900ull, 0xbfbe111cd84473d7ull},
        {0xbfd6d8bb6b4dca00ull, 0xbfd755bf29c06d04ull}, {0xbfcd8f04fd096d80ull, 0xbfcdd2743062c130ull},
        {0xbfdaafb23b684980ull, 0xbfdb7764637ce2b1ull}, {0x4030fc47aee85be4ull, 0x4166b421a358e13dull},
        {0x3ff0ed5ed6534a20ull, 0x3ff444323b77d92aull}, {0x402e263f514d91a0ull, 0x413ae005c1e986f0ull},
        {0x402dc690717af4e8ull, 0x41364b45352a81bdull}, {0x403270a47f0a53e2ull, 0x41884ea3c3c84121ull},
        {0xc012535983b8e876ull, 0xc04868fc3820124dull}, {0xc0304bcc37b4b009ull, 0xc156ca0b8a37acd3ull},
        {0xc01a5966b21f1396ull, 0xc076ae9222cc12f7ull}, {0xc02ddcff626eb852ull, 0xc1374ae500e7533full},
        {0xc020ed9a3019cd6eull, 0xc0a2854d2aab8a5full},
        {0x4035dab76b89b207ull, 0x41d716f7bb9c87dbull}, {0x4035d187a12d7d23ull, 0x41d6469c55754089ull},
        {0x403527ff77fdabb1ull, 0x41c6f9a671b8ec60ull}, {0x4035deab000d3561ull, 0x41d772ea071259caull},
        {0xc035232f2cb69934ull, 0xc1c68c16c49344beull},
    };
    for (const auto& [input, expected] : cases) {
        const std::uint64_t result = std::bit_cast<std::uint64_t>(sinh_nid_postfix(std::bit_cast<double>(input)));
        if (result != expected) {
            std::fprintf(stderr, "Guest sinh failed for %016llx: got %016llx, expected %016llx\n", static_cast<unsigned long long>(input), static_cast<unsigned long long>(result), static_cast<unsigned long long>(expected));
            std::abort();
        }
    }
}

static bool SinhWithinUlps(std::uint64_t result, std::uint64_t expected, std::uint64_t ulps) {
    return ((result ^ expected) >> 63) == 0 && (result > expected ? result - expected : expected - result) <= ulps;
}

static void CheckSinhLargeArguments() {
    constexpr std::uint64_t cases[][2] = {
        {0x4036000000000000ull, 0x41dab5adb9c43600ull}, {0x4036000000000001ull, 0x41dab5adb9c4361aull},
        {0x4059000000000000ull, 0x48e3494a9b171bf5ull}, {0x4062c00000000000ull, 0x4d652cac29822593ull},
        {0x40862e41ffffffffull, 0x7fdffc045692fc9eull}, {0x40862e4200000000ull, 0x7fdffc045693009eull},
        {0x40862e4200000001ull, 0x7fdffc045693049eull}, {0x40862e42ffffffffull, 0x7fe000020b8c410bull},
        {0x4086330000000000ull, 0x7fecee0e3df7d1f3ull},
        {0x408633cdffffffffull, 0x7feffdc12c49df1cull}, {0x408633ce00000000ull, 0x7feffdc12c49e31cull},
        {0x408633ce8fb9e87dull, 0x7fefffffffbffd3cull}, {0x408633ce8fba087dull, 0x7ff0000000000000ull},
        {0x408633ceffffffffull, 0x7ff0000000000000ull}, {0x408633cf00000000ull, 0x7ff0000000000000ull},
        {0x7fefffffffffffffull, 0x7ff0000000000000ull},
    };
    for (const auto& [input, expected] : cases) {
        for (const std::uint64_t sign : {std::uint64_t{0}, std::uint64_t{1} << 63}) {
            const std::uint64_t result = std::bit_cast<std::uint64_t>(sinh_nid_postfix(std::bit_cast<double>(input | sign)));
            const bool overflow = expected == 0x7ff0000000000000ull;
            if (overflow ? result != (expected | sign) : !SinhWithinUlps(result, expected | sign, 4)) {
                std::fprintf(stderr, "Guest sinh failed for %016llx: got %016llx, expected %016llx\n", static_cast<unsigned long long>(input | sign), static_cast<unsigned long long>(result), static_cast<unsigned long long>(expected | sign));
                std::abort();
            }
        }
    }
}

static void CheckSinhOverflowErrno() {
    constexpr int sentinel = 1234;
    for (const std::uint64_t sign : {std::uint64_t{0}, std::uint64_t{1} << 63}) {
        for (const std::uint64_t input : {0x408633cf00000000ull, 0x7fefffffffffffffull}) {
            errno = sentinel;
            const std::uint64_t result = std::bit_cast<std::uint64_t>(sinh_nid_postfix(std::bit_cast<double>(input | sign)));
            if (result != (0x7ff0000000000000ull | sign) || errno != ERANGE) {
                std::fprintf(stderr, "Guest sinh overflow failed for %016llx: got %016llx, errno %d\n", static_cast<unsigned long long>(input | sign), static_cast<unsigned long long>(result), errno);
                std::abort();
            }
        }
        for (const std::uint64_t input : {0x408633ce8fb9e87dull, 0x7ff0000000000000ull, 0x3ff0000000000000ull, 0ull}) {
            errno = sentinel;
            sinh_nid_postfix(std::bit_cast<double>(input | sign));
            if (errno != sentinel) {
                std::fprintf(stderr, "Guest sinh changed errno for %016llx: errno %d\n", static_cast<unsigned long long>(input | sign), errno);
                std::abort();
            }
        }
    }
}

static bool AsinhWithinUlps(std::uint64_t result, std::uint64_t expected, std::uint64_t ulps) {
    return ((result ^ expected) >> 63) == 0 && (result > expected ? result - expected : expected - result) <= ulps;
}

static void CheckAsinhMsunBits() {
    constexpr std::uint64_t cases[][2] = {
        {0x0000000000000000ull, 0x0000000000000000ull}, {0x0000000000000001ull, 0x0000000000000001ull},
        {0x000fffffffffffffull, 0x000fffffffffffffull}, {0x3c90000000000000ull, 0x3c90000000000000ull},
        {0x3e2ffffffffffffeull, 0x3e2ffffffffffffeull}, {0x3e2fffffffffffffull, 0x3e2fffffffffffffull},
        {0x3e30000000000000ull, 0x3e30000000000000ull}, {0x3e30000000000001ull, 0x3e30000000000001ull},
        {0x3fe0000000000000ull, 0x3fdecc2caec5160aull}, {0x3ff0000000000000ull, 0x3fec34366179d427ull},
        {0x3ffffffffffffffeull, 0x3ff719218313d086ull}, {0x3fffffffffffffffull, 0x3ff719218313d087ull},
        {0x4000000000000000ull, 0x3ff719218313d087ull}, {0x4000000000000001ull, 0x3ff719218313d088ull},
        {0x40000000ffffffffull, 0x3ff71922680cf8fbull},
        {0x40000000022b9b89ull, 0x3ff719218504c3dbull}, {0x400000003975b451ull, 0x3ff71921b67892afull},
        {0x41b0000027529ad0ull, 0x403419ecb987ee2bull}, {0x41b00000c4233f8eull, 0x403419ecc354f82full},
        {0x41b00000f8114bd4ull, 0x403419ecc693d8c6ull}, {0x3fc858aceb437920ull, 0x3fc833b16e1536cfull},
        {0x3fcfb2fb91be23b8ull, 0x3fcf623e1dc2a7a9ull}, {0x3fd3d86039728f38ull, 0x3fd38a4bb16cd986ull},
        {0x3fd4f018ee15f210ull, 0x3fd494d053c63f5aull}, {0x3fd604e5888b9750ull, 0x3fd59b3a3e84d0f4ull},
        {0x3fd7781dc03644bcull, 0x3fd6f902e56b239full}, {0x3fd93948915edbd0ull, 0x3fd89cd215ce473aull},
        {0x3fdbd5bf8a15f854ull, 0x3fdb0650596801feull}, {0x3fded53018738f74ull, 0x3fddbff057eac993ull},
        {0x3ff11a74b868ac6full, 0x3fedbcdc2899ca28ull}, {0x3ff3070681bd56f0ull, 0x3ff0251226fd5bb4ull},
        {0x3ff55fe87b9ca7c4ull, 0x3ff19a41c3590934ull},
    };
    for (const auto& [input, expected] : cases) {
        for (const std::uint64_t sign : {std::uint64_t{0}, std::uint64_t{1} << 63}) {
            const std::uint64_t result = std::bit_cast<std::uint64_t>(asinh_nid_postfix(std::bit_cast<double>(input | sign)));
            if (result != (expected | sign)) {
                std::fprintf(stderr, "Guest asinh failed for %016llx: got %016llx, expected %016llx\n", static_cast<unsigned long long>(input | sign), static_cast<unsigned long long>(result), static_cast<unsigned long long>(expected | sign));
                std::abort();
            }
        }
    }
}

static void CheckAsinhLargeArguments() {
    constexpr std::uint64_t cases[][2] = {
        {0x4000000100000000ull, 0x3ff71922680cf8fcull}, {0x4000000100000001ull, 0x3ff71922680cf8fdull},
        {0x4008000000000000ull, 0x3ffd185b507edc0eull}, {0x4024000000000000ull, 0x4007fc5c506d2bdbull},
        {0x408f400000000000ull, 0x401e67530a363e15ull}, {0x41affffffffffffeull, 0x403419ecb712c481ull},
        {0x41afffffffffffffull, 0x403419ecb712c481ull}, {0x41b0000000000000ull, 0x403419ecb712c481ull},
        {0x41b00000ffffffffull, 0x403419ecc712c401ull}, {0x41b0000100000000ull, 0x403419ecc712c400ull},
        {0x41b0000100000001ull, 0x403419ecc712c400ull}, {0x4202a05f20000000ull, 0x4037b810429a7c2aull},
        {0x54b249ad2594c37dull, 0x406cde73f84a4f2cull}, {0x7fefffffffffffffull, 0x408633ce8fb9f87dull},
    };
    for (const auto& [input, expected] : cases) {
        for (const std::uint64_t sign : {std::uint64_t{0}, std::uint64_t{1} << 63}) {
            const std::uint64_t result = std::bit_cast<std::uint64_t>(asinh_nid_postfix(std::bit_cast<double>(input | sign)));
            if (!AsinhWithinUlps(result, expected | sign, 4)) {
                std::fprintf(stderr, "Guest asinh failed for %016llx: got %016llx, expected %016llx\n", static_cast<unsigned long long>(input | sign), static_cast<unsigned long long>(result), static_cast<unsigned long long>(expected | sign));
                std::abort();
            }
        }
    }
    Require(asinh_nid_postfix(std::numeric_limits<double>::infinity()) == std::numeric_limits<double>::infinity());
    Require(asinh_nid_postfix(-std::numeric_limits<double>::infinity()) == -std::numeric_limits<double>::infinity());
    Require(std::isnan(asinh_nid_postfix(std::numeric_limits<double>::quiet_NaN())));
}

static void CheckRoundingNaNConsoleBits() {
    constexpr std::uint64_t cases[][2] = {
        {0x7ff0000000000001ull, 0x7ff8000000000001ull}, {0xfff4000000000123ull, 0xfffc000000000123ull}, {0x7ff8000000000456ull, 0x7ff8000000000456ull},
        {0x7ff0000000000000ull, 0x7ff0000000000000ull}, {0xfff0000000000000ull, 0xfff0000000000000ull},
    };
    constexpr std::uint32_t floatCases[][2] = {
        {0x7f800001u, 0x7fc00001u}, {0xffa00123u, 0xffe00123u}, {0x7fc00456u, 0x7fc00456u}, {0x7f800000u, 0x7f800000u}, {0xff800000u, 0xff800000u},
    };
    for (const auto function : {trunc_nid_postfix, ceil_nid_postfix, floor_nid_postfix, nearbyint_nid_postfix}) {
        for (const auto& [input, expected] : cases) {
            Require(std::bit_cast<std::uint64_t>(function(std::bit_cast<double>(input))) == expected);
        }
    }
    for (const auto function : {truncf_nid_postfix, ceilf_nid_postfix, floorf_nid_postfix, nearbyintf_nid_postfix}) {
        for (const auto& [input, expected] : floatCases) {
            Require(std::bit_cast<std::uint32_t>(function(std::bit_cast<float>(input))) == expected);
        }
    }
}

template<typename T, typename UInt>
static void CheckNextafterEnvironment(T (APS5_VABI *next)(T, T), UInt sign, UInt one, UInt minimum, UInt maximum, UInt infinity) {
    const RestoreFloatingPointState saved;
    const auto savedControl = _mm_getcsr();
    const struct {
        UInt x, y, normalResult, dazResult;
        unsigned normalFlags, dazFlags;
        int normalError, dazError;
    } cases[] = {
        {0, one, 1, 0, 0x32, 0, 97, 97},
        {sign, sign | one, sign | 1, 0, 0x32, 0, 97, 97},
        {1, 0, 0, 0, 0x32, 0, ERANGE, 97},
        {1, one, 2, 2, 0x32, 0, ERANGE, 97},
        {sign | 1, sign | one, sign | 2, sign | 2, 0x32, 0, ERANGE, 97},
        {minimum, 0, minimum - 1, minimum - 1, 0x30, 0x30, ERANGE, ERANGE},
        {maximum, infinity, infinity, infinity, 0x28, 0x28, ERANGE, ERANGE},
        {1, 1, 1, 1, 0x02, 0, 97, 97},
        {one, one + 1, one + 1, one + 1, 0, 0, 97, 97},
    };
    for (unsigned denorm = 0; denorm != 4; ++denorm) {
        const unsigned control = 0x1f80u | ((denorm & 1u) ? 0x40u : 0u) | ((denorm & 2u) ? 0x8000u : 0u);
        const bool daz = (denorm & 1u) != 0;
        for (const auto& item : cases) {
            Require(std::fesetround(FE_TONEAREST) == 0);
            Require(std::feclearexcept(FE_ALL_EXCEPT) == 0);
            errno = 97;
            _mm_setcsr(control);
            const auto result = std::bit_cast<UInt>(next(std::bit_cast<T>(item.x), std::bit_cast<T>(item.y)));
            const auto flags = _mm_getcsr() & 0x3fu;
            const int exceptions = std::fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT);
            const int error = errno;
            const auto expectedFlags = daz ? item.dazFlags : item.normalFlags;
            Require(result == (daz ? item.dazResult : item.normalResult));
            Require(flags == expectedFlags);
            const int expectedExceptions = ((expectedFlags & 0x01u) ? FE_INVALID : 0) |
                                           ((expectedFlags & 0x04u) ? FE_DIVBYZERO : 0) |
                                           ((expectedFlags & 0x08u) ? FE_OVERFLOW : 0) |
                                           ((expectedFlags & 0x10u) ? FE_UNDERFLOW : 0) |
                                           ((expectedFlags & 0x20u) ? FE_INEXACT : 0);
            Require(exceptions == expectedExceptions);
            Require(error == (daz ? item.dazError : item.normalError));
        }
    }
    constexpr int rounding[] = {FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    for (const int mode : rounding) {
        Require(std::fesetround(mode) == 0);
        const unsigned control = (_mm_getcsr() & 0x6000u) | 0x1f80u;
        errno = 97;
        Require(std::feclearexcept(FE_ALL_EXCEPT) == 0);
        _mm_setcsr(control);
        Require(std::bit_cast<UInt>(next(std::bit_cast<T>(maximum), std::bit_cast<T>(infinity))) == (mode == FE_UPWARD ? infinity : maximum));
        Require((_mm_getcsr() & 0x3fu) == 0x28u);
        Require(errno == ERANGE);
        errno = 97;
        Require(std::feclearexcept(FE_ALL_EXCEPT) == 0);
        _mm_setcsr(control);
        Require(std::bit_cast<UInt>(next(std::bit_cast<T>(UInt{1}), std::bit_cast<T>(one))) == 2);
        Require((_mm_getcsr() & 0x3fu) == 0x32u);
        Require(errno == (mode == FE_UPWARD ? 97 : ERANGE));
    }
    _mm_setcsr(savedControl);
}

static void CheckExpm1ScalingRounding() {
    const RestoreFloatingPointState saved;
    const unsigned savedCsr = _mm_getcsr();
    const struct {
        std::uint64_t input;
        std::uint64_t result[4];
    } cases[] = {
        {0x407ff33f4d52628cull, {0x6e06c8c735268691ull, 0x6e06c8c73526868full, 0x6e06c8c735268691ull, 0x6e06c8c735268690ull}},
        {0x407eff5467ccdb38ull, {0x6ca6e270fe0a802eull, 0x6ca6e270fe0a802cull, 0x6ca6e270fe0a802eull, 0x6ca6e270fe0a802cull}},
    };
    const int rounding[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    double (APS5_VABI * volatile call)(double) = expm1_nid_postfix;
    for (unsigned mode = 0; mode != 4; ++mode) {
        for (unsigned denorm = 0; denorm != 4; ++denorm) {
            const unsigned control = 0x1f80u | (mode << 13u) | ((denorm & 1u) ? 0x40u : 0u) | ((denorm & 2u) ? 0x8000u : 0u);
            for (std::uint16_t sticky : {0u, 5u, 32u}) {
                for (const auto& item : cases) {
                    Require(std::fesetenv(FE_DFL_ENV) == 0 && std::fesetround(rounding[mode]) == 0);
                    Require(fesetexceptflag_nid_postfix(&sticky, 0x3f) == 0);
                    _mm_setcsr(control | sticky);
                    GuestFloatingPointEnvironment before;
                    Require(fegetenv_nid_postfix(&before) == 0 && (before.status & 0x3fu) == sticky);
                    errno = 1234;
                    const auto result = std::bit_cast<std::uint64_t>(call(std::bit_cast<double>(item.input)));
                    const int error = errno;
                    GuestFloatingPointEnvironment after;
                    Require(fegetenv_nid_postfix(&after) == 0);
                    if (result != item.result[mode] || after.mxcsr != (control | sticky | 0x20u) || (after.status & 0x3fu) != sticky || after.control != before.control || error != 1234) {
                        std::fprintf(stderr, "expm1 scaling: input=%016llx mode=%u denorm=%u sticky=%u bits=%016llx csr=%x x87=%x errno=%d\n", static_cast<unsigned long long>(item.input), mode, denorm, sticky, static_cast<unsigned long long>(result), after.mxcsr, after.status, error);
                        Require(false);
                    }
                }
            }
        }
    }
    _mm_setcsr(savedCsr);
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

static void CheckCbrtfConsoleBits() {
    constexpr std::uint32_t cases[][2] = {
        {0x00000123u, 0x28059605u}, {0x00400327u, 0x2a4b334bu}, {0x3f000555u, 0x3f4b32c7u}, {0x3fc00778u, 0x3f9287e5u},
        {0x42000999u, 0x404b3509u}, {0x7e000bbcu, 0x544b362au}, {0xbf400dddu, 0xbf689d01u}, {0x80201002u, 0xaa215ff8u},
        {0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u}, {0x00000001u, 0x26a14518u}, {0x007fffffu, 0x2a7fffffu},
        {0x00800000u, 0x2a800000u}, {0x3f800000u, 0x3f800000u}, {0x41000000u, 0x40000000u}, {0xc1000000u, 0xc0000000u},
        {0x7f7fffffu, 0x54cb2ff5u}, {0x7f800000u, 0x7f800000u}, {0xff800000u, 0xff800000u}, {0x7fc00001u, 0x7fc00001u},
        {0xffc00002u, 0xffc00002u},
    };
    for (const auto& [input, expected] : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(cbrtf_nid_postfix(std::bit_cast<float>(input))) == expected && errno == 1234);
    }
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
        {" -0xz!", 0, 0, 0, 0},
        {"0xg", 16, 0, 0, 0},
        {"0x", 16, 0, 0, 0},
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
        {" +0Xg!", 0, 0, 0, 0},
        {"0x!", 16, 0, 0, 0},
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
    for (const int base : {-1, 1, 37}) {
        const char text[] = " 12";
        const auto rejects = [&](auto parse, int error) {
            char* end = nullptr;
            *__error_nid_postfix() = 0;
            Require(parse(text, &end, base) == 0 && end == text && *__error_nid_postfix() == error);
        };
        rejects(strtol_nid_postfix, EINVAL);
        rejects(strtoul_nid_postfix, EINVAL);
        rejects(strtoll_nid_postfix, EINVAL);
        rejects(strtoull_nid_postfix, EINVAL);
        rejects(strtoimax_nid_postfix, 0);
        rejects(strtoumax_nid_postfix, 0);
        rejects(_Stoull_nid_postfix, 0);
        rejects(_Stoul_nid_postfix, 0);
    }
    *__error_nid_postfix() = 0;
    Require(atoi_nid_postfix("9223372036854775808") == 0 && atoi_nid_postfix("4294967297") == 1 && atoi_nid_postfix("-9223372036854775809") == -1);
    Require(atol_nid_postfix("9223372036854775808") == INT64_MIN && atoll_nid_postfix("-9223372036854775809") == INT64_MAX && *__error_nid_postfix() == 0);
    Require(atol_nid_postfix("99999999999999999999") == -1 && *__error_nid_postfix() == ERANGE);
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

static void CheckTrigfConsoleBits() {
    constexpr struct {
        std::uint32_t input;
        std::uint32_t sine;
        std::uint32_t cosine;
        std::uint32_t tangent;
        int error;
    } cases[] = {
        {0x3e801d5au, 0x3e7d9057u, 0x3f780703u, 0x3e82db72u, 0}, {0x3fc00321u, 0x3f7f5c46u, 0x3d90acbbu, 0x4161edb0u, 0},
        {0x40400556u, 0x3e102d3eu, 0xbf7d7328u, 0xbe11a09du, 0}, {0x40c00781u, 0xbe8e9c41u, 0x3f75de78u, 0xbe947c92u, 0},
        {0x412009acu, 0xbf0bc6b7u, 0xbf56790du, 0x3f26d71eu, 0}, {0x47000bbbu, 0x3eb0df2fu, 0x3f703d5au, 0x3ebc799eu, 0},
        {0x4e000de0u, 0xbf664722u, 0x3edfade4u, 0xc003c6abu, 0}, {0x5f001006u, 0xbe0ecf11u, 0x3f7d7f8cu, 0xbe1037deu, 0},
        {0x7e001112u, 0x3eabd0cbu, 0x3f7127beu, 0x3eb6645du, 0}, {0xc0400556u, 0xbe102d3eu, 0xbf7d7328u, 0x3e11a09du, 0},
        {0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0}, {0x80000000u, 0x80000000u, 0x3f800000u, 0x80000000u, 0},
        {0x39000000u, 0x39000000u, 0x3f800000u, 0x39000000u, 0}, {0x3f490fdbu, 0x3f3504f3u, 0x3f3504f3u, 0x3f800000u, 0},
        {0x40490fdbu, 0xb3bbbd2eu, 0xbf800000u, 0x33bbbd2fu, 0}, {0x4dc90fdbu, 0xbf3d5f98u, 0x3f2c42b6u, 0xbf8cb742u, 0},
        {0x7f7fffffu, 0xbf0599b3u, 0x3f5a5f96u, 0xbf1c9ecau, 0}, {0xff7fffffu, 0x3f0599b3u, 0x3f5a5f96u, 0x3f1c9ecau, 0},
        {0x7f800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, EDOM}, {0xff800000u, 0xffc00000u, 0xffc00000u, 0xffc00000u, EDOM},
        {0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0x7fc00001u, 0},
    };
    for (const auto& item : cases) {
        const float x = std::bit_cast<float>(item.input);
        const int expected = item.error != 0 ? item.error : 1234;
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(sinf_nid_postfix(x)) == item.sine && errno == expected);
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(cosf_nid_postfix(x)) == item.cosine && errno == expected);
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(tanf_nid_postfix(x)) == item.tangent && errno == expected);
        float sine = 0.0f;
        float cosine = 0.0f;
        sincosf_nid_postfix(x, &sine, &cosine);

        Require(std::bit_cast<std::uint32_t>(sine) == item.sine && std::bit_cast<std::uint32_t>(cosine) == item.cosine);
    }
}

static void CheckRemainderf() {
    Require(remainderf_nid_postfix(5.0f, 2.0f) == 1.0f && remainderf_nid_postfix(7.0f, 2.0f) == -1.0f);
    Require(remainderf_nid_postfix(-5.0f, 2.0f) == -1.0f && remainderf_nid_postfix(5.5f, 2.0f) == -0.5f);
    Require(remainderf_nid_postfix(6.0f, 3.0f) == 0.0f && !std::signbit(remainderf_nid_postfix(6.0f, 3.0f)));
    Require(remainderf_nid_postfix(-6.0f, 3.0f) == 0.0f && std::signbit(remainderf_nid_postfix(-6.0f, 3.0f)));
    Require(remainderf_nid_postfix(3.0f, INFINITY) == 3.0f);
    Require(std::isnan(remainderf_nid_postfix(1.0f, 0.0f)) && std::isnan(remainderf_nid_postfix(INFINITY, 2.0f)));
    Require(std::isnan(remainderf_nid_postfix(NAN, 2.0f)) && std::isnan(remainderf_nid_postfix(2.0f, NAN)));
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

static void CheckExp2fConsoleBits() {
    constexpr struct {
        std::uint32_t input;
        std::uint32_t expected;
        int error;
    } cases[] = {
        {0x3d00010bu, 0x3f82cd8du, 0}, {0x3f000303u, 0x3fb5066du, 0}, {0x40a0051bu, 0x42000e29u, 0}, {0x42f0070cu, 0x7b813a1bu, 0},
        {0xbf400909u, 0x3f183437u, 0}, {0xc2f00b14u, 0x037c3073u, 0}, {0x00000000u, 0x3f800000u, 0}, {0x80000000u, 0x3f800000u, 0},
        {0x33000000u, 0x3f800000u, 0}, {0x3f800000u, 0x40000000u, 0}, {0x42fe0000u, 0x7f000000u, 0}, {0x42ffffffu, 0x7f7fffa7u, 0},
        {0x43000000u, 0x7f800000u, ERANGE}, {0x7f7fffffu, 0x7f800000u, ERANGE}, {0x7f800000u, 0x7f800000u, 0}, {0xff800000u, 0x00000000u, 0},
        {0xc2fc0000u, 0x00800000u, 0}, {0xc3150000u, 0x00000001u, 0}, {0xc315ffffu, 0x00000001u, 0}, {0xc3160000u, 0x00000000u, ERANGE},
        {0xc3160001u, 0x00000000u, ERANGE}, {0xff7fffffu, 0x00000000u, ERANGE}, {0x7fc00001u, 0x7fc00001u, 0}, {0xffc00002u, 0xffc00002u, 0},
    };
    for (const auto& item : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(exp2f_nid_postfix(std::bit_cast<float>(item.input))) == item.expected);
        Require(errno == (item.error != 0 ? item.error : 1234));
    }
}

static void CheckHyperbolic() {
    const double infinity = std::numeric_limits<double>::infinity();
    Require(_Cosh_nid_postfix(0.0, 2.0) == 2.0);
    Require(Near(_Cosh_nid_postfix(1.0, 2.0), 3.0861612696304874, 1e-15));
    Require(Near(_Cosh_nid_postfix(-1.0, -2.0), -3.0861612696304874, 1e-15));
    Require(std::isinf(_Cosh_nid_postfix(infinity, -1.0)) && std::signbit(_Cosh_nid_postfix(infinity, -1.0)));
    Require(std::isnan(_Cosh_nid_postfix(infinity, 0.0)));
    Require(Near(_Cosh_nid_postfix(711.0, 1e-300), 0x1.2190c08e2f1a4p+28, 1e-15));
    Require(_Cosh_nid_postfix(720.0, 0.0) == 0.0 && !std::signbit(_Cosh_nid_postfix(720.0, 0.0)));
    Require(_Cosh_nid_postfix(720.0, -0.0) == 0.0 && std::signbit(_Cosh_nid_postfix(720.0, -0.0)));
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

static void CheckAcoshfDomainEnvironment() {
    const RestoreFloatingPointState saved;
    const unsigned savedCsr = _mm_getcsr();
    const struct {
        std::uint32_t input;
        std::uint32_t result;
        unsigned raised;
        unsigned x87Raised;
        int error;
    } cases[] = {
        {0x00000000u, 0xffc00000u, 1u, 1u, EDOM},
        {0x80000000u, 0xffc00000u, 1u, 1u, EDOM},
        {0x3f000000u, 0xffc00000u, 1u, 1u, EDOM},
        {0x3f7fffffu, 0xffc00000u, 1u, 1u, EDOM},
        {0xbf800000u, 0xffc00000u, 1u, 1u, EDOM},
        {0xff800000u, 0xffc00000u, 1u, 1u, EDOM},
        {0xffc00001u, 0xffc00001u, 1u, 1u, EDOM},
        {0xff800001u, 0xffc00001u, 1u, 1u, EDOM},
        {0x3f800000u, 0x00000000u, 0u, 0u, 1234},
        {0x7f800000u, 0x7f800000u, 0u, 0u, 1234},
        {0x7fc00001u, 0x7fc00001u, 0u, 0u, 1234},
        {0x7f800001u, 0x7fc00001u, 1u, 0u, 1234},
    };
    const int rounding[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    float (APS5_VABI * volatile call)(float) = acoshf_nid_postfix;
    for (unsigned mode = 0; mode != 4; ++mode) {
        for (unsigned denorm = 0; denorm != 4; ++denorm) {
            const unsigned control = 0x1f80u | (mode << 13u) | ((denorm & 1u) ? 0x40u : 0u) | ((denorm & 2u) ? 0x8000u : 0u);
            for (std::uint16_t sticky : {0u, 5u, 32u}) {
                for (const auto& item : cases) {
                    Require(std::fesetenv(FE_DFL_ENV) == 0 && std::fesetround(rounding[mode]) == 0);
                    Require(fesetexceptflag_nid_postfix(&sticky, 0x3f) == 0);
                    _mm_setcsr(control | sticky);
                    GuestFloatingPointEnvironment before;
                    Require(fegetenv_nid_postfix(&before) == 0 && (before.status & 0x3fu) == sticky);
                    errno = 1234;
                    const auto result = std::bit_cast<std::uint32_t>(call(std::bit_cast<float>(item.input)));
                    const int error = errno;
                    GuestFloatingPointEnvironment after;
                    Require(fegetenv_nid_postfix(&after) == 0);
                    if (result != item.result || after.mxcsr != (control | sticky | item.raised) || (after.status & 0x3fu) != (sticky | item.x87Raised) || after.control != before.control || error != item.error) {
                        std::fprintf(stderr, "acoshf domain: input=%08llx mode=%u denorm=%u sticky=%u bits=%08llx csr=%x x87=%x errno=%d\n", static_cast<unsigned long long>(item.input), mode, denorm, sticky, static_cast<unsigned long long>(result), after.mxcsr, after.status, error);
                        Require(false);
                    }
                }
            }
        }
    }
    _mm_setcsr(savedCsr);
}

static void CheckAcoshfMsunBoundaries() {
    Require(acoshf_nid_postfix(1.0f) == 0.0f && !std::signbit(acoshf_nid_postfix(1.0f)));
    errno = EINTR;
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(std::isnan(acoshf_nid_postfix(0.5f)) && errno == EDOM && (std::fetestexcept(FE_INVALID) & FE_INVALID) != 0);
    for (const std::uint32_t bits : {0x3f800001u, 0x3fffffffu, 0x40000000u, 0x40000001u,
                                     0x4d7fffffu, 0x4d800000u, 0x4d800001u, 0x7f7fffffu}) {
        const float x = std::bit_cast<float>(bits);
        Require(Near(acoshf_nid_postfix(x), std::acosh(static_cast<double>(x)), 1e-6));
    }
    Require(std::isinf(acoshf_nid_postfix(INFINITY)) && acoshf_nid_postfix(INFINITY) > 0.0f);
    Require(std::isnan(acoshf_nid_postfix(-INFINITY)));
    Require(std::isnan(acoshf_nid_postfix(std::numeric_limits<float>::quiet_NaN())));
    const RestoreFloatingPointState saved;
    const unsigned savedCsr = _mm_getcsr();
    const struct {
        std::uint32_t input;
        std::uint32_t result[4];
        unsigned raised;
    } cases[] = {
        {0x3f800000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, 0u},
        {0x3f800001u, {0x39ffffffu, 0x39ffffffu, 0x3a000001u, 0x39ffffffu}, 32u},
        {0x3fffffffu, {0x3fa89213u, 0x3fa89212u, 0x3fa89214u, 0x3fa89212u}, 32u},
        {0x40000000u, {0x3fa89214u, 0x3fa89213u, 0x3fa89214u, 0x3fa89213u}, 32u},
        {0x40000001u, {0x3fa89215u, 0x3fa89214u, 0x3fa89215u, 0x3fa89214u}, 32u},
        {0x40000002u, {0x3fa89216u, 0x3fa89215u, 0x3fa89217u, 0x3fa89215u}, 32u},
        {0x40000003u, {0x3fa89217u, 0x3fa89217u, 0x3fa89218u, 0x3fa89217u}, 32u},
        {0x4d7fffffu, {0x41a0cf66u, 0x41a0cf65u, 0x41a0cf66u, 0x41a0cf65u}, 32u},
        {0x4d800000u, {0x41a0cf66u, 0x41a0cf64u, 0x41a0cf66u, 0x41a0cf64u}, 32u},
        {0x4d800001u, {0x41a0cf66u, 0x41a0cf65u, 0x41a0cf67u, 0x41a0cf65u}, 32u},
        {0x4d8050f2u, {0x41a0d473u, 0x41a0d472u, 0x41a0d474u, 0x41a0d472u}, 32u},
        {0x7f7fffffu, {0x42b2d4fcu, 0x42b2d4fbu, 0x42b2d4fdu, 0x42b2d4fbu}, 32u},
    };
    const int rounding[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    float (APS5_VABI * volatile call)(float) = acoshf_nid_postfix;
    for (unsigned mode = 0; mode != 4; ++mode) {
        for (unsigned denorm = 0; denorm != 4; ++denorm) {
            const unsigned control = 0x1f80u | (mode << 13u) | ((denorm & 1u) ? 0x40u : 0u) | ((denorm & 2u) ? 0x8000u : 0u);
            for (std::uint16_t sticky : {0u, 5u, 32u}) {
                for (const auto& item : cases) {
                    Require(std::fesetenv(FE_DFL_ENV) == 0 && std::fesetround(rounding[mode]) == 0);
                    Require(fesetexceptflag_nid_postfix(&sticky, 0x3f) == 0);
                    _mm_setcsr(control | sticky);
                    GuestFloatingPointEnvironment before;
                    Require(fegetenv_nid_postfix(&before) == 0 && (before.status & 0x3fu) == sticky);
                    errno = 1234;
                    const auto result = std::bit_cast<std::uint32_t>(call(std::bit_cast<float>(item.input)));
                    const int error = errno;
                    GuestFloatingPointEnvironment after;
                    Require(fegetenv_nid_postfix(&after) == 0);
                    if (result != item.result[mode] || after.mxcsr != (control | sticky | item.raised) || (after.status & 0x3fu) != sticky || after.control != before.control || error != 1234) {
                        std::fprintf(stderr, "acoshf positive: input=%08llx mode=%u denorm=%u sticky=%u bits=%08llx expected=%08llx csr=%x x87=%x errno=%d\n", static_cast<unsigned long long>(item.input), mode, denorm, sticky, static_cast<unsigned long long>(result), static_cast<unsigned long long>(item.result[mode]), after.mxcsr, after.status, error);
                        Require(false);
                    }
                }
            }
        }
    }
    _mm_setcsr(savedCsr);

}

static void CheckCoshfMsunBoundaries() {
    static constexpr struct {
        std::uint32_t input;
        std::uint32_t output;
    } cases[] = {
        {0x00000000u, 0x3f800000u},
        {0x80000000u, 0x3f800000u},
        {0x00000001u, 0x3f800000u},
        {0x397fffffu, 0x3f800000u},
        {0x39800000u, 0x3f800000u},
        {0x3eb17217u, 0x3f87c3b6u},
        {0x3eb17218u, 0x3f87c3b6u},
        {0x3f800000u, 0x3fc583aau},
        {0xbf800000u, 0x3fc583aau},
        {0x410fffffu, 0x457d389cu},
        {0x41100000u, 0x457d38acu},
        {0x42b17216u, 0x7effff04u},
        {0x42b17217u, 0x7effff84u},
        {0x42b17218u, 0x7f000002u},
        {0x42b2d4fbu, 0x7f7fff6cu},
        {0x42b2d4fcu, 0x7f7fffecu},
        {0xc2b2d4fcu, 0x7f7fffecu},
    };
    for (const auto& test : cases)
        Require(std::bit_cast<std::uint32_t>(coshf_nid_postfix(std::bit_cast<float>(test.input))) == test.output);
    Require(std::bit_cast<std::uint32_t>(coshf_nid_postfix(std::bit_cast<float>(0x42b2d4fdu))) == 0x7f800000u);
    Require(std::bit_cast<std::uint32_t>(coshf_nid_postfix(INFINITY)) == 0x7f800000u);
    Require(std::bit_cast<std::uint32_t>(coshf_nid_postfix(-INFINITY)) == 0x7f800000u);
    Require(std::bit_cast<std::uint32_t>(coshf_nid_postfix(std::bit_cast<float>(0x7fc12345u))) == 0x7fc12345u);
}

static void CheckScaledHyperbolic() {
    const int rounding = std::fegetround();
    Require(std::fesetround(FE_TONEAREST) == 0);
    unsigned checks = 0;
    for (bool sine : {true, false}) {
        const auto function = sine ? _FSinh_nid_postfix : _FCosh_nid_postfix;
        const char* name = sine ? "_FSinh" : "_FCosh";
        const auto checkValue = [&](float x, float y) {
            const long double value = sine ? std::sinh(static_cast<long double>(x)) : std::cosh(static_cast<long double>(x));
            const float expected = static_cast<float>(value * static_cast<long double>(y));
            std::feclearexcept(FE_ALL_EXCEPT);
            errno = 12345;
            const float result = function(x, y);
            const auto actualBits = std::bit_cast<std::uint32_t>(result);
            const auto expectedBits = std::bit_cast<std::uint32_t>(expected);
            const auto distance = actualBits > expectedBits ? actualBits - expectedBits : expectedBits - actualBits;
            const bool exact = expected == 0.0f || std::isinf(expected);
            if ((exact ? actualBits != expectedBits : !std::isfinite(result) || distance > 2) ||
                (std::isfinite(expected) && (std::fetestexcept(FE_OVERFLOW | FE_INVALID) != 0 || errno != 12345))) {
                std::fprintf(stderr, "%s(%a, %a): got %08x, expected %08x, errno %d, flags %x\n",
                    name, static_cast<double>(x), static_cast<double>(y), actualBits, expectedBits, errno, std::fetestexcept(StandardExceptions));
                std::abort();
            }
            ++checks;
        };
        checkValue(90.0f, 1e-20f);
        checkValue(100.0f, 1e-40f);
        const float magnitudes[] = {0.0f, 0x1p-126f, 0.001f, 0.5f, 1.0f, 4.0f, 7.0f, 8.0f, 10.0f, 20.0f, 50.0f, 88.0f, 90.0f, 100.0f, 150.0f, 180.0f, 190.0f, 193.0f};
        const float scales[] = {0.0f, 0x1p-149f, 0x1p-140f, 0x1p-126f, 1e-20f, 0.5f, 1.0f, 1e20f, std::numeric_limits<float>::max()};
        for (float magnitude : magnitudes) {
            for (float scale : scales) {
                for (float xSign : {1.0f, -1.0f}) {
                    for (float ySign : {1.0f, -1.0f}) checkValue(magnitude * xSign, scale * ySign);
                }
            }
        }
        for (float scale : scales) {
            if (scale == 0.0f) continue;
            for (long double target : {static_cast<long double>(std::numeric_limits<float>::max()), 0x1p-150L}) {
                if (!sine && target < scale) continue;
                const long double ratio = target / static_cast<long double>(scale);
                const float boundary = static_cast<float>(sine ? std::asinh(ratio) : std::acosh(ratio));
                float magnitude = std::nextafter(std::nextafter(boundary, -INFINITY), -INFINITY);
                for (unsigned neighbor = 0; neighbor != 5; ++neighbor) {
                    for (float xSign : {1.0f, -1.0f}) {
                        for (float ySign : {1.0f, -1.0f}) checkValue(magnitude * xSign, scale * ySign);
                    }
                    magnitude = std::nextafter(magnitude, INFINITY);
                }
            }
        }
        const auto checkSpecial = [&](float x, float y, std::uint32_t expectedBits, int error, int flags) {
            std::feclearexcept(FE_ALL_EXCEPT);
            std::feraiseexcept(FE_DIVBYZERO);
            errno = 12345;
            const float result = function(x, y);
            const int raised = std::fetestexcept(StandardExceptions);
            if (std::bit_cast<std::uint32_t>(result) != expectedBits || errno != error || raised != (FE_DIVBYZERO | flags)) {
                std::fprintf(stderr, "%s special (%a, %a): got %08x, expected %08x, errno %d, flags %x\n",
                    name, static_cast<double>(x), static_cast<double>(y), std::bit_cast<std::uint32_t>(result), expectedBits, errno, raised);
                std::abort();
            }
            ++checks;
        };
        const float infinity = std::numeric_limits<float>::infinity();
        for (float sign : {1.0f, -1.0f}) {
            checkSpecial(sign * infinity, 0.0f, 0x7fc00000u, EDOM, FE_INVALID);
            checkSpecial(sign * 100.0f, 0.0f, sine && sign < 0 ? 0x80000000u : 0u, 12345, 0);
            checkSpecial(sign * 100.0f, -0.0f, sine && sign < 0 ? 0u : 0x80000000u, 12345, 0);
            checkSpecial(sign * 1.0f, infinity, sine && sign < 0 ? 0xff800000u : 0x7f800000u, 12345, 0);
        }
        for (std::uint32_t bits : {0x7fc12345u, 0xffc54321u, 0x7fa12345u, 0xffa54321u}) {
            const float nan = std::bit_cast<float>(bits);
            checkSpecial(nan, 0.0f, bits, 12345, 0);
            checkSpecial(infinity, nan, bits, 12345, 0);
            checkSpecial(nan, std::bit_cast<float>(0x7fc99999u), bits, 12345, 0);
        }
        checkSpecial(0.0f, infinity, sine ? 0x7fc00000u : 0x7f800000u, sine ? EDOM : 12345, sine ? FE_INVALID : 0);
        checkSpecial(1.0f, std::numeric_limits<float>::max(), 0x7f800000u, 12345, FE_OVERFLOW | FE_INEXACT);
        checkSpecial(100.0f, 1.0f, 0x7f800000u, ERANGE, FE_OVERFLOW | FE_INEXACT);
        for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
            Require(std::fesetround(mode) == 0);
            checkSpecial(std::numeric_limits<float>::max(), 0x1p-149f, 0x7f800000u, ERANGE, FE_OVERFLOW | FE_INEXACT);
        }
        Require(std::fesetround(FE_TONEAREST) == 0);
        if (sine) checkSpecial(0x1p-126f, 0x1p-149f, 0u, 12345, FE_UNDERFLOW | FE_INEXACT);
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(std::fesetround(rounding) == 0);
    std::printf("Scaled hyperbolic checks: %u\n", checks);
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

static void CheckAtanhf() {
    const struct { std::uint32_t input; std::uint32_t output; } cases[] = {
        {0x00000000u, 0x00000000u}, {0x80000000u, 0x80000000u}, {0x00000001u, 0x00000001u}, {0x0da24260u, 0x0da24260u},
        {0x31000000u, 0x31000000u}, {0xb1000000u, 0xb1000000u}, {0x31800000u, 0x31800000u}, {0x35800000u, 0x35800000u},
        {0x3dcccccdu, 0x3dcd7c9eu}, {0x3e800000u, 0x3e82c578u}, {0xbe800000u, 0xbe82c578u}, {0x3eaaaaaau, 0x3eb17217u},
        {0x3eaaaaabu, 0x3eb17218u}, {0x3effffffu, 0x3f0c9f53u}, {0x3f000000u, 0x3f0c9f54u}, {0xbf000000u, 0xbf0c9f54u},
        {0x3f199999u, 0x3f317217u}, {0x3f19999au, 0x3f317218u}, {0x3f400000u, 0x3f791396u}, {0xbf400000u, 0xbf791396u},
        {0x3f666666u, 0x3fbc71afu}, {0x3f7fffffu, 0x410aa123u}, {0xbf7fffffu, 0xc10aa123u},
    };
    for (const auto& test : cases) {
        *__error_nid_postfix() = 12345;
        const float result = atanhf_nid_postfix(std::bit_cast<float>(test.input));
        if (std::bit_cast<std::uint32_t>(result) != test.output || *__error_nid_postfix() != 12345) {
            std::fprintf(stderr, "Guest atanhf failed for %08x\n", test.input);
            std::abort();
        }
    }
    for (const std::uint32_t input : {0x3f800000u, 0xbf800000u}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        *__error_nid_postfix() = 0;
        const float result = atanhf_nid_postfix(std::bit_cast<float>(input));
        Require(std::bit_cast<std::uint32_t>(result) == (input | 0x7f800000u) && *__error_nid_postfix() == ERANGE);
        Require(std::fetestexcept(FE_DIVBYZERO) != 0);
    }
    for (const std::uint32_t input : {0x3f800001u, 0xbf800001u, 0x3fc00000u, 0xbfc00000u, 0x7f7fffffu, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0xffc00000u}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        *__error_nid_postfix() = 0;
        Require(std::isnan(atanhf_nid_postfix(std::bit_cast<float>(input))) && *__error_nid_postfix() == EDOM);
        Require(std::fetestexcept(FE_INVALID) != 0);
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
        if (__fpclassifyf_nid_postfix(value) != test.code) {
            std::fprintf(stderr, "Guest __fpclassifyf failed for %08x\n", test.bits);
            std::abort();
        }
    }
    Require(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FInf_nid_postfix)) == 1);
    Require(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FNan_nid_postfix)) == 2);
}

static void RequireBytes(const void* constant, std::initializer_list<unsigned char> expected) {
    unsigned char bytes[16];
    std::memcpy(bytes, constant, sizeof(bytes));
    int index = 0;
    for (const unsigned char byte : expected) Require(bytes[index++] == byte);
    while (index < 16) Require(bytes[index++] == 0);
}

static void CheckLogfConsoleBits() {
    constexpr struct {
        std::uint32_t input;
        std::uint32_t expected;
        int error;
    } cases[] = {
        {0x00000983u, 0xc2bef664u, 0}, {0x3e003791u, 0xc004f9cfu, 0}, {0x3f7f0a10u, 0xbb76666fu, 0}, {0x3f81086bu, 0x3c03adacu, 0},
        {0x40400de0u, 0x3f8ca894u, 0}, {0x5f00a041u, 0x422eb14eu, 0}, {0x3f800000u, 0x00000000u, 0}, {0x40000000u, 0x3f317218u, 0},
        {0x00000001u, 0xc2ce8ed0u, 0}, {0x00800000u, 0xc2aeac50u, 0}, {0x7f7fffffu, 0x42b17218u, 0}, {0x7f800000u, 0x7f800000u, 0},
        {0x3f800001u, 0x33ffffffu, 0}, {0x3f7fffffu, 0xb3800000u, 0}, {0x7fc00001u, 0x7fc00001u, 0},
        {0x00000000u, 0xff800000u, ERANGE}, {0x80000000u, 0xff800000u, ERANGE}, {0x80000001u, 0xffc00000u, EDOM},
        {0xbf800000u, 0xffc00000u, EDOM}, {0xff800000u, 0xffc00000u, EDOM}, {0xffc00002u, 0xffc00002u, EDOM},
    };
    for (const auto& item : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(logf_nid_postfix(std::bit_cast<float>(item.input))) == item.expected);
        Require(errno == (item.error != 0 ? item.error : 1234));
    }
}

static void CheckFrexpfEnvironment() {
    constexpr struct {
        std::uint32_t input;
        std::uint32_t fraction;
        int exponent;
    } cases[] = {
        {0x00000000u, 0x00000000u, 0}, {0x80000000u, 0x80000000u, 0},
        {0x00000001u, 0x3f000000u, -148}, {0x80000001u, 0xbf000000u, -148},
        {0x007fffffu, 0x3f7ffffeu, -126}, {0x807fffffu, 0xbf7ffffeu, -126},
        {0x00800000u, 0x3f000000u, -125}, {0x80800000u, 0xbf000000u, -125},
        {0x3f000000u, 0x3f000000u, 0}, {0x3f800000u, 0x3f000000u, 1},
        {0xbf800000u, 0xbf000000u, 1}, {0x41400000u, 0x3f400000u, 4},
        {0x7f7fffffu, 0x3f7fffffu, 128}, {0xff7fffffu, 0xbf7fffffu, 128},
        {0x7f800000u, 0x7f800000u, 0}, {0xff800000u, 0xff800000u, 0},
        {0x7f800001u, 0x7f800001u, 0}, {0xff800001u, 0xff800001u, 0},
        {0x7fc01234u, 0x7fc01234u, 0}, {0xffc01234u, 0xffc01234u, 0},
    };
    std::fenv_t saved;
    Require(std::fegetenv(&saved) == 0);
    const unsigned savedCsr = _mm_getcsr();
    constexpr int rounding[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    const int savedError = *__error_nid_postfix();
    for (unsigned mode = 0; mode != 4; ++mode) {
        for (unsigned denorm = 0; denorm != 4; ++denorm) {
            const unsigned control = 0x1f80u | (mode << 13) | ((denorm & 1u) ? 0x40u : 0u) | ((denorm & 2u) ? 0x8000u : 0u);
            for (const unsigned sticky : {0u, 5u, 32u}) {
                for (const auto& item : cases) {
                    Require(std::fesetenv(FE_DFL_ENV) == 0);
                    Require(std::fesetround(rounding[mode]) == 0);
                    *__error_nid_postfix() = 97;
                    _mm_setcsr(control | sticky);
                    int exponent = 77;
                    const auto fraction = std::bit_cast<std::uint32_t>(frexpf_nid_postfix(std::bit_cast<float>(item.input), &exponent));
                    const unsigned after = _mm_getcsr();
                    const bool subnormal = (item.input & 0x7fffffffu) != 0 && (item.input & 0x7fffffffu) < 0x00800000u;
                    const bool flushed = subnormal && (denorm & 1u) != 0;
                    const unsigned flags = sticky | (subnormal && !flushed ? 2u : 0u);
                    const auto expectedFraction = flushed ? (item.input & 0x80000000u) | 0x3f000000u : item.fraction;
                    const int expectedExponent = flushed ? -151 : item.exponent;
                    if (fraction != expectedFraction || exponent != expectedExponent || after != (control | flags) || *__error_nid_postfix() != 97) {
                        std::fprintf(stderr, "frexpf input=%08x mode=%u denorm=%u sticky=%u fraction=%08x exponent=%d csr=%x\n", item.input, mode, denorm, sticky, fraction, exponent, after);
                        std::abort();
                    }
                }
            }
        }
    }
    Require(std::fesetenv(&saved) == 0);
    _mm_setcsr(savedCsr);
    *__error_nid_postfix() = savedError;
}

static void CheckDinkumwareConstants() {
    RequireBytes(&_Inf_nid_postfix, {0, 0, 0, 0, 0, 0, 0xf0, 0x7f});
    RequireBytes(&_Denorm_nid_postfix, {1});
    RequireBytes(&_Hugeval_nid_postfix, {0, 0, 0, 0, 0, 0, 0xf0, 0x7f});
    RequireBytes(&_Nan_nid_postfix, {0, 0, 0, 0, 0, 0, 0xf8, 0x7f});
    RequireBytes(&_Snan_nid_postfix, {1, 0, 0, 0, 0, 0, 0xf0, 0x7f});
    RequireBytes(&_FDenorm_nid_postfix, {1});
    RequireBytes(&_FSnan_nid_postfix, {1, 0, 0x80, 0x7f});
    RequireBytes(&_LDenorm_nid_postfix, {1});
    RequireBytes(&_LInf_nid_postfix, {0, 0, 0, 0, 0, 0, 0, 0x80, 0xff, 0x7f});
    RequireBytes(&_LNan_nid_postfix, {0, 0, 0, 0, 0, 0, 0, 0xc0, 0xff, 0x7f});
    RequireBytes(&_LSnan_nid_postfix, {1, 0, 0, 0, 0, 0, 0, 0x80, 0xff, 0x7f});
    Require(_Dtest_nid_postfix(&_Denorm_nid_postfix._Double) == -2 && _Denorm_nid_postfix._Double == std::numeric_limits<double>::denorm_min());
    Require(_Dtest_nid_postfix(&_Hugeval_nid_postfix._Double) == 1 && _Hugeval_nid_postfix._Double == HUGE_VAL);
    Require(_Dtest_nid_postfix(&_Nan_nid_postfix._Double) == 2 && _Dtest_nid_postfix(&_Snan_nid_postfix._Double) == 2);
    Require(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FDenorm_nid_postfix)) == -2);
    Require(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FSnan_nid_postfix)) == 2);
    Require(_LDenorm_nid_postfix._Long_double == std::numeric_limits<long double>::denorm_min());
    Require(_LInf_nid_postfix._Long_double == std::numeric_limits<long double>::infinity());
    Require(std::isnan(_LNan_nid_postfix._Long_double) && std::isnan(_LSnan_nid_postfix._Long_double));
}

static void CheckMissingClassificationExports() {
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Require(isnanf_nid_postfix(nan) == 1);
    Require(isnanf_nid_postfix(infinity) == 0 && isnanf_nid_postfix(-infinity) == 0);
    Require(isnanf_nid_postfix(0.f) == 0 && isnanf_nid_postfix(-1.f) == 0);

    const long double negativeNan = std::copysign(std::numeric_limits<long double>::quiet_NaN(), -1.L);
    Require(__signbitl_nid_postfix(0.L) == 0 && __signbitl_nid_postfix(-0.L) == 0x8000);
    Require(__signbitl_nid_postfix(1.L) == 0 && __signbitl_nid_postfix(-1.L) == 0x8000);
    Require(__signbitl_nid_postfix(std::numeric_limits<long double>::infinity()) == 0);
    Require(__signbitl_nid_postfix(-std::numeric_limits<long double>::infinity()) == 0x8000);
    Require(__signbitl_nid_postfix(negativeNan) == 0x8000);
    Require(__signbit_nid_postfix(0.0) == 0 && __signbit_nid_postfix(-0.0) == 0x8000 && __signbit_nid_postfix(-1e-310) == 0x8000);
    Require(__signbit_nid_postfix(std::copysign(std::numeric_limits<double>::quiet_NaN(), -1.0)) == 0x8000);
    Require(__signbitf_nid_postfix(1.f) == 0 && __signbitf_nid_postfix(-0.f) == 0x8000 && __signbitf_nid_postfix(-infinity) == 0x8000);
    Require(__signbitf_nid_postfix(nan) == 0 && __signbitf_nid_postfix(std::copysign(nan, -1.f)) == 0x8000);
}

static void CheckFloatMathExports() {
    Require(ceilf_nid_postfix(2.25f) == 3.f && ceilf_nid_postfix(-2.25f) == -2.f);
    Require(floorf_nid_postfix(2.75f) == 2.f && floorf_nid_postfix(-2.25f) == -3.f);
    Require(truncf_nid_postfix(2.75f) == 2.f && truncf_nid_postfix(-2.75f) == -2.f);
    Require(ceilf_nid_postfix(2.f) == 2.f && floorf_nid_postfix(-2.f) == -2.f && truncf_nid_postfix(2.f) == 2.f);
    Require(std::bit_cast<std::uint32_t>(ceilf_nid_postfix(-0.f)) == 0x80000000u);
    Require(std::bit_cast<std::uint32_t>(floorf_nid_postfix(-0.f)) == 0x80000000u);
    Require(std::bit_cast<std::uint32_t>(truncf_nid_postfix(-0.f)) == 0x80000000u);
    Require(ceilf_nid_postfix(std::numeric_limits<float>::infinity()) == std::numeric_limits<float>::infinity());
    Require(floorf_nid_postfix(-std::numeric_limits<float>::infinity()) == -std::numeric_limits<float>::infinity());
    Require(std::isnan(ceilf_nid_postfix(std::numeric_limits<float>::quiet_NaN())));

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    Require(isnan_nid_postfix(nan) == 1 && isnan_nid_postfix(infinity) == 0 && isnan_nid_postfix(0.0) == 0);
    Require(isnan_nid_postfix(-nan) == 1 && isnan_nid_postfix(-1.0) == 0);
    Require(isinf_nid_postfix(infinity) == 1 && isinf_nid_postfix(-infinity) == 1);
    Require(isinf_nid_postfix(nan) == 0 && isinf_nid_postfix(1.0) == 0);
}

static void CheckIntegerAbsAndDivision() {
    for (auto function : {labs_nid_postfix, llabs_nid_postfix, imaxabs_nid_postfix}) {
        Require(function(-5) == 5 && function(7) == 7 && function(0) == 0);
        Require(function(-0x100000000ll) == 0x100000000ll);
        Require(function(INT64_MAX) == INT64_MAX && function(INT64_MIN) == INT64_MIN);
    }
    const auto small = imaxdiv_nid_postfix(-7, 2);
    Require(small.quot == -3 && small.rem == -1);
    const auto large = imaxdiv_nid_postfix(0x123456789abll, -0x1000);
    Require(large.quot == -0x12345678ll && large.rem == 0x9abll);
}

static void CheckScaling() {
    Require(scalbnl_nid_postfix(3.0L, 4) == 48.0L && scalbnl_nid_postfix(1.0L, -16445) == 0x1p-16445L);
    Require(scalbnl_nid_postfix(1.0L, 16384) == std::numeric_limits<long double>::infinity());
    Require(significandf_nid_postfix(12.0f) == 1.5f && significandf_nid_postfix(-0.375f) == -1.5f);
    Require(significandf_nid_postfix(0x1p-149f) == 1.0f);
    Require(std::signbit(significandf_nid_postfix(-0.0f)) && significandf_nid_postfix(-0.0f) == 0.0f);
    Require(significandf_nid_postfix(std::numeric_limits<float>::infinity()) == std::numeric_limits<float>::infinity());
    Require(std::isnan(significandf_nid_postfix(std::numeric_limits<float>::quiet_NaN())));
    Require(significand_nid_postfix(12.0) == 1.5 && significand_nid_postfix(-0.375) == -1.5);
    Require(significand_nid_postfix(0x1p-1074) == 1.0);
    Require(std::signbit(significand_nid_postfix(-0.0)) && significand_nid_postfix(-0.0) == 0.0);
    Require(significand_nid_postfix(std::numeric_limits<double>::infinity()) == std::numeric_limits<double>::infinity());
    Require(std::isnan(significand_nid_postfix(std::numeric_limits<double>::quiet_NaN())));
}

static bool Near(long double actual, long double expected) {
    return std::fabs(actual - expected) <= std::fabs(expected) * 1e-17L;
}

static void CheckAcosfMsun() {
    Require(std::bit_cast<std::uint32_t>(acosf_nid_postfix(std::bit_cast<float>(0x3d58d031u))) == 0x3fc2488au);
    Require(std::bit_cast<std::uint32_t>(acosf_nid_postfix(std::bit_cast<float>(0x3f00b99au))) == 0x3f859f4fu);
    Require(std::bit_cast<std::uint32_t>(acosf_nid_postfix(std::bit_cast<float>(0xbf00b99au))) == 0x40064033u);
    Require(std::bit_cast<std::uint32_t>(acosf_nid_postfix(1.0f)) == 0u);
    Require(std::bit_cast<std::uint32_t>(acosf_nid_postfix(-1.0f)) == 0x40490fdbu);
    Require(std::bit_cast<std::uint32_t>(acosf_nid_postfix(std::bit_cast<float>(0x32800000u))) == 0x3fc90fdbu);
}

static void CheckLongDoubleHyperbolic() {
    const long double infinity = std::numeric_limits<long double>::infinity();
    const long double nan = std::numeric_limits<long double>::quiet_NaN();
    Require(_LCosh_nid_postfix(0.0L, 2.0L) == 2.0L);
    Require(Near(_LCosh_nid_postfix(1.0L, 2.0L), 3.08616126963048755696L));
    Require(Near(_LCosh_nid_postfix(-1.0L, -2.0L), -3.08616126963048755696L));
    Require(std::isinf(_LCosh_nid_postfix(infinity, -1.0L)) && std::signbit(_LCosh_nid_postfix(infinity, -1.0L)));
    Require(std::isnan(_LCosh_nid_postfix(infinity, 0.0L)));
    Require(std::isnan(_LCosh_nid_postfix(nan, 1.0L)));
    Require(_LCosh_nid_postfix(720.0L, 0.0L) == 0.0L && !std::signbit(_LCosh_nid_postfix(720.0L, 0.0L)));
    Require(_LCosh_nid_postfix(720.0L, -0.0L) == 0.0L && std::signbit(_LCosh_nid_postfix(720.0L, -0.0L)));
    Require(Near(_LCosh_nid_postfix(11357.0L, 0x1p-16000L), 0xc.e2773666cc8cc68p+380L));
    Require(Near(_LCosh_nid_postfix(-11357.0L, 0x1p-16000L), 0xc.e2773666cc8cc68p+380L));
    Require(Near(_LCosh_nid_postfix(11360.0L, 0x1p-16384L), 0x8.165ba1f5c0ff231p+1L));
    Require(std::isinf(_LCosh_nid_postfix(11360.0L, 1.0L)));
    Require(_LSinh_nid_postfix(0.0L, 2.0L) == 0.0L && !std::signbit(_LSinh_nid_postfix(0.0L, 2.0L)));
    Require(_LSinh_nid_postfix(0.0L, -2.0L) == 0.0L && std::signbit(_LSinh_nid_postfix(0.0L, -2.0L)));
    Require(Near(_LSinh_nid_postfix(1.0L, 2.0L), 2.35040238728760291376L));
    Require(Near(_LSinh_nid_postfix(-1.0L, -2.0L), 2.35040238728760291376L));
    Require(Near(_LSinh_nid_postfix(1.0L, -2.0L), -2.35040238728760291376L));
    Require(std::isinf(_LSinh_nid_postfix(infinity, -1.0L)) && std::signbit(_LSinh_nid_postfix(infinity, -1.0L)));
    Require(std::isinf(_LSinh_nid_postfix(-infinity, -1.0L)) && !std::signbit(_LSinh_nid_postfix(-infinity, -1.0L)));
    Require(std::isnan(_LSinh_nid_postfix(infinity, 0.0L)));
    Require(std::isnan(_LSinh_nid_postfix(nan, 1.0L)));
    Require(_LSinh_nid_postfix(720.0L, 0.0L) == 0.0L && !std::signbit(_LSinh_nid_postfix(720.0L, 0.0L)));
    Require(_LSinh_nid_postfix(720.0L, -0.0L) == 0.0L && std::signbit(_LSinh_nid_postfix(720.0L, -0.0L)));
    Require(_LSinh_nid_postfix(-720.0L, 0.0L) == 0.0L && std::signbit(_LSinh_nid_postfix(-720.0L, 0.0L)));
    Require(_LSinh_nid_postfix(-720.0L, -0.0L) == 0.0L && !std::signbit(_LSinh_nid_postfix(-720.0L, -0.0L)));
    Require(Near(_LSinh_nid_postfix(11357.0L, 0x1p-16000L), 0xc.e2773666cc8cc68p+380L));
    Require(Near(_LSinh_nid_postfix(-11357.0L, 0x1p-16000L), -0xc.e2773666cc8cc68p+380L));
    Require(Near(_LSinh_nid_postfix(11357.0L, -0x1p-16000L), -0xc.e2773666cc8cc68p+380L));
    Require(Near(_LSinh_nid_postfix(-11357.0L, -0x1p-16000L), 0xc.e2773666cc8cc68p+380L));
    Require(Near(_LSinh_nid_postfix(11360.0L, 0x1p-16384L), 0x8.165ba1f5c0ff231p+1L));
    Require(Near(_LSinh_nid_postfix(-11360.0L, 0x1p-16384L), -0x8.165ba1f5c0ff231p+1L));
    Require(std::isinf(_LSinh_nid_postfix(11360.0L, 1.0L)));
    Require(std::isinf(_LSinh_nid_postfix(-11360.0L, 1.0L)) && std::signbit(_LSinh_nid_postfix(-11360.0L, 1.0L)));
}

static void CheckLongDoubleFunctions() {
    const long double infinity = std::numeric_limits<long double>::infinity();
    const long double nan = std::numeric_limits<long double>::quiet_NaN();
    Require(acoshl_nid_postfix(1.0L) == 0.0L && std::isnan(acoshl_nid_postfix(0.5L)));
    Require(asinhl_nid_postfix(-0.0L) == 0.0L && std::signbit(asinhl_nid_postfix(-0.0L)));
    Require(atanhl_nid_postfix(1.0L) == infinity && Near(atanhl_nid_postfix(0.5L), 0.5493061443340548456976226L));
    Require(Near(atan2l_nid_postfix(1.0L, 1.0L), 0.7853981633974483096156608L) && Near(atan2l_nid_postfix(0.0L, -1.0L), 3.1415926535897932384626434L));
    Require(cbrtl_nid_postfix(-27.0L) == -3.0L && Near(cbrtl_nid_postfix(0x1p-16443L) * cbrtl_nid_postfix(0x1p-16443L) * cbrtl_nid_postfix(0x1p-16443L), 0x1p-16443L));
    Require(erfl_nid_postfix(0.0L) == 0.0L && erfl_nid_postfix(infinity) == 1.0L && erfcl_nid_postfix(0.0L) == 1.0L);
    Require(erfcl_nid_postfix(infinity) == 0.0L && erfcl_nid_postfix(-infinity) == 2.0L);
    Require(expm1l_nid_postfix(0.0L) == 0.0L && expm1l_nid_postfix(-infinity) == -1.0L);
    Require(log1pl_nid_postfix(0.0L) == 0.0L && log1pl_nid_postfix(-1.0L) == -infinity);
    Require(log2l_nid_postfix(8.0L) == 3.0L && log2l_nid_postfix(0x1p-16445L) == -16445.0L);
    Require(lgammal_nid_postfix(1.0L) == 0.0L && lgammal_nid_postfix(2.0L) == 0.0L && lgammal_nid_postfix(0.0L) == infinity);
    Require(tgammal_nid_postfix(5.0L) == 24.0L && Near(tgammal_nid_postfix(0.5L), 1.7724538509055160272981675L));
    Require(tgammaf_nid_postfix(5.0f) == 24.0f && std::abs(tgammaf_nid_postfix(0.5f) - 1.77245385f) < 1e-6f);
    Require(tgammaf_nid_postfix(1.0f) == 1.0f && tgammaf_nid_postfix(2.0f) == 1.0f && tgammaf_nid_postfix(3.0f) == 2.0f);
    Require(std::isinf(tgammaf_nid_postfix(0.0f)) && std::isnan(tgammaf_nid_postfix(-1.0f)));
    for (const float pole : {-1.0f, -2.0f, -710.0f, -std::numeric_limits<float>::infinity()}) Require(std::bit_cast<std::uint32_t>(tgammaf_nid_postfix(pole)) == 0xffc00000u);
    const float negativeGammaUnderflow = tgammaf_nid_postfix(-4260.884765625f);
    const float positiveGammaUnderflow = tgammaf_nid_postfix(-4261.2f);
    Require(negativeGammaUnderflow == 0.0f && std::signbit(negativeGammaUnderflow));
    Require(positiveGammaUnderflow == 0.0f && !std::signbit(positiveGammaUnderflow));
    for (const float x : {0.0f, -0.0f, -INFINITY, INFINITY, 40.0f, 1.0e5f, 3.0e38f, -4261.2f, 0.5f}) {
        errno = 7;
        static_cast<void>(tgammaf_nid_postfix(x));
        Require(errno == 7);
    }
    for (const float x : {-1.0f, -2.0f, -1.0e5f, -3.0e38f}) {
        errno = 7;
        Require(std::isnan(tgammaf_nid_postfix(x)) && errno == EDOM);
    }
    Require(hypotl_nid_postfix(3.0L, -4.0L) == 5.0L && hypotl_nid_postfix(nan, -infinity) == infinity);

    Require(hypot3f_nid_postfix(3.0f, -4.0f, 12.0f) == 13.0f && hypot3f_nid_postfix(0.0f, 3.0f, -4.0f) == 5.0f);
    Require(hypot3f_nid_postfix(2.0f, 0.0f, 0.0f) == 2.0f && hypot3f_nid_postfix(-2.0f, 3.0f, 6.0f) == 7.0f);
    Require(hypot3_nid_postfix(1.0, 2.0, 2.0) == 3.0 && hypot3_nid_postfix(12.0, 3.0, 4.0) == 13.0);
    Require(hypot3l_nid_postfix(2.0L, -3.0L, 6.0L) == 7.0L);
    const double payload = std::bit_cast<double>(0x7ff8000000000123ull);
    Require(std::bit_cast<std::uint64_t>(hypot3_nid_postfix(1.0, payload, std::numeric_limits<double>::infinity())) == 0x7ff8000000000123ull);
    Require(std::isnan(hypot3f_nid_postfix(std::numeric_limits<float>::infinity(), 1.0f, std::numeric_limits<float>::quiet_NaN())));
    Require(hypot3l_nid_postfix(-infinity, 1.0L, 2.0L) == infinity && hypot3_nid_postfix(1.0, -std::numeric_limits<double>::infinity(), 0.0) == std::numeric_limits<double>::infinity());
    Require(hypot3_nid_postfix(0x1p1000, 0x1p1000, 0x1p1000) == std::hypot(0x1p1000, std::hypot(0x1p1000, 0x1p1000)));

    Require(j0f_nid_postfix(0.0f) == 1.0f && j1f_nid_postfix(0.0f) == 0.0f && jnf_nid_postfix(2, 0.0f) == 0.0f);
    Require(std::fabs(j0f_nid_postfix(2.5f) - static_cast<float>(-0.048383776468198)) < 1e-6f);
    Require(std::fabs(jnf_nid_postfix(3, 4.0f) - static_cast<float>(0.430171473875622)) < 1e-6f);
    Require(y0f_nid_postfix(0.0f) == -std::numeric_limits<float>::infinity() && y1f_nid_postfix(0.0f) == -std::numeric_limits<float>::infinity());
    Require(ynf_nid_postfix(2, 0.0f) == -std::numeric_limits<float>::infinity());
    Require(std::fabs(ynf_nid_postfix(2, 3.0f) - static_cast<float>(-0.160400393484924)) < 1e-6f);
    Require(std::isnan(y0f_nid_postfix(-1.0f)));
}

static void CheckDoubleClassification() {
    const struct { std::uint64_t bits; short code; } cases[] = {
        {0x0000000000000000ull, 0}, {0x8000000000000000ull, 0},
        {0x0000000000000001ull, -2}, {0x800fffffffffffffull, -2},
        {0x0010000000000000ull, -1}, {0xbff0000000000000ull, -1}, {0x7fefffffffffffffull, -1},
        {0x7ff0000000000000ull, 1}, {0xfff0000000000000ull, 1},
        {0x7ff0000000000001ull, 2}, {0x7ff8000000000000ull, 2}, {0xfff8000000000000ull, 2},
    };
    for (const auto& test : cases) {
        double value;
        std::memcpy(&value, &test.bits, sizeof(value));
        if (_Dtest_nid_postfix(&value) != test.code) {
            std::fprintf(stderr, "Guest _Dtest failed for %016llx\n", static_cast<unsigned long long>(test.bits));
            std::abort();
        }
    }
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

static void CheckLongDoubleRounding(long double value, std::int64_t expected, bool invalid) {
    std::feclearexcept(FE_ALL_EXCEPT);
    errno = 12345;
    const std::int64_t result = llroundl_nid_postfix(value);
    if (result != expected || (std::fetestexcept(FE_INVALID) != 0) != invalid || errno != 12345) {
        std::fprintf(stderr, "llroundl failed for %La: expected %lld, got %lld\n", value,
            static_cast<long long>(expected), static_cast<long long>(result));
        std::abort();
    }
}

static void CheckNoConversionZero() {
    for (const char* text : {"", " ", "-", " -", "+", "-.", "+.", "-x", "-.e1", "-e5", "- 1", "  -."}) {
        char* end = nullptr;
        const double parsed = strtod_nid_postfix(text, &end);
        Require(parsed == 0. && !std::signbit(parsed) && end == text);
        const bool negative = std::strchr(text, '-') != nullptr;
        const bool loneSign = negative && text[std::strlen(text) - 1] == '-';
        const float parsedFloat = strtof_nid_postfix(text, &end);
        Require(parsedFloat == 0.f && std::signbit(parsedFloat) == (negative && !loneSign) && end == text);
        const long double parsedLong = strtold_nid_postfix(text, &end);
        Require(parsedLong == 0.L && std::signbit(parsedLong) == negative && end == text);
        Require(!std::signbit(atof_nid_postfix(text)));
    }
    for (const char* text : {"-i", "-in", "-N", "-nax"}) {
        char* end = nullptr;
        const float parsedFloat = strtof_nid_postfix(text, &end);
        Require(parsedFloat == 0.f && std::signbit(parsedFloat) && end == text);
        Require(!std::signbit(strtold_nid_postfix(text, nullptr)));
    }
    struct SignedNan { const char* text; std::uint32_t bits; };
    for (const SignedNan& signedNan : {SignedNan{"-nan", 0xffc00000u}, SignedNan{"-NAN", 0xffc00000u}, SignedNan{" -nan(5)", 0xffc00005u}}) {
        const char* const text = signedNan.text;
        char* end = nullptr;
        const float parsedFloat = strtof_nid_postfix(text, &end);
        Require(std::bit_cast<std::uint32_t>(parsedFloat) == signedNan.bits && end == text + std::strlen(text));
    }
    Require(std::bit_cast<std::uint32_t>(strtof_nid_postfix("nan", nullptr)) == 0x7fc00000u);
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
    Require(parsedWideFloat == 0.f && std::signbit(parsedWideFloat) && wideEnd == wide);
    const long double parsedWideLong = wcstold_nid_postfix(wide, &wideEnd);
    Require(parsedWideLong == 0.L && std::signbit(parsedWideLong) && wideEnd == wide);
}

template<typename TValue, std::size_t TBytes>
static void RequireCharacteristic(const unsigned char (&constant)[16], TValue expected) {
    TValue value;
    std::memcpy(&value, constant, sizeof(value));
    Require(value == expected);
    for (std::size_t index = TBytes; index < 16; ++index) Require(constant[index] == 0);
}

template<typename TValue, std::size_t TBytes>
static void RequireCharacteristics(const DinkumwareFloatCharacteristics& table, const int (&integers)[6]) {
    const int fields[] = {table.digits, table.mantissaDigits, table.maxExponent10, table.maxExponent, table.minExponent10, table.minExponent};
    for (int index = 0; index < 6; ++index) Require(fields[index] == integers[index]);
    RequireCharacteristic<TValue, TBytes>(table.epsilon, std::numeric_limits<TValue>::epsilon());
    RequireCharacteristic<TValue, TBytes>(table.max, std::numeric_limits<TValue>::max());
    RequireCharacteristic<TValue, TBytes>(table.min, std::numeric_limits<TValue>::min());
}

static void CheckDinkumwareFloatCharacteristics() {
    static_assert(sizeof(DinkumwareFloatCharacteristics) == 80 && offsetof(DinkumwareFloatCharacteristics, epsilon) == 32);
    RequireCharacteristics<float, 4>(_Flt_nid_postfix, {FLT_DIG, FLT_MANT_DIG, FLT_MAX_10_EXP, FLT_MAX_EXP, FLT_MIN_10_EXP, FLT_MIN_EXP});
    RequireCharacteristics<double, 8>(_Dbl_nid_postfix, {DBL_DIG, DBL_MANT_DIG, DBL_MAX_10_EXP, DBL_MAX_EXP, DBL_MIN_10_EXP, DBL_MIN_EXP});
    RequireCharacteristics<long double, 10>(_Ldbl_nid_postfix, {18, 64, 4932, 16384, -4931, -16381});
    for (const auto* table : {&_Flt_nid_postfix, &_Dbl_nid_postfix, &_Ldbl_nid_postfix})
        for (std::size_t index = 24; index < 32; ++index) Require(reinterpret_cast<const unsigned char*>(table)[index] == 0);
}

static void CheckCbrtConsoleBits() {
    constexpr std::uint64_t cases[][2] = {
        {0x0000000000000000ull, 0x0000000000000000ull}, {0x8000000000000000ull, 0x8000000000000000ull},
        {0x0000000000000001ull, 0x2990000000000000ull}, {0x8000000000000001ull, 0xa990000000000000ull},
        {0x000fffffffffffffull, 0x2aa428a2f98d728aull}, {0x0010000000000000ull, 0x2aa428a2f98d728bull},
        {0x3ff0000000000000ull, 0x3ff0000000000000ull}, {0xbff0000000000000ull, 0xbff0000000000000ull},
        {0x4020000000000000ull, 0x4000000000000000ull}, {0xc03b000000000000ull, 0xc008000000000000ull},
        {0x408f400000000000ull, 0x4024000000000000ull}, {0x4000000000000000ull, 0x3ff428a2f98d728bull},
        {0x3fe0000000000000ull, 0x3fe965fea53d6e3dull}, {0x7fefffffffffffffull, 0x554428a2f98d728bull},
        {0xffefffffffffffffull, 0xd54428a2f98d728bull}, {0x7ff0000000000000ull, 0x7ff0000000000000ull},
        {0xfff0000000000000ull, 0xfff0000000000000ull}, {0x7ff8000000000001ull, 0x7ff8000000000001ull},
        {0xfff8000000000002ull, 0xfff8000000000002ull}, {0x3ffa0ccfe147c94dull, 0x3ff2d29f09b6d6a3ull},
        {0xbf840fe48e07e54dull, 0xbfcb634d0e8f50ffull}, {0x3fa7d31e3ec31ee2ull, 0x3fd70508d1167185ull},
        {0xc050f6b81592ae3full, 0xc01050a4409443faull}, {0xc0506b7f30c60f3cull, 0xc0102385f7679661ull},
        {0xc039809ec6db32bcull, 0xc0078c3d13f2e055ull},
    };
    for (const auto& [input, expected] : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint64_t>(cbrt_nid_postfix(std::bit_cast<double>(input))) == expected && errno == 1234);
    }
}

static void CheckLogbConsoleBits() {
    constexpr struct {
        std::uint64_t input;
        std::uint64_t expected;
    } cases[] = {
        {0x0000000000000000ull, 0xfff0000000000000ull},
        {0x8000000000000000ull, 0xfff0000000000000ull},
        {0x0000000000000001ull, 0xc090c80000000000ull},
        {0x8000000000000001ull, 0xc090c80000000000ull},
        {0x000fffffffffffffull, 0xc08ff80000000000ull},
        {0x0010000000000000ull, 0xc08ff00000000000ull},
        {0x3fe8000000000000ull, 0xbff0000000000000ull},
        {0x4020000000000000ull, 0x4008000000000000ull},
        {0xc020000000000000ull, 0x4008000000000000ull},
        {0x7fefffffffffffffull, 0x408ff80000000000ull},
        {0x7ff0000000000000ull, 0x7ff0000000000000ull},
        {0xfff0000000000000ull, 0x7ff0000000000000ull},
        {0x7ff0000000000001ull, 0x7ff8000000000001ull},
        {0xfff4000000000123ull, 0xfffc000000000123ull},
        {0x7ff8000000000456ull, 0x7ff8000000000456ull},
    };
    for (const auto& item : cases) {
        errno = 1234;
        Require(std::bit_cast<std::uint64_t>(logb_nid_postfix(std::bit_cast<double>(item.input))) == item.expected && errno == 1234);
    }
    constexpr struct {
        std::uint32_t input;
        std::uint32_t expected;
    } floatCases[] = {
        {0x00000000u, 0xff800000u},
        {0x80000000u, 0xff800000u},
        {0x00000001u, 0xc3150000u},
        {0x80000001u, 0xc3150000u},
        {0x007fffffu, 0xc2fe0000u},
        {0x00800000u, 0xc2fc0000u},
        {0x3f400000u, 0xbf800000u},
        {0x41000000u, 0x40400000u},
        {0xc1000000u, 0x40400000u},
        {0x7f7fffffu, 0x42fe0000u},
        {0x7f800000u, 0x7f800000u},
        {0xff800000u, 0x7f800000u},
        {0x7f800001u, 0x7fc00001u},
        {0xffa00123u, 0xffe00123u},
        {0x7fc00456u, 0x7fc00456u},
    };
    for (const auto& item : floatCases) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(logbf_nid_postfix(std::bit_cast<float>(item.input))) == item.expected && errno == 1234);
    }
}

int main() {
    CheckFloatClassification();
    CheckDinkumwareConstants();
    CheckNoConversionZero();
    CheckMissingClassificationExports();
    CheckFloatMathExports();
    CheckIntegerConversions();
    CheckCbrtfConsoleBits();
    CheckExtendedMath();
    CheckTrigfConsoleBits();
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
    CheckNanConversions();
    CheckModfDecomposition();
    Require(fmodf_nid_postfix(5.5f, 2.f) == 1.5f);
    Require(fmodf_nid_postfix(-5.5f, 2.f) == -1.5f);
    Require(std::signbit(fmodf_nid_postfix(-4.f, 2.f)));
    Require(std::isnan(fmodf_nid_postfix(1.f, 0.f)));
    int quo = 0;
    Require(remquo_nid_postfix(7.0, 3.0, &quo) == 1.0 && (quo & 7) == 2);
    Require(remquo_nid_postfix(-7.0, 3.0, &quo) == -1.0 && (quo & 7) == ((-2) & 7));
    int quof = 0;
    Require(remquof_nid_postfix(7.0f, 3.0f, &quof) == 1.0f && (quof & 7) == 2);
    Require(remquof_nid_postfix(-7.0f, 3.0f, &quof) == -1.0f && (quof & 7) == ((-2) & 7));
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
    Require(rand_r_nid_postfix(&randomState) == 16807 && randomState == 16807u);
    Require(rand_r_nid_postfix(&randomState) == 282475249 && rand_r_nid_postfix(&randomState) == 1622650073);
    randomState = 0;
    Require(rand_r_nid_postfix(&randomState) == 0x2ae1b767 && randomState == 0x2ae1b767u);
    randomState = 0x7fffffffu;
    Require(rand_r_nid_postfix(&randomState) == 0 && randomState == 0u);
    randomState = 0x7ffffffeu;
    Require(rand_r_nid_postfix(&randomState) == 2147466840 && randomState == 2147466840u);
    randomState = 0xffffffffu;
    Require(rand_r_nid_postfix(&randomState) == 2147466840 && randomState == 2147466840u);
    srand_nid_postfix(1);
    Require(rand_nid_postfix() == 408024109 && rand_nid_postfix() == 11635919 && rand_nid_postfix() == 196474438);
    srand_nid_postfix(0);
    Require(rand_nid_postfix() == 0 && rand_nid_postfix() == 408024109);
    srand_nid_postfix(0xffffffffu);
    Require(rand_nid_postfix() == 876841727 && rand_nid_postfix() == 830605684);
    for (int step = 0; step < 1000; ++step) Require(static_cast<unsigned int>(rand_nid_postfix()) <= 0x3fffffffu);
    bool threw = false;
    try { rand_r_nid_postfix(nullptr); } catch (...) { threw = true; }
    Require(threw);
    Require(hypot_nid_postfix(3.0, 4.0) == 5.0 && hypot_nid_postfix(-3.0, -4.0) == 5.0 && hypotf_nid_postfix(3.f, -4.f) == 5.f);
    Require(copysign_nid_postfix(1.0, 2.0) == 1.0 && copysign_nid_postfix(1.0, -2.0) == -1.0);
    Require(copysign_nid_postfix(-1.0, 2.0) == 1.0 && copysign_nid_postfix(-1.0, -2.0) == -1.0);
    Require(std::bit_cast<std::uint64_t>(copysign_nid_postfix(0.0, -1.0)) == 0x8000000000000000ull);
    Require(std::bit_cast<std::uint64_t>(copysign_nid_postfix(-0.0, 1.0)) == 0);
    Require(copysign_nid_postfix(INFINITY, -1.0) == -INFINITY && copysign_nid_postfix(-INFINITY, 1.0) == INFINITY);
    Require(std::isnan(copysign_nid_postfix(std::numeric_limits<double>::quiet_NaN(), -1.0)) && std::signbit(copysign_nid_postfix(std::numeric_limits<double>::quiet_NaN(), -1.0)));
    Require(std::isnan(copysign_nid_postfix(std::numeric_limits<double>::quiet_NaN(), 1.0)) && !std::signbit(copysign_nid_postfix(std::numeric_limits<double>::quiet_NaN(), 1.0)));
    Require(copysignf_nid_postfix(1.0f, 2.0f) == 1.0f && copysignf_nid_postfix(1.0f, -2.0f) == -1.0f);
    Require(std::bit_cast<std::uint32_t>(copysignf_nid_postfix(0.0f, -1.0f)) == 0x80000000u);
    Require(std::bit_cast<std::uint32_t>(copysignf_nid_postfix(-0.0f, 1.0f)) == 0);
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
    Require(erf_nid_postfix(0.) == 0. && std::signbit(erf_nid_postfix(-0.)) && erf_nid_postfix(10.) == 1. && erf_nid_postfix(-10.) == -1.);
    Require(std::abs(erf_nid_postfix(0.5) - 0.5204998778130465) < 1e-15 && std::isnan(erf_nid_postfix(std::numeric_limits<double>::quiet_NaN())));
    Require(erfc_nid_postfix(0.) == 1. && erfc_nid_postfix(100.) == 0. && erfc_nid_postfix(-100.) == 2.);
    Require(std::abs(erfc_nid_postfix(0.5) - 0.47950012218695346) < 1e-15 && std::isnan(erfc_nid_postfix(std::numeric_limits<double>::quiet_NaN())));
    Require(erfcf_nid_postfix(0.f) == 1.f && erfcf_nid_postfix(100.f) == 0.f && erfcf_nid_postfix(-100.f) == 2.f);
    Require(std::abs(erfcf_nid_postfix(0.5f) - 0.47950012f) < 1e-6f && std::isnan(erfcf_nid_postfix(std::numeric_limits<float>::quiet_NaN())));
    Require(lgamma_nid_postfix(1.) == 0. && lgamma_nid_postfix(2.) == 0. && std::isinf(lgamma_nid_postfix(0.)));
    Require(std::abs(lgamma_nid_postfix(0.5) - 0.5723649429247001) < 1e-15 && std::abs(lgamma_nid_postfix(10.) - 12.801827480081469) < 1e-13);
    Require(std::abs(lgamma_nid_postfix(-0.5) - 1.2655121234846454) < 1e-15);
    for (const double sign : {1.0, -1.0}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::abs(lgamma_nid_postfix(sign * std::numeric_limits<double>::denorm_min()) - 744.44007192138122) < 1e-12);
        Require(std::abs(lgamma_nid_postfix(sign * 0x1p-1025) - 710.47586007394398) < 1e-12);
        Require(std::abs(lgamma_nid_postfix(sign * 0x1p-1022) - 708.39641853226408) < 1e-12);
        Require(std::abs(lgamma_nid_postfix(sign * 0x1p-57) - 39.509389291916882) < 1e-13);
        Require(std::fetestexcept(FE_OVERFLOW | FE_DIVBYZERO | FE_INVALID) == 0);
    }
    const auto gammaSign = [](double x) {
        signgam_nid_postfix = 7;
        lgamma_nid_postfix(x);

        return signgam_nid_postfix;
    };
    Require(gammaSign(0.5) == 1 && gammaSign(10.) == 1 && gammaSign(0.) == 1 && gammaSign(-0.) == -1);
    Require(gammaSign(-0.5) == -1 && gammaSign(-1.5) == 1 && gammaSign(-2.5) == -1 && gammaSign(-0x1p-80) == -1);
    Require(gammaSign(-1.) == 1 && gammaSign(-2.) == 1 && gammaSign(-0x1p60) == 1 && gammaSign(-4503599627370495.5) == 1);
    Require(gammaSign(INFINITY) == 1 && gammaSign(-INFINITY) == 1 && gammaSign(NAN) == 1);
    Require(llroundl_nid_postfix(2.5L) == 3 && llroundl_nid_postfix(-2.5L) == -3 && llroundl_nid_postfix(0.49999999999999999L) == 0);
    Require(llroundl_nid_postfix(9007199254740993.0L) == 9007199254740993LL && llroundl_nid_postfix(-1e30L) == INT64_MIN);
    for (const int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        Require(std::fesetround(mode) == 0);
        CheckLongDoubleRounding(9223372036854775807.0L, INT64_MAX, false);
        CheckLongDoubleRounding(9223372036854775806.5L, INT64_MAX, false);
        CheckLongDoubleRounding(9223372036854775806.0L, INT64_MAX - 1, false);
        CheckLongDoubleRounding(-0x1p63L, INT64_MIN, false);
        CheckLongDoubleRounding(-9223372036854775807.5L, INT64_MIN, false);
        CheckLongDoubleRounding(-9223372036854775807.0L, INT64_MIN + 1, false);
        for (const long double value : {9223372036854775807.5L, 0x1p63L, -9223372036854775809.0L,
            std::numeric_limits<long double>::infinity(), -std::numeric_limits<long double>::infinity(),
            std::numeric_limits<long double>::quiet_NaN()}) {
            CheckLongDoubleRounding(value, INT64_MIN, true);
        }
    }
    Require(std::fesetround(FE_TONEAREST) == 0);
    Require(logbf_nid_postfix(8.f) == 3.f && logbf_nid_postfix(-0.75f) == -1.f);
    Require(logbf_nid_postfix(std::numeric_limits<float>::denorm_min()) == -149.f);
    Require(logbf_nid_postfix(0.f) == -std::numeric_limits<float>::infinity());
    Require(logbf_nid_postfix(-std::numeric_limits<float>::infinity()) == std::numeric_limits<float>::infinity());
    Require(std::isnan(logbf_nid_postfix(std::numeric_limits<float>::quiet_NaN())));
    Require(ilogb_nid_postfix(8.0) == 3 && ilogb_nid_postfix(-0.75) == -1 && ilogb_nid_postfix(1.0) == 0);
    Require(ilogb_nid_postfix(1024.0) == 10 && ilogb_nid_postfix(0.125) == -3);
    Require(ilogbf_nid_postfix(8.f) == 3 && ilogbf_nid_postfix(-0.75f) == -1 && ilogbf_nid_postfix(1.f) == 0);
    Require(ilogbf_nid_postfix(1024.f) == 10 && ilogbf_nid_postfix(0.125f) == -3);
    Require(ilogb_nid_postfix(0.0) == INT_MIN && ilogb_nid_postfix(-0.0) == INT_MIN);
    Require(ilogb_nid_postfix(std::numeric_limits<double>::infinity()) == INT_MAX);
    Require(ilogb_nid_postfix(-std::numeric_limits<double>::infinity()) == INT_MAX);
    Require(ilogb_nid_postfix(std::numeric_limits<double>::quiet_NaN()) == INT_MAX);
    Require(ilogbf_nid_postfix(0.f) == INT_MIN && ilogbf_nid_postfix(-0.f) == INT_MIN);
    Require(ilogbf_nid_postfix(std::numeric_limits<float>::infinity()) == INT_MAX);
    Require(ilogbf_nid_postfix(std::numeric_limits<float>::quiet_NaN()) == INT_MAX);
    Require(ilogbf_nid_postfix(std::numeric_limits<float>::denorm_min()) == -149);
    Require(exp2_nid_postfix(-3.) == 0.125);
    Require(ldexp_nid_postfix(0.75, 4) == 12.);
    CheckSquareRoot();
    CheckExp2Errno();
    CheckExactOperations();
    CheckNearbyint();
    CheckFegetenv();
    CheckLongDouble();
    CheckScalbnErrno();
    CheckNearbyintl();
    CheckCbrtConsoleBits();
    CheckLogbConsoleBits();
    CheckExpm1ConsoleBits();
    CheckLog1pConsoleBits();
    CheckNextafterConsoleBits();
    CheckRemainderf();
    CheckTanhConsoleBits();
    CheckSinhMsunBits();
    CheckSinhLargeArguments();
    CheckSinhOverflowErrno();
    CheckAsinhMsunBits();
    CheckAsinhLargeArguments();
    CheckRoundingNaNConsoleBits();
    CheckNextafterEnvironment(nextafterf_nid_postfix, 0x80000000u, 0x3f800000u, 0x00800000u, 0x7f7fffffu, 0x7f800000u);
    CheckNextafterEnvironment(nextafter_nid_postfix, std::uint64_t{0x8000000000000000ull}, std::uint64_t{0x3ff0000000000000ull}, std::uint64_t{0x0010000000000000ull}, std::uint64_t{0x7fefffffffffffffull}, std::uint64_t{0x7ff0000000000000ull});
    CheckExpm1ScalingRounding();
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
    Require(nextafter_nid_postfix(1.0, 2.0) == 0x1.0000000000001p0 && nextafter_nid_postfix(1.0, 0.0) == 0x1.fffffffffffffp-1);
    Require(nextafter_nid_postfix(0.0, 1.0) == std::numeric_limits<double>::denorm_min());
    Require(nextafter_nid_postfix(0.0, -1.0) == -std::numeric_limits<double>::denorm_min());
    Require(nextafter_nid_postfix(std::numeric_limits<double>::denorm_min(), -1.0) == 0.0);
    Require(nextafter_nid_postfix(1.0, 1.0) == 1.0 && !std::signbit(nextafter_nid_postfix(-0.0, 0.0)));
    Require(nextafter_nid_postfix(std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity()) == std::numeric_limits<double>::infinity());
    Require(std::isnan(nextafter_nid_postfix(std::numeric_limits<double>::quiet_NaN(), 1.0)) && std::isnan(nextafter_nid_postfix(1.0, std::numeric_limits<double>::quiet_NaN())));
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
    std::feclearexcept(FE_ALL_EXCEPT);
    std::feraiseexcept(FE_DIVBYZERO);
    for (const long double value : {0.0L, -0.0L, 1.0L, -1.0L, std::numeric_limits<long double>::denorm_min(),
                                  -std::numeric_limits<long double>::denorm_min(), std::numeric_limits<long double>::min(),
                                  std::numeric_limits<long double>::max(), std::numeric_limits<long double>::lowest(),
                                  0x1p1024L, -0x1p1024L, 0x1p-16382L}) {
        Require(__isfinitel_nid_postfix(value) == 1);
    }
    for (const long double value : {std::numeric_limits<long double>::infinity(), -std::numeric_limits<long double>::infinity(),
                                  std::numeric_limits<long double>::quiet_NaN(), -std::numeric_limits<long double>::quiet_NaN(),
                                  std::numeric_limits<long double>::signaling_NaN(), -std::numeric_limits<long double>::signaling_NaN()}) {
        Require(__isfinitel_nid_postfix(value) == 0);
    }
    Require(std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(__isnormalf_nid_postfix(1.f) == 1 && __isnormalf_nid_postfix(0.f) == 0);
    Require(__isnormalf_nid_postfix(std::numeric_limits<float>::denorm_min()) == 0);
    Require(__isnormal_nid_postfix(1.) == 1 && __isnormal_nid_postfix(0.) == 0);
    Require(__isnormal_nid_postfix(std::numeric_limits<double>::denorm_min()) == 0);
    CheckAsin();
    const auto quotient = div_nid_postfix(7, 2);
    CheckAsinfErrno();
    CheckAsinfConsoleBits();
    Require(quotient.quot == 3 && quotient.rem == 1);
    const auto negativeNumerator = div_nid_postfix(-7, 2);
    Require(negativeNumerator.quot == -3 && negativeNumerator.rem == -1);
    const auto negativeDenominator = div_nid_postfix(7, -2);
    Require(negativeDenominator.quot == -3 && negativeDenominator.rem == 1);
    const auto minimum = div_nid_postfix(std::numeric_limits<int>::min(), 10);
    Require(minimum.quot == -214748364 && minimum.rem == -8);
    CheckPowfSpecialValue();
    CheckPowfUnderflowFlag();
    Require(abs_nid_postfix(0) == 0);
    CheckAcosfErrno();
    CheckAcosfMsun();
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
    Require(__powidf2_nid_postfix(2.0, 10) == 1024.0);
    Require(__powidf2_nid_postfix(2.0, -10) == 0.0009765625);
    Require(__powidf2_nid_postfix(-2.0, 3) == -8.0);
    Require(__powidf2_nid_postfix(-2.0, -3) == -0.125);
    Require(__powidf2_nid_postfix(-2.0, 4) == 16.0);
    Require(__powidf2_nid_postfix(0.0, 0) == 1.0);
    Require(__powidf2_nid_postfix(std::numeric_limits<double>::quiet_NaN(), 0) == 1.0);
    Require(std::isnan(__powidf2_nid_postfix(std::numeric_limits<double>::quiet_NaN(), 1)));
    Require(__powidf2_nid_postfix(-1.0, std::numeric_limits<int>::min()) == 1.0);
    Require(__powidf2_nid_postfix(-1.0, std::numeric_limits<int>::max()) == -1.0);
    Require(__powidf2_nid_postfix(2.0, std::numeric_limits<int>::min()) == 0.0);
    Require(std::isinf(__powidf2_nid_postfix(0.0, -1)));
    Require(std::signbit(__powidf2_nid_postfix(-0.0, 3)));
    Require(std::isinf(__powidf2_nid_postfix(-0.0, -3)) && std::signbit(__powidf2_nid_postfix(-0.0, -3)));
    Require(__powidf2_nid_postfix(2.0, -127) == 0x1p-127);
    CheckHyperbolic();
    CheckAcoshfDomainEnvironment();
    CheckAcoshfMsunBoundaries();
    CheckCoshfMsunBoundaries();
    CheckExp2fConsoleBits();
    CheckScaledHyperbolic();
    CheckSinhf();
    CheckAtanhf();
    CheckLogfConsoleBits();
    CheckHypotSpecialValue();
    CheckAtanfConsoleBits();
    CheckAtan2fConsoleBits();
    CheckFloatUnderflowErrno();
    CheckLdexpfErrno();
    CheckFmodErrno();
    CheckFmodfErrno();
    CheckDoubleClassification();
    CheckIntegerAbsAndDivision();
    CheckScaling();
    CheckLongDoubleFunctions();
    CheckLongDoubleHyperbolic();
    CheckDinkumwareFloatCharacteristics();
    CheckFrexpfEnvironment();
}
