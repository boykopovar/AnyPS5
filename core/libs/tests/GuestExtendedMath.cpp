#include "prx/libc/include/general/VabiMacros.hpp"

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

extern "C" {
long double APS5_VABI acosl_nid_postfix(long double);
long double APS5_VABI asinl_nid_postfix(long double);
long double APS5_VABI atanl_nid_postfix(long double);
long double APS5_VABI atan2l_nid_postfix(long double, long double);
long double APS5_VABI tanl_nid_postfix(long double);
long double APS5_VABI acoshl_nid_postfix(long double);
long double APS5_VABI asinhl_nid_postfix(long double);
long double APS5_VABI atanhl_nid_postfix(long double);
long double APS5_VABI coshl_nid_postfix(long double);
long double APS5_VABI sinhl_nid_postfix(long double);
long double APS5_VABI tanhl_nid_postfix(long double);
long double APS5_VABI expl_nid_postfix(long double);
long double APS5_VABI exp2l_nid_postfix(long double);
long double APS5_VABI expm1l_nid_postfix(long double);
long double APS5_VABI log10l_nid_postfix(long double);
long double APS5_VABI log1pl_nid_postfix(long double);
long double APS5_VABI log2l_nid_postfix(long double);
long double APS5_VABI logbl_nid_postfix(long double);
long double APS5_VABI sqrtl_nid_postfix(long double);
long double APS5_VABI cbrtl_nid_postfix(long double);
long double APS5_VABI hypotl_nid_postfix(long double, long double);
long double APS5_VABI fabsl_nid_postfix(long double);
long double APS5_VABI copysignl_nid_postfix(long double, long double);
long double APS5_VABI fdiml_nid_postfix(long double, long double);
long double APS5_VABI fmaxl_nid_postfix(long double, long double);
long double APS5_VABI fminl_nid_postfix(long double, long double);
long double APS5_VABI fmal_nid_postfix(long double, long double, long double);
long double APS5_VABI ceill_nid_postfix(long double);
long double APS5_VABI floorl_nid_postfix(long double);
long double APS5_VABI roundl_nid_postfix(long double);
std::int64_t APS5_VABI lroundl_nid_postfix(long double);
std::int64_t APS5_VABI llroundl_nid_postfix(long double);
long double APS5_VABI nearbyintl_nid_postfix(long double);
long double APS5_VABI fmodl_nid_postfix(long double, long double);
long double APS5_VABI remainderl_nid_postfix(long double, long double);
long double APS5_VABI remquol_nid_postfix(long double, long double, int*);
long double APS5_VABI nextafterl_nid_postfix(long double, long double);
long double APS5_VABI nexttowardl_nid_postfix(long double, long double);
long double APS5_VABI frexpl_nid_postfix(long double, int*);
long double APS5_VABI ldexpl_nid_postfix(long double, int);
long double APS5_VABI modfl_nid_postfix(long double, long double*);
long double APS5_VABI scalbnl_nid_postfix(long double, int);
long double APS5_VABI scalblnl_nid_postfix(long double, std::int64_t);
}

namespace {

constexpr long double pi = 3.141592653589793238462643383279502884L;
constexpr long double infinity = std::numeric_limits<long double>::infinity();
constexpr long double nan = std::numeric_limits<long double>::quiet_NaN();
constexpr long double delta = 0x1p-60L;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Guest extended math: %s\n", message);
        std::abort();
    }
}

void Near(long double actual, long double expected, const char* message) {
    const auto tolerance = 16 * std::numeric_limits<long double>::epsilon() * std::fabs(expected);
    Require(std::isfinite(actual) && std::fabs(actual - expected) <= tolerance, message);
}

