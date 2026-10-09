#include "prx/libc/include/general/VabiMacros.hpp"

#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

extern "C" {
float APS5_VABI sqrtf_nid_postfix(float);
float APS5_VABI fabsf_nid_postfix(float);
float APS5_VABI ceilf_nid_postfix(float);
float APS5_VABI truncf_nid_postfix(float);
double APS5_VABI nearbyint_nid_postfix(double);
double APS5_VABI remquo_nid_postfix(double, double, int*);
float APS5_VABI remquof_nid_postfix(float, float, int*);
double APS5_VABI copysign_nid_postfix(double, double);
float APS5_VABI copysignf_nid_postfix(float, float);
double APS5_VABI nan_nid_postfix(const char*);
float APS5_VABI nanf_nid_postfix(const char*);
long double APS5_VABI nanl_nid_postfix(const char*);
double APS5_VABI nextafter_nid_postfix(double, double);
double APS5_VABI nexttoward_nid_postfix(double, long double);
float APS5_VABI nexttowardf_nid_postfix(float, long double);
double APS5_VABI fdim_nid_postfix(double, double);
float APS5_VABI fdimf_nid_postfix(float, float);
double APS5_VABI fmax_nid_postfix(double, double);
float APS5_VABI fmaxf_nid_postfix(float, float);
double APS5_VABI fmin_nid_postfix(double, double);
float APS5_VABI fminf_nid_postfix(float, float);
float APS5_VABI fmaf_nid_postfix(float, float, float);
double APS5_VABI scalbln_nid_postfix(double, std::int64_t);
float APS5_VABI scalblnf_nid_postfix(float, std::int64_t);
}

namespace {

constexpr int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Guest numeric helpers: %s\n", message);
        std::abort();
    }
}

template<typename TValue>
void CheckExtrema(TValue (APS5_VABI *maximum)(TValue, TValue), TValue (APS5_VABI *minimum)(TValue, TValue)) {
    const auto nan = std::numeric_limits<TValue>::quiet_NaN();
    const auto signalingNan = std::numeric_limits<TValue>::signaling_NaN();
    const auto infinity = std::numeric_limits<TValue>::infinity();
    Require(maximum(3, -2) == 3 && minimum(3, -2) == -2, "extrema finite operands");
    Require(maximum(infinity, -infinity) == infinity && minimum(infinity, -infinity) == -infinity, "extrema infinite operands");
    for (const auto function : {maximum, minimum}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(function(nan, 3) == 3 && function(3, nan) == 3, "extrema quiet NaN and numeric operand");
        Require(std::fetestexcept(FE_ALL_EXCEPT) == 0, "extrema quiet NaN exceptions");
        Require(function(signalingNan, 3) == 3 && function(3, signalingNan) == 3, "extrema signaling NaN and numeric operand");
        Require(std::fetestexcept(FE_ALL_EXCEPT) == 0, "extrema signaling NaN exceptions");
        Require(std::isnan(function(nan, nan)), "extrema two NaNs");
        std::feraiseexcept(FE_DIVBYZERO);
        (void)function(1, 2);
        Require(std::fetestexcept(FE_DIVBYZERO) != 0, "extrema must preserve existing exceptions");
    }
    Require(!std::signbit(maximum(TValue(0), TValue(-0.L))) && !std::signbit(maximum(TValue(-0.L), TValue(0))), "maximum signed-zero ordering");
    Require(std::signbit(minimum(TValue(0), TValue(-0.L))) && std::signbit(minimum(TValue(-0.L), TValue(0))), "minimum signed-zero ordering");
    Require(std::signbit(maximum(TValue(-0.L), TValue(-0.L))) && !std::signbit(minimum(TValue(0), TValue(0))), "extrema equal signed zeros");
}

template<typename TValue>
void CheckDifference(TValue (APS5_VABI *function)(TValue, TValue)) {
    const auto infinity = std::numeric_limits<TValue>::infinity();
    const auto nan = std::numeric_limits<TValue>::quiet_NaN();
    Require(function(7, 2) == 5 && function(2, 7) == 0, "positive difference finite operands");
    Require(!std::signbit(function(-0.L, 0.L)), "positive difference positive zero");
    Require(function(infinity, 1) == infinity && function(1, infinity) == 0, "positive difference infinity");
    Require(std::isnan(function(nan, 1)) && std::isnan(function(1, nan)), "positive difference NaN");
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(std::isinf(function(std::numeric_limits<TValue>::max(), -std::numeric_limits<TValue>::max())), "positive difference overflow");
    Require(std::fetestexcept(FE_OVERFLOW) != 0, "positive difference overflow flag");
}

