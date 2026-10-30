#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cfenv>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), location.line());
    std::abort();
}

[[gnu::noinline]] static long double Extended(std::uint16_t signExponent, std::uint64_t mantissa) {
    long double value = 0;
    unsigned char bytes[sizeof(long double)] = {};
    std::memcpy(bytes, &mantissa, sizeof(mantissa));
    std::memcpy(bytes + 8, &signExponent, sizeof(signExponent));
    std::memcpy(&value, bytes, sizeof(bytes));
    return value;
}

static bool SameBits(long double left, long double right) { return std::memcmp(&left, &right, 10) == 0; }

static void CheckRoundingAndScaling() {
    Require(copysignl_nid_postfix(3.0L, -1.0L) == -3.0L && copysignl_nid_postfix(-3.0L, 0.0L) == 3.0L);
    Require(std::signbit(copysignl_nid_postfix(0.0L, -0.0L)) && !std::signbit(copysignl_nid_postfix(-0.0L, 2.0L)));
    Require(sqrtl_nid_postfix(6.25L) == 2.5L && std::isnan(sqrtl_nid_postfix(-1.0L)));
    Require(sqrtl_nid_postfix(-0.0L) == 0.0L && std::signbit(sqrtl_nid_postfix(-0.0L)));

    const int saved = std::fegetround();
    const struct {
        int mode;
        long long half;
        long long negativeHalf;
    } modes[] = {{FE_TONEAREST, 2, -2}, {FE_UPWARD, 3, -2}, {FE_DOWNWARD, 2, -3}, {FE_TOWARDZERO, 2, -2}};
    for (const auto& mode : modes) {
        Require(std::fesetround(mode.mode) == 0);
        errno = 0;
        Require(lrintl_nid_postfix(2.5L) == mode.half && lrintl_nid_postfix(-2.5L) == mode.negativeHalf);
        Require(llrintl_nid_postfix(2.5L) == mode.half && llrintl_nid_postfix(-2.5L) == mode.negativeHalf);
        Require(lrintf_nid_postfix(2.5f) == mode.half && lrintf_nid_postfix(-2.5f) == mode.negativeHalf);
        Require(llrintf_nid_postfix(2.5f) == mode.half && llrintf_nid_postfix(-2.5f) == mode.negativeHalf);
        Require(errno == 0);
    }
    std::fesetround(saved);
    Require(llrintl_nid_postfix(3.5L) == 4 && lrintf_nid_postfix(16777216.0f) == 16777216);

    Require(scalbf_nid_postfix(1.5f, 3.0f) == 12.0f && scalbf_nid_postfix(12.0f, -2.0f) == 3.0f);
    Require(std::isnan(scalbf_nid_postfix(1.0f, 0.5f)) && std::isnan(scalbf_nid_postfix(NAN, 1.0f)));
    Require(std::isinf(scalbf_nid_postfix(1.0f, INFINITY)) && scalbf_nid_postfix(8.0f, -INFINITY) == 0.0f);
    Require(scalblnf_nid_postfix(1.0f, 10) == 1024.0f && scalblnf_nid_postfix(1024.0f, -10) == 1.0f);
    Require(std::isinf(scalblnf_nid_postfix(1.0f, INT64_MAX)) && scalblnf_nid_postfix(1.0f, INT64_MIN) == 0.0f);

    Require(__isinfl_nid_postfix(static_cast<long double>(INFINITY)) == 1 && __isinfl_nid_postfix(-static_cast<long double>(INFINITY)) == 1);
    Require(__isinfl_nid_postfix(1.0L) == 0 && __isinfl_nid_postfix(static_cast<long double>(NAN)) == 0 && __isinfl_nid_postfix(0.0L) == 0);
}

static void CheckSignalingNanPassThrough() {
    const long double signaling = Extended(0x7fff, 0x8000000000001234ull);
    Require(SameBits(fdiml_nid_postfix(signaling, 1.0L), signaling));
    Require(SameBits(fdiml_nid_postfix(1.0L, signaling), signaling));
    Require(SameBits(roundl_nid_postfix(signaling), signaling));
    long double integral = 0;
    Require(SameBits(modfl_nid_postfix(signaling, &integral), signaling) && SameBits(integral, signaling));
    Require(SameBits(fmal_nid_postfix(1.0L, 1.0L, signaling), signaling));
    Require(SameBits(nanl_nid_postfix("123"), Extended(0x7fff, 0xc000000000000000ull)));
}