void CheckTranscendentals() {
    struct Case {
        long double (APS5_VABI *function)(long double);
        long double input;
        long double expected;
        const char* name;
    };
    const Case cases[] = {
        {acosl_nid_postfix, 0.5L, pi / 3, "acosl"},
        {asinl_nid_postfix, 0.5L, pi / 6, "asinl"},
        {atanl_nid_postfix, 1.L, pi / 4, "atanl"},
        {tanl_nid_postfix, pi / 4, 1.L, "tanl"},
        {acoshl_nid_postfix, 1.25L, 0.693147180559945309417232121458176568L, "acoshl"},
        {asinhl_nid_postfix, 0.75L, 0.693147180559945309417232121458176568L, "asinhl"},
        {atanhl_nid_postfix, 0.6L, 0.693147180559945309417232121458176568L, "atanhl"},
        {coshl_nid_postfix, 1.L, 1.54308063481524377847790562075706168L, "coshl"},
        {sinhl_nid_postfix, 1.L, 1.17520119364380145688238185059560082L, "sinhl"},
        {tanhl_nid_postfix, 1.L, 0.76159415595576488811945828260479359L, "tanhl"},
        {expl_nid_postfix, 1.L, 2.71828182845904523536028747135266250L, "expl"},
        {exp2l_nid_postfix, 0.5L, 1.41421356237309504880168872420969808L, "exp2l"},
        {expm1l_nid_postfix, delta, delta, "expm1l tiny input"},
        {log10l_nid_postfix, 1000.L, 3.L, "log10l"},
        {log1pl_nid_postfix, delta, delta, "log1pl tiny input"},
        {log2l_nid_postfix, 8.L, 3.L, "log2l"},
        {logbl_nid_postfix, 0x1.8p12000L, 12000.L, "logbl extended exponent"},
        {sqrtl_nid_postfix, 2.L, 1.41421356237309504880168872420969808L, "sqrtl"},
        {cbrtl_nid_postfix, -27.L, -3.L, "cbrtl"},
    };
    for (const auto& test : cases) {
        Near(test.function(test.input), test.expected, test.name);
        Require(std::isnan(test.function(nan)), test.name);
    }
    Near(atan2l_nid_postfix(1.L, -1.L), 3 * pi / 4, "atan2l quadrant");
    Near(atan2l_nid_postfix(-0.L, -1.L), -pi, "atan2l signed zero");
    Near(hypotl_nid_postfix(3.L, 4.L), 5.L, "hypotl");
    Require(hypotl_nid_postfix(0x1.8p12001L, 0x1p12002L) == 0x1.4p12002L, "hypotl extended range");
    Require(std::isinf(hypotl_nid_postfix(infinity, nan)), "hypotl infinity and NaN");
    Require(std::isnan(atan2l_nid_postfix(nan, 1.L)), "atan2l NaN");
    Require(expl_nid_postfix(-infinity) == 0 && std::isinf(expl_nid_postfix(infinity)), "expl infinity");
    Require(exp2l_nid_postfix(12000.L) == 0x1p12000L, "exp2l extended range");
    Require(std::isfinite(expl_nid_postfix(1000.L)) && expl_nid_postfix(1000.L) > 0x1p1024L, "expl extended range");
    Require(sqrtl_nid_postfix(0x1p12000L) == 0x1p6000L, "sqrtl extended range");
    Require(log2l_nid_postfix(0x1p12000L) == 12000.L, "log2l extended range");
    Near(log1pl_nid_postfix(delta), delta, "log1pl cancellation");
    Near(expm1l_nid_postfix(-delta), -delta, "expm1l cancellation");
    Require(std::signbit(sqrtl_nid_postfix(-0.L)), "sqrtl signed zero");
    Require(std::signbit(cbrtl_nid_postfix(-0.L)), "cbrtl signed zero");
}