void CheckSignsAndNaNs() {
    const auto doubleNan = std::bit_cast<double>(UINT64_C(0x7ff8123456789abc));
    const auto floatNan = std::bit_cast<float>(UINT32_C(0x7fc12345));
    Require(std::bit_cast<std::uint64_t>(copysign_nid_postfix(doubleNan, -0.L)) == UINT64_C(0xfff8123456789abc), "copysign double NaN payload");
    Require(std::bit_cast<std::uint32_t>(copysignf_nid_postfix(floatNan, -0.L)) == UINT32_C(0xffc12345), "copysign float NaN payload");
    Require(std::signbit(copysign_nid_postfix(0, -1)) && std::signbit(copysignf_nid_postfix(0, -1)), "copysign negative zero");
    Require(copysign_nid_postfix(-3, 0) == 3 && copysignf_nid_postfix(-3, 0) == 3, "copysign positive sign");
    Require(fabsf_nid_postfix(-3) == 3 && !std::signbit(fabsf_nid_postfix(-0.L)), "fabsf sign removal");
    Require(std::bit_cast<std::uint32_t>(fabsf_nid_postfix(std::bit_cast<float>(UINT32_C(0xffc12345)))) == UINT32_C(0x7fc12345), "fabsf NaN payload");
    for (const char* tag : {"", "0", "123", "0x123", "invalid"}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        const auto doubleValue = nan_nid_postfix(tag);
        const auto floatValue = nanf_nid_postfix(tag);
        const auto extendedValue = nanl_nid_postfix(tag);
        Require(std::isnan(doubleValue) && std::isnan(floatValue) && std::isnan(extendedValue), "NaN constructors");
        Require((std::bit_cast<std::uint64_t>(doubleValue) & UINT64_C(0x0008000000000000)) != 0, "double NaN must be quiet");
        Require((std::bit_cast<std::uint32_t>(floatValue) & UINT32_C(0x00400000)) != 0, "float NaN must be quiet");
        Require(std::fetestexcept(FE_INVALID) == 0, "NaN constructors must not raise invalid");
    }
}

void CheckRoundingAndRoot() {
    Require(sqrtf_nid_postfix(4) == 2 && sqrtf_nid_postfix(0x1p-148f) == 0x1p-74f, "sqrtf normal and subnormal input");
    Require(std::signbit(sqrtf_nid_postfix(-0.L)), "sqrtf negative zero");
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(std::isnan(sqrtf_nid_postfix(-1)) && std::fetestexcept(FE_INVALID), "sqrtf invalid input");
    const double positive[] = {2, 1, 2, 1};
    const double negative[] = {-2, -2, -1, -1};
    for (std::size_t mode = 0; mode < 4; ++mode) {
        Require(std::fesetround(modes[mode]) == 0, "set rounding mode");
        Require(ceilf_nid_postfix(1.25f) == 2 && ceilf_nid_postfix(-1.25f) == -1, "ceilf fixed direction");
        Require(truncf_nid_postfix(1.75f) == 1 && truncf_nid_postfix(-1.75f) == -1, "truncf fixed direction");
        Require(std::signbit(ceilf_nid_postfix(-0.25f)) && std::signbit(truncf_nid_postfix(-0.25f)), "float rounding negative zero");
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(nearbyint_nid_postfix(1.5) == positive[mode] && nearbyint_nid_postfix(-1.5) == negative[mode], "nearbyint dynamic direction");
        Require(std::fetestexcept(FE_INEXACT) == 0, "nearbyint must not raise inexact");
        std::feraiseexcept(FE_INEXACT);
        (void)nearbyint_nid_postfix(1.5);
        Require(std::fetestexcept(FE_INEXACT) != 0, "nearbyint must preserve inexact");
        Require(std::fegetround() == modes[mode], "rounding mode preserved");
    }
    Require(std::fesetround(FE_TONEAREST) == 0, "restore nearest mode");
    Require(nearbyint_nid_postfix(2.5) == 2 && nearbyint_nid_postfix(3.5) == 4, "nearbyint ties to even");
    Require(std::signbit(nearbyint_nid_postfix(-0.L)), "nearbyint negative zero");
    Require(fmaf_nid_postfix(1.f + 0x1p-13f, 1.f - 0x1p-13f, -1.f) == -0x1p-26f, "fmaf must round once");
    Require(fmaf_nid_postfix(0x1p100f, 0x1p-100f, -1.f) == 0, "fmaf exponent range");
}

template<typename TValue>
void CheckRemainder(TValue (APS5_VABI *function)(TValue, TValue, int*)) {
    for (const int mode : modes) {
        Require(std::fesetround(mode) == 0, "set remainder rounding mode");
        int quotient = 0;
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(function(7, 2, &quotient) == -1 && quotient > 0 && (quotient & 7) == 4, "remquo positive quotient bits");
        Require(function(-7, 2, &quotient) == 1 && quotient < 0 && ((-quotient) & 7) == 4, "remquo negative quotient bits");
        Require(function(6, 4, &quotient) == -2 && (quotient & 7) == 2, "remquo tie chooses even quotient above");
        Require(function(10, 4, &quotient) == 2 && (quotient & 7) == 2, "remquo tie chooses even quotient below");
        Require(std::fetestexcept(FE_INEXACT) == 0, "remquo must not raise inexact");
        const auto zero = function(TValue(-0.L), 2, &quotient);
        Require(zero == 0 && std::signbit(zero), "remquo signed zero");
        Require(function(3, std::numeric_limits<TValue>::infinity(), &quotient) == 3, "remquo infinite divisor");
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::isnan(function(1, 0, &quotient)) && std::fetestexcept(FE_INVALID), "remquo zero divisor");
    }
    Require(std::fesetround(FE_TONEAREST) == 0, "restore remainder rounding mode");
}