static void CheckErrno() {
    errno = 0;
    Require(std::isnan(fmodl_nid_postfix(NAN, 1.0L)) && errno == EDOM);
    errno = 0;
    Require(fmodl_nid_postfix(7.0L, 2.0L) == 1.0L && errno == 0);
    int quotient = 0;
    errno = 0;
    Require(std::isnan(remquol_nid_postfix(1.0L, 0.0L, &quotient)) && errno == EDOM);
    errno = 0;
    remquol_nid_postfix(NAN, 0.0L, &quotient);

    Require(errno == 0);
    Require(remquol_nid_postfix(-7.0L, 2.0L, &quotient) == 1.0L && quotient == -4);
    errno = 0;
    Require(std::isnan(scalb_nid_postfix(1.0, 2.5)) && errno == 0);
    Require(scalb_nid_postfix(1.0, 3.0) == 8.0 && scalb_nid_postfix(4.0, -INFINITY) == 0.0);
    errno = 0;
    Require(std::isnan(drem_nid_postfix(1.0, 0.0)) && errno == EDOM);
    errno = 0;
    Require(std::isnan(drem_nid_postfix(INFINITY, 1.0)) && errno == EDOM);
    errno = 0;
    Require(std::isnan(dremf_nid_postfix(1.0f, 0.0f)) && errno == EDOM);
    errno = 0;
    Require(std::isnan(remainderl_nid_postfix(1.0L, 0.0L)) && errno == EDOM);
    errno = 0;
    Require(std::isnan(remainderl_nid_postfix(INFINITY, 1.0L)) && errno == EDOM);
    errno = 0;
    Require(std::isnan(remainderl_nid_postfix(NAN, 0.0L)) && errno == 0);
    Require(remainderl_nid_postfix(7.0L, 2.0L) == -1.0L && errno == 0);
    Require(drem_nid_postfix(7.0, 2.0) == -1.0 && lrint_nid_postfix(2.5) == 2);
}

static void CheckConsoleQuirks() {
    Require(ilogbl_nid_postfix(NAN) == INT_MAX && ilogbl_nid_postfix(0.0L) == INT_MIN && ilogbl_nid_postfix(8.0L) == 3);
    Require(scalblnl_nid_postfix(1.0L, INT64_C(0x100000001)) == 2.0L && scalblnl_nid_postfix(1.0L, INT64_MAX) == 0.5L);
    Require(scalbln_nid_postfix(1.0, INT64_C(0x100000001)) == INFINITY && scalbln_nid_postfix(1.0, INT64_MIN) == 0.0);
    Require(lrint_nid_postfix(0x1p40) == INT64_C(0x10000000000));
    for (const double outside : {0x1p63, -0x1p64, 1e300, static_cast<double>(INFINITY), -static_cast<double>(INFINITY), static_cast<double>(NAN)}) {
        Require(lrint_nid_postfix(outside) == INT64_MIN && llrint_nid_postfix(outside) == LLONG_MIN);
        Require(lrintf_nid_postfix(static_cast<float>(outside)) == INT64_MIN && llrintf_nid_postfix(static_cast<float>(outside)) == LLONG_MIN);
    }
    Require(lrint_nid_postfix(-0x1p63) == INT64_MIN && lrintf_nid_postfix(0x1.fffffep62f) == INT64_C(0x7fffff8000000000));
    const long double product = Extended(0x4000, 0xe000000000000000ull) * Extended(0x4001, 0xda76b022816f6a94ull);
    Require(SameBits(fmal_nid_postfix(Extended(0x4000, 0xe000000000000000ull), Extended(0x4001, 0xda76b022816f6a94ull), Extended(0x9b45, 0xd3b49eac84283c95ull)), product));
    Require(finite_nid_postfix(1.0) == 1 && finite_nid_postfix(INFINITY) == 0);
}

static void CheckRoundingModes() {
    struct Case {
        int mode;
        double negative;
        double positive;
    };
    const Case cases[] = {{FE_TONEAREST, -2.0, 2.0}, {FE_DOWNWARD, -2.0, 1.0}, {FE_UPWARD, -1.0, 2.0}, {FE_TOWARDZERO, -1.0, 1.0}};
    const int saved = std::fegetround();
    for (const auto& test : cases) {
        Require(std::fesetround(test.mode) == 0);
        Require(rint_nid_postfix(-1.5) == test.negative && rint_nid_postfix(1.5) == test.positive);
        Require(rintf_nid_postfix(-1.5f) == static_cast<float>(test.negative) && rintf_nid_postfix(1.5f) == static_cast<float>(test.positive));
        Require(rintl_nid_postfix(-1.5L) == test.negative && rintl_nid_postfix(1.5L) == test.positive);
        Require(lrint_nid_postfix(-1.5) == static_cast<std::int64_t>(test.negative) && llrint_nid_postfix(1.5) == static_cast<long long>(test.positive));
        Require(std::fegetround() == test.mode);
    }
    Require(std::fesetround(saved) == 0);
}