void CheckArithmetic() {
    const long double value = 1.L + delta;
    Require(fabsl_nid_postfix(-value) == value && !std::signbit(fabsl_nid_postfix(-0.L)), "fabsl precision and sign");
    Require(copysignl_nid_postfix(value, -0.L) == -value, "copysignl precision");
    Require(std::signbit(copysignl_nid_postfix(0.L, -1.L)), "copysignl negative zero");
    Require(std::isnan(copysignl_nid_postfix(nan, -1.L)) && std::signbit(copysignl_nid_postfix(nan, -1.L)), "copysignl NaN sign");
    Require(fdiml_nid_postfix(value, 1.L) == delta, "fdiml precision");
    Require(fdiml_nid_postfix(1.L, value) == 0 && !std::signbit(fdiml_nid_postfix(1.L, value)), "fdiml positive zero");
    Require(std::isnan(fdiml_nid_postfix(nan, 1.L)), "fdiml NaN");
    Require(fmaxl_nid_postfix(1.L, value) == value && fminl_nid_postfix(1.L, value) == 1.L, "min/max precision");
    for (const auto function : {fmaxl_nid_postfix, fminl_nid_postfix}) {
        Require(function(nan, value) == value && function(value, nan) == value, "min/max one NaN");
        Require(std::isnan(function(nan, nan)), "min/max both NaN");
    }
    Require(fmal_nid_postfix(1.L + 0x1p-32L, 1.L - 0x1p-32L, -1.L) == -0x1p-64L, "fmal must round only once");
    Require(fmal_nid_postfix(0x1p12000L, 0x1p-12000L, delta) == value, "fmal extended range and precision");
    Require(std::isnan(fmal_nid_postfix(infinity, 0.L, 1.L)), "fmal invalid product");
}

void CheckRounding() {
    const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    const long double nearby[] = {2.L, 1.L, 2.L, 1.L};
    const long double negativeNearby[] = {-2.L, -2.L, -1.L, -1.L};
    for (std::size_t mode = 0; mode < 4; ++mode) {
        Require(std::fesetround(modes[mode]) == 0, "set rounding mode");
        Require(ceill_nid_postfix(1.L + delta) == 2.L && ceill_nid_postfix(-1.L - delta) == -1.L, "ceill precision");
        Require(floorl_nid_postfix(1.L + delta) == 1.L && floorl_nid_postfix(-1.L - delta) == -2.L, "floorl precision");
        Require(roundl_nid_postfix(2.5L) == 3.L && roundl_nid_postfix(-2.5L) == -3.L, "roundl ties away");
        Require(roundl_nid_postfix(2.5L - delta) == 2.L, "roundl extended precision");
        for (const auto function : {lroundl_nid_postfix, llroundl_nid_postfix}) {
            Require(function(2.5L) == 3 && function(-2.5L) == -3, "integer round ties away");
            Require(function(0x1p62L + 1.5L) == INT64_C(4611686018427387906), "integer round extended precision");
            Require(function(0x1p63L - 1.L) == INT64_MAX && function(-0x1p63L) == INT64_MIN, "integer round 64-bit boundaries");
            std::feclearexcept(FE_ALL_EXCEPT);
            (void)function(infinity);
            Require(std::fetestexcept(FE_INVALID) != 0, "integer round invalid");
        }
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(nearbyintl_nid_postfix(1.5L) == nearby[mode], "nearbyintl mode");
        Require(nearbyintl_nid_postfix(-1.5L) == negativeNearby[mode], "nearbyintl negative mode");
        Require(std::fetestexcept(FE_INEXACT) == 0, "nearbyintl must not raise inexact");
        std::feraiseexcept(FE_INEXACT);
        (void)nearbyintl_nid_postfix(1.5L);
        Require(std::fetestexcept(FE_INEXACT) != 0, "nearbyintl must preserve existing inexact");
        for (const auto function : {ceill_nid_postfix, floorl_nid_postfix, roundl_nid_postfix, nearbyintl_nid_postfix}) {
            Require(function(-0.L) == 0 && std::signbit(function(-0.L)), "rounding signed zero");
            Require(function(infinity) == infinity && function(-infinity) == -infinity, "rounding infinity");
            Require(std::isnan(function(nan)), "rounding NaN");
        }
        Require(std::fegetround() == modes[mode], "rounding mode preserved");
    }
    Require(std::fesetround(FE_TONEAREST) == 0, "restore nearest rounding");
}