template<typename TValue, typename TTarget>
void CheckNeighbors(TValue (APS5_VABI *function)(TValue, TTarget)) {
    const auto infinity = std::numeric_limits<TValue>::infinity();
    const auto epsilon = std::numeric_limits<TValue>::epsilon();
    Require(function(1, 2) == TValue(1) + epsilon, "neighbor above one");
    Require(function(1, 0) == TValue(1) - epsilon / 2, "neighbor below one");
    Require(std::signbit(function(TValue(0), TTarget(-0.L))), "equal neighbor target zero sign");
    Require(function(infinity, 0) == std::numeric_limits<TValue>::max(), "finite neighbor of infinity");
    Require(std::isnan(function(std::numeric_limits<TValue>::quiet_NaN(), 1)), "neighbor NaN");
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(function(0, 1) == std::numeric_limits<TValue>::denorm_min(), "neighbor above zero");
    Require(std::fetestexcept(FE_UNDERFLOW) != 0, "neighbor underflow flag");
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(function(std::numeric_limits<TValue>::max(), infinity) == infinity, "neighbor overflow");
    Require(std::fetestexcept(FE_OVERFLOW) != 0, "neighbor overflow flag");
}

template<typename TValue>
void CheckScaling(TValue (APS5_VABI *function)(TValue, std::int64_t)) {
    const auto infinity = std::numeric_limits<TValue>::infinity();
    const auto maximum = std::numeric_limits<TValue>::max();
    const auto minimum = std::numeric_limits<TValue>::denorm_min();
    Require(function(1.5, 10) == 1536 && function(8, -2) == 2, "scalbln finite scaling");
    Require(function(1, std::numeric_limits<TValue>::min_exponent - std::numeric_limits<TValue>::digits) == minimum, "scalbln exact subnormal");
    Require(function(infinity, INT64_MIN) == infinity, "scalbln infinite input");
    Require(std::signbit(function(TValue(-0.L), INT64_MAX)), "scalbln zero input");
    Require(std::isnan(function(std::numeric_limits<TValue>::quiet_NaN(), INT64_MAX)), "scalbln NaN");
    for (const int mode : modes) {
        Require(std::fesetround(mode) == 0, "set scaling rounding mode");
        for (const auto exponent : {INT64_C(4294967296), INT64_MAX}) {
            std::feclearexcept(FE_ALL_EXCEPT);
            Require(function(1, exponent) == (mode == FE_DOWNWARD || mode == FE_TOWARDZERO ? maximum : infinity), "scalbln positive overflow rounding and 64-bit exponent");
            Require(std::fetestexcept(FE_OVERFLOW) && std::fetestexcept(FE_INEXACT), "scalbln overflow flags");
            Require(function(-1, exponent) == (mode == FE_UPWARD || mode == FE_TOWARDZERO ? -maximum : -infinity), "scalbln negative overflow rounding");
        }
        for (const auto exponent : {INT64_C(-4294967296), INT64_MIN}) {
            std::feclearexcept(FE_ALL_EXCEPT);
            Require(function(1, exponent) == (mode == FE_UPWARD ? minimum : TValue(0)), "scalbln positive underflow rounding and 64-bit exponent");
            Require(std::fetestexcept(FE_UNDERFLOW) && std::fetestexcept(FE_INEXACT), "scalbln underflow flags");
            const auto negative = function(-1, exponent);
            Require(negative == (mode == FE_DOWNWARD ? -minimum : TValue(-0.L)) && std::signbit(negative), "scalbln negative underflow rounding");
        }
    }
    Require(std::fesetround(FE_TONEAREST) == 0, "restore scaling rounding mode");
}

}

int main() {
    std::fenv_t saved;
    Require(std::fegetenv(&saved) == 0, "save floating environment");
    Require(std::fesetround(FE_TONEAREST) == 0, "set nearest mode");
    CheckExtrema(fmax_nid_postfix, fmin_nid_postfix);
    CheckExtrema(fmaxf_nid_postfix, fminf_nid_postfix);
    CheckDifference(fdim_nid_postfix);
    CheckDifference(fdimf_nid_postfix);
    CheckSignsAndNaNs();
    CheckRoundingAndRoot();
    CheckRemainder(remquo_nid_postfix);
    CheckRemainder(remquof_nid_postfix);
    CheckNeighbors(nextafter_nid_postfix);
    CheckNeighbors(nexttoward_nid_postfix);
    CheckNeighbors(nexttowardf_nid_postfix);
    Require(nexttoward_nid_postfix(1, 1.L + 0x1p-60L) == 1. + 0x1p-52, "nexttoward retains extended target precision");
    Require(nexttowardf_nid_postfix(1, 1.L + 0x1p-60L) == 1.f + 0x1p-23f, "nexttowardf retains extended target precision");
    CheckScaling(scalbln_nid_postfix);
    CheckScaling(scalblnf_nid_postfix);
    Require(std::fesetenv(&saved) == 0, "restore floating environment");
    return 0;
}