static void CheckLongDoubleExpOverflowRounding() {
    struct Case {
        int mode;
        long double multiplier;
        bool infinity;
    };
    const Case cases[] = {
        {FE_TONEAREST, 1.0L, true},
        {FE_TONEAREST, -1.0L, true},
        {FE_TOWARDZERO, 1.0L, false},
        {FE_TOWARDZERO, -1.0L, false},
        {FE_DOWNWARD, 1.0L, false},
        {FE_DOWNWARD, -1.0L, true},
        {FE_UPWARD, 1.0L, true},
        {FE_UPWARD, -1.0L, false},
    };
    const long double maximum = std::numeric_limits<long double>::max();
    const int saved = std::fegetround();
    for (const auto& test : cases) {
        Require(std::fesetround(test.mode) == 0);
        errno = 0;
        std::feclearexcept(FE_ALL_EXCEPT);
        long double result = 1.0L;
        const short kind = _LExp_nid_postfix(&result, test.multiplier, 16384);
        const long double expected = test.infinity
            ? std::copysign(std::numeric_limits<long double>::infinity(), test.multiplier)
            : std::copysign(maximum, test.multiplier);
        Require(SameBits(result, expected));
        Require(kind == (test.infinity ? 1 : -1));
        Require(errno == ERANGE && (std::fetestexcept(FE_ALL_EXCEPT) & FE_OVERFLOW) != 0);
    }
    Require(std::fesetround(FE_TOWARDZERO) == 0);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    long double result = 30000.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, 0) == -1 && SameBits(result, maximum) && errno == ERANGE && (std::fetestexcept(FE_ALL_EXCEPT) & FE_OVERFLOW) != 0);
    result = 30000.0L;
    Require(_LExp_nid_postfix(&result, -1.0L, 0) == -1 && SameBits(result, -maximum) && errno == ERANGE && (std::fetestexcept(FE_ALL_EXCEPT) & FE_OVERFLOW) != 0);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = -30000.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, 0) == 0 && result == 0.0L && errno == ERANGE && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_UNDERFLOW | FE_INEXACT)) == (FE_UNDERFLOW | FE_INEXACT));
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = -30000.0L;
    Require(_LExp_nid_postfix(&result, -1.0L, 0) == 0 && result == 0.0L && std::signbit(result) && errno == ERANGE && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_UNDERFLOW | FE_INEXACT)) == (FE_UNDERFLOW | FE_INEXACT));
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, -16400) == -1 && __fpclassifyl_nid_postfix(result) == -2 && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    Require(std::fesetround(saved) == 0);
}

static void CheckLongDoubleExpCompensatingRange() {
    struct Case {
        long double x;
        long double multiplier;
        short scale;
        long double expected;
    };
    const Case cases[] = {
        {30000.0L, 1.0L, -32768, 0xe.6ea97839ad67302p+10509L},
        {-30000.0L, 1.0L, 32767, 0x8.de778b1319b01b4p-10517L},
        {1.0L, std::numeric_limits<long double>::max(), -2, 0xa.df85458a2bb4a9ap+16380L},
        {30000.0L, -1.0L, -32768, -0xe.6ea97839ad67302p+10509L},
        {-30000.0L, -1.0L, 32767, -0x8.de778b1319b01b4p-10517L},
        {1.0L, -std::numeric_limits<long double>::max(), -2, -0xa.df85458a2bb4a9ap+16380L},
    };
    for (const auto& test : cases) {
        errno = 0;
        std::feclearexcept(FE_ALL_EXCEPT);
        long double result = test.x;
        const short kind = _LExp_nid_postfix(&result, test.multiplier, test.scale);
        Require(kind == -1);
    Require(std::fabs(result - test.expected) <= std::fabs(test.expected) * 2e-17L);

        Require(__fpclassifyl_nid_postfix(result) == -1);
        Require(errno == 0);
        const int raised = std::fetestexcept(FE_ALL_EXCEPT);
        Require((raised & (FE_OVERFLOW | FE_UNDERFLOW | FE_INVALID | FE_DIVBYZERO)) == 0);
        Require((raised & FE_INEXACT) != 0);
    }
}