void CheckRemaindersAndNeighbors() {
    Require(fmodl_nid_postfix(7.L, 2.L) == 1.L && remainderl_nid_postfix(7.L, 2.L) == -1.L, "remainder quotient rules");
    Require(fmodl_nid_postfix(-7.L, 2.L) == -1.L && remainderl_nid_postfix(-7.L, 2.L) == 1.L, "negative remainder rules");
    for (const auto function : {fmodl_nid_postfix, remainderl_nid_postfix}) {
        Require(function(1.L + delta, 1.L) == delta, "remainder precision");
        Require(function(-0.L, 2.L) == 0 && std::signbit(function(-0.L, 2.L)), "remainder zero sign");
        Require(function(3.L, infinity) == 3.L, "remainder infinite divisor");
        Require(std::isnan(function(nan, 2.L)), "remainder NaN");
    }
    int quotient = 0;
    Require(remquol_nid_postfix(7.L, 2.L, &quotient) == -1.L && quotient > 0 && (quotient & 7) == 4, "remquol quotient bits");
    Require(remquol_nid_postfix(-7.L, 2.L, &quotient) == 1.L && quotient < 0 && ((-quotient) & 7) == 4, "remquol negative quotient");
    Require(remquol_nid_postfix(1.L + delta, 1.L, &quotient) == delta && quotient == 1, "remquol precision");
    for (const auto function : {nextafterl_nid_postfix, nexttowardl_nid_postfix}) {
        Require(function(1.L, 2.L) == 1.L + 0x1p-63L, "next value above one");
        Require(function(1.L, 0.L) == 1.L - 0x1p-64L, "next value below one");
        Require(function(0.L, 1.L) == std::numeric_limits<long double>::denorm_min(), "next value above zero");
        Require(std::signbit(function(0.L, -0.L)), "neighbor equality preserves target sign");
        Require(function(infinity, 0.L) == std::numeric_limits<long double>::max(), "neighbor of infinity");
        Require(std::isnan(function(nan, 1.L)), "neighbor NaN");
    }
}

void CheckDecompositionAndScaling() {
    int exponent = 0;
    Require(frexpl_nid_postfix(0x1.8p12000L, &exponent) == 0.75L && exponent == 12001, "frexpl extended range");
    Require(frexpl_nid_postfix(std::numeric_limits<long double>::denorm_min(), &exponent) == 0.5L && exponent == -16444, "frexpl subnormal");
    Require(frexpl_nid_postfix(1.L + delta, &exponent) == 0.5L + delta / 2 && exponent == 1, "frexpl precision");
    Require(std::signbit(frexpl_nid_postfix(-0.L, &exponent)) && exponent == 0, "frexpl signed zero");
    long double integral = 0;
    Require(modfl_nid_postfix(1.L + delta, &integral) == delta && integral == 1.L, "modfl precision");
    Require(modfl_nid_postfix(-1.L - delta, &integral) == -delta && integral == -1.L, "modfl negative parts");
    Require(modfl_nid_postfix(-infinity, &integral) == 0 && std::signbit(modfl_nid_postfix(-infinity, &integral)) && integral == -infinity, "modfl infinity");
    Require(std::isnan(modfl_nid_postfix(nan, &integral)) && std::isnan(integral), "modfl NaN");
    for (const auto function : {ldexpl_nid_postfix, scalbnl_nid_postfix}) {
        Require(function(1.L + delta, 12000) == 0x1p12000L + 0x1p11940L, "scaling extended range and precision");
        Require(function(1.L, -16445) == std::numeric_limits<long double>::denorm_min(), "scaling subnormal");
        Require(std::signbit(function(-0.L, 12000)), "scaling signed zero");
        Require(function(infinity, -12000) == infinity && std::isnan(function(nan, 12000)), "scaling infinity and NaN");
    }
    Require(scalblnl_nid_postfix(1.L + delta, 12000) == 0x1p12000L + 0x1p11940L, "scalblnl precision");
    Require(scalblnl_nid_postfix(1.L, -16445) == std::numeric_limits<long double>::denorm_min(), "scalblnl subnormal");
    for (const auto power : {INT64_C(4294967296), INT64_MAX}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::isinf(scalblnl_nid_postfix(1.L, power)), "scalblnl must not narrow positive exponent");
        Require(std::fetestexcept(FE_OVERFLOW) != 0, "scalblnl overflow flag");
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(scalblnl_nid_postfix(-1.L, -power) == 0 && std::signbit(scalblnl_nid_postfix(-1.L, -power)), "scalblnl must not narrow negative exponent");
        Require(std::fetestexcept(FE_UNDERFLOW) != 0, "scalblnl underflow flag");
    }
    Require(scalblnl_nid_postfix(infinity, INT64_MIN) == infinity, "scalblnl infinite input");
    Require(std::signbit(scalblnl_nid_postfix(-0.L, INT64_MAX)), "scalblnl zero input");
}