static void CheckLongDoubleExpTinyMultiplierOverflow() {
    const long double tiny = std::numeric_limits<long double>::denorm_min();
    const int saved = std::fegetround();
    for (const int mode : {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
        Require(std::fesetround(mode) == 0);
        for (const long double multiplier : {tiny, -tiny}) {
            const bool negative = std::signbit(multiplier);
            const bool infinite = mode == FE_TONEAREST || (mode == FE_UPWARD && !negative) || (mode == FE_DOWNWARD && negative);
            const long double expected = std::copysign(infinite
                ? std::numeric_limits<long double>::infinity()
                : std::numeric_limits<long double>::max(), multiplier);
            for (const long double x : {50000.0L, 50001.0L}) {
                errno = 0;
                std::feclearexcept(FE_ALL_EXCEPT);
                long double result = x;
                const short kind = _LExp_nid_postfix(&result, multiplier, 0);
                Require(SameBits(result, expected));
                Require(kind == (infinite ? 1 : -1));
                Require(errno == ERANGE);
                const int raised = std::fetestexcept(FE_ALL_EXCEPT);
                Require((raised & (FE_OVERFLOW | FE_INEXACT)) == (FE_OVERFLOW | FE_INEXACT));
                Require((raised & (FE_UNDERFLOW | FE_INVALID | FE_DIVBYZERO)) == 0);
            }
        }
    }
    Require(std::fesetround(saved) == 0);
}

static void CheckLongDoubleExpTinyRounding() {
    struct Case {
        int mode;
        long double multiplier;
        long double x;
        short scale;
        bool away;
    };
    const Case cases[] = {
        {FE_TONEAREST, 1.0L, 0.0L, -20000, false},
        {FE_TOWARDZERO, 1.0L, 0.0L, -20000, false},
        {FE_DOWNWARD, 1.0L, 0.0L, -20000, false},
        {FE_UPWARD, 1.0L, 0.0L, -20000, true},
        {FE_TONEAREST, -1.0L, 0.0L, -20000, false},
        {FE_TOWARDZERO, -1.0L, 0.0L, -20000, false},
        {FE_UPWARD, -1.0L, 0.0L, -20000, false},
        {FE_DOWNWARD, -1.0L, 0.0L, -20000, true},
        {FE_TONEAREST, 1.0L, -0.001L, -16446, false},
        {FE_TOWARDZERO, 1.0L, -0.001L, -16446, false},
        {FE_DOWNWARD, 1.0L, -0.001L, -16446, false},
        {FE_UPWARD, 1.0L, -0.001L, -16446, true},
        {FE_TONEAREST, -1.0L, -0.001L, -16446, false},
        {FE_TOWARDZERO, -1.0L, -0.001L, -16446, false},
        {FE_UPWARD, -1.0L, -0.001L, -16446, false},
        {FE_DOWNWARD, -1.0L, -0.001L, -16446, true},
    };
    const long double denormMin = std::numeric_limits<long double>::denorm_min();
    const int saved = std::fegetround();
    for (const auto& test : cases) {
        Require(std::fesetround(test.mode) == 0);
        errno = 0;
        std::feclearexcept(FE_ALL_EXCEPT);
        long double result = test.x;
        const short kind = _LExp_nid_postfix(&result, test.multiplier, test.scale);
        const long double expected = test.away
            ? std::copysign(denormMin, test.multiplier)
            : std::copysign(0.0L, test.multiplier);
    Require(kind == (test.away ? -1 : 0));
        Require(errno == (test.away ? 0 : ERANGE) && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_UNDERFLOW | FE_INEXACT)) == (test.away ? 0 : (FE_UNDERFLOW | FE_INEXACT)));
    }
    Require(std::fesetround(saved) == 0);
}

static void CheckClassification() {
    const long double pseudoDenormal = Extended(0x0000, 0x8000000000000000ull);
    Require(_LDtest_nid_postfix(&pseudoDenormal) == -1 && __fpclassifyl_nid_postfix(pseudoDenormal) == -2);
    for (const long double unnormal : {Extended(0x3fff, 0x4000000000000000ull), Extended(0x8001, 0), Extended(0x7ffe, 0x7fffffffffffffffull)}) {
        Require(_LDtest_nid_postfix(&unnormal) == -2 && _LDclass_nid_postfix(unnormal) == -2);
        Require(__fpclassifyl_nid_postfix(unnormal) == -1 && __isnormall_nid_postfix(unnormal) == 1);
    }
    const long double zero = 0.0L, infinity = INFINITY, nan = NAN, denormal = Extended(0, 1);
    Require(_LDtest_nid_postfix(&zero) == 0 && _LDtest_nid_postfix(&infinity) == 1 && _LDtest_nid_postfix(&nan) == 2 && _LDtest_nid_postfix(&denormal) == -2);
    Require(__fpclassifyl_nid_postfix(1.0L) == -1 && __fpclassifyl_nid_postfix(0.0L) == 0 && __fpclassifyl_nid_postfix(INFINITY) == 1);
    Require(__isnanl_nid_postfix(NAN) == 1 && __isnanl_nid_postfix(INFINITY) == 0);
    Require(__isnormall_nid_postfix(1.0L) == 1 && __isnormall_nid_postfix(denormal) == 0);
    Require(_Dclass_nid_postfix(5e-324) == -2 && _Dclass_nid_postfix(-0.0) == 0);
    Require(_LDsign_nid_postfix(-2.0L) == -32768 && _LDsign_nid_postfix(2.0L) == 0);
}