void CheckExceptions() {
    struct Case {
        long double (APS5_VABI *function)(long double);
        long double input;
        int flag;
        const char* name;
    };
    const Case cases[] = {
        {acosl_nid_postfix, 2.L, FE_INVALID, "acosl domain"},
        {asinl_nid_postfix, 2.L, FE_INVALID, "asinl domain"},
        {acoshl_nid_postfix, 0.L, FE_INVALID, "acoshl domain"},
        {atanhl_nid_postfix, 2.L, FE_INVALID, "atanhl domain"},
        {sqrtl_nid_postfix, -1.L, FE_INVALID, "sqrtl domain"},
        {log10l_nid_postfix, -1.L, FE_INVALID, "log10l domain"},
        {log2l_nid_postfix, -1.L, FE_INVALID, "log2l domain"},
        {log1pl_nid_postfix, -2.L, FE_INVALID, "log1pl domain"},
        {tanl_nid_postfix, infinity, FE_INVALID, "tanl infinite input"},
        {atanhl_nid_postfix, 1.L, FE_DIVBYZERO, "atanhl pole"},
        {log10l_nid_postfix, 0.L, FE_DIVBYZERO, "log10l pole"},
        {log2l_nid_postfix, 0.L, FE_DIVBYZERO, "log2l pole"},
        {log1pl_nid_postfix, -1.L, FE_DIVBYZERO, "log1pl pole"},
        {logbl_nid_postfix, 0.L, FE_DIVBYZERO, "logbl pole"},
        {expl_nid_postfix, 20000.L, FE_OVERFLOW, "expl overflow"},
        {exp2l_nid_postfix, 20000.L, FE_OVERFLOW, "exp2l overflow"},
        {expm1l_nid_postfix, 20000.L, FE_OVERFLOW, "expm1l overflow"},
        {coshl_nid_postfix, 20000.L, FE_OVERFLOW, "coshl overflow"},
        {sinhl_nid_postfix, 20000.L, FE_OVERFLOW, "sinhl overflow"},
    };
    bool success = true;
    for (const auto& test : cases) {
        std::feclearexcept(FE_ALL_EXCEPT);
        const auto result = test.function(test.input);
        const auto flags = std::fetestexcept(FE_ALL_EXCEPT);
        if (!(flags & test.flag) || (test.flag == FE_INVALID ? !std::isnan(result) : !std::isinf(result))) {
            std::fprintf(stderr, "%s: flags %x, NaN %d, infinite %d\n", test.name, flags, std::isnan(result), std::isinf(result));
            success = false;
        }
    }
    Require(success, "floating exception cases");
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(std::isnan(fmodl_nid_postfix(1.L, 0.L)) && std::fetestexcept(FE_INVALID), "fmodl invalid divisor");
}

}

int main() {
    std::fenv_t saved;
    Require(std::fegetenv(&saved) == 0, "save floating environment");
    Require(std::fesetround(FE_TONEAREST) == 0, "set nearest rounding");
    CheckTranscendentals();
    CheckArithmetic();
    CheckRounding();
    CheckRemaindersAndNeighbors();
    CheckDecompositionAndScaling();
    CheckExceptions();
    Require(std::fesetenv(&saved) == 0, "restore floating environment");
    return 0;
}