static void CheckLongDoubleExp() {
    const long double e = std::exp(1.0L);
    long double result = 1.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, 0) == -1);
    Require(std::fabs(result - e) <= std::fabs(e) * 1e-17L);
    result = 0.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, 0) == -1 && result == 1.0L);
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, 10) == -1);
    Require(std::fabs(result - e * 1024.0L) <= std::fabs(e * 1024.0L) * 1e-17L);
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, 3.0L, 0) == -1);
    Require(std::fabs(result - 3.0L * e) <= std::fabs(3.0L * e) * 1e-17L);
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, -2.0L, 0) == -1);
    Require(std::fabs(result + 2.0L * e) <= std::fabs(2.0L * e) * 1e-17L);
    result = 0.0L;
    Require(_LExp_nid_postfix(&result, 5.0L, 0) == -1 && result == 5.0L);
    for (int i = -200; i <= 200; ++i) {
        const long double x = static_cast<long double>(i) / 16.0L;
        const long double expected = std::exp(x);
        result = x;
        Require(_LExp_nid_postfix(&result, 1.0L, 0) == -1);
        Require(std::fabs(result - expected) <= std::fabs(expected) * 1e-17L);
    }
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, 0.0L, 0) == 0 && result == 0.0L);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = NAN;
    Require(_LExp_nid_postfix(&result, 0.0L, 0) == 2 && std::isnan(result) && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    result = NAN;
    Require(_LExp_nid_postfix(&result, 0.0L, 17) == 2 && std::isnan(result));
    result = 29491.0L;
    Require(_LExp_nid_postfix(&result, 0.0L, 0) == 0 && result == 0.0L);
    result = INFINITY;
    Require(_LExp_nid_postfix(&result, 0.0L, -17) == 2 && std::isnan(result) && errno == EDOM && (std::fetestexcept(FE_ALL_EXCEPT) & FE_INVALID) != 0);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = -INFINITY;
    Require(_LExp_nid_postfix(&result, INFINITY, 0) == 2 && std::isnan(result) && errno == EDOM && (std::fetestexcept(FE_ALL_EXCEPT) & FE_INVALID) != 0);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = INFINITY;
    Require(_LExp_nid_postfix(&result, -INFINITY, 0) == 1 && std::isinf(result) && result < 0.0L && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    result = -INFINITY;
    Require(_LExp_nid_postfix(&result, 1.0L, 0) == 0 && result == 0.0L);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = INFINITY;
    Require(_LExp_nid_postfix(&result, 1.0L, 0) == 1 && std::isinf(result) && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = NAN;
    Require(_LExp_nid_postfix(&result, 1.0L, 0) == 2 && std::isnan(result) && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, NAN, 0) == 2 && result == 1.0L && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, INFINITY, 0) == 1 && std::isinf(result) && result > 0.0L && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, -INFINITY, 0) == 1 && std::isinf(result) && result < 0.0L && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = INFINITY;
    Require(_LExp_nid_postfix(&result, NAN, 0) == 2 && result == INFINITY && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    result = 12000.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, 0) == 1 && std::isinf(result));
    result = -12000.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, 0) == 0 && result == 0.0L);
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, -20000) == 0 && result == 0.0L);
    result = 1.0L;
    Require(_LExp_nid_postfix(&result, 1.0L, 20000) == 1 && std::isinf(result));
}

int main() {
    CheckRoundingAndScaling();
    CheckSignalingNanPassThrough();
    CheckErrno();
    CheckConsoleQuirks();
    CheckRoundingModes();
    CheckLongDoubleExpOverflowRounding();
    CheckLongDoubleExpCompensatingRange();
    CheckLongDoubleExpTinyMultiplierOverflow();
    CheckLongDoubleExpTinyRounding();
    CheckClassification();
    CheckLongDoubleExp();
    return 0;
}
