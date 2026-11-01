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

static void CheckLogblLroundl() {
    Require(logbl_nid_postfix(8.0L) == 3.0L && logbl_nid_postfix(0.75L) == -1.0L && logbl_nid_postfix(-0.75L) == -1.0L);
    Require(logbl_nid_postfix(0x1p-16445L) == -16445.0L);
    Require(logbl_nid_postfix(0.0L) == -INFINITY && logbl_nid_postfix(-0.0L) == -INFINITY);
    Require(logbl_nid_postfix(INFINITY) == INFINITY && logbl_nid_postfix(-INFINITY) == INFINITY);
    Require(std::isnan(logbl_nid_postfix(NAN)));
    Require(lroundl_nid_postfix(2.5L) == 3 && lroundl_nid_postfix(-2.5L) == -3);
    Require(lroundl_nid_postfix(0.5L) == 1 && lroundl_nid_postfix(-0.5L) == -1);
    Require(lroundl_nid_postfix(0.49999999999999999L) == 0);
    Require(lroundl_nid_postfix(9007199254740993.0L) == INT64_C(9007199254740993));
    Require(lroundl_nid_postfix(0x1p62L) == (INT64_C(1) << 62));
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

static void CheckFiniteClassification() {
    for (const float value : {1.0f, 0.0f, -0.0f, 0x1p-149f, 0x1.fffffep127f}) Require(finitef_nid_postfix(value) != 0);
    for (const float value : {static_cast<float>(INFINITY), -static_cast<float>(INFINITY), static_cast<float>(NAN)}) Require(finitef_nid_postfix(value) == 0);
    for (const double value : {1.0, 0.0, -0.0, 0x1p-1074, 0x1.fffffffffffffp1023}) Require(__isfinite_nid_postfix(value) != 0);
    for (const double value : {static_cast<double>(INFINITY), -static_cast<double>(INFINITY), static_cast<double>(NAN)}) Require(__isfinite_nid_postfix(value) == 0);
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

static void CheckRintTiesToEven() {
    const int saved = std::fegetround();
    Require(std::fesetround(FE_TONEAREST) == 0);
    Require(rint_nid_postfix(0.5) == 0.0 && !std::signbit(rint_nid_postfix(0.5)));
    Require(rint_nid_postfix(-0.5) == 0.0 && std::signbit(rint_nid_postfix(-0.5)));
    Require(rint_nid_postfix(2.5) == 2.0 && rint_nid_postfix(3.5) == 4.0 && rint_nid_postfix(-2.5) == -2.0);
    Require(!std::signbit(rint_nid_postfix(0.0)) && std::signbit(rint_nid_postfix(-0.0)));
    Require(rint_nid_postfix(INFINITY) == INFINITY && rint_nid_postfix(-INFINITY) == -INFINITY);
    Require(std::isnan(rint_nid_postfix(NAN)));
    Require(rint_nid_postfix(4503599627370497.0) == 4503599627370497.0);
    Require(rintf_nid_postfix(2.5f) == 2.0f && rintf_nid_postfix(0.5f) == 0.0f && !std::signbit(rintf_nid_postfix(0.5f)));
    Require(std::signbit(rintf_nid_postfix(-0.5f)) && rintf_nid_postfix(8388609.0f) == 8388609.0f);
    Require(rintl_nid_postfix(2.5L) == 2.0L && rintl_nid_postfix(3.5L) == 4.0L && std::signbit(rintl_nid_postfix(-0.5L)));
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

int main() {
    CheckRoundingAndScaling();
    CheckLogblLroundl();
    CheckSignalingNanPassThrough();
    CheckErrno();
    CheckFiniteClassification();
    CheckConsoleQuirks();
    CheckRoundingModes();
    CheckRintTiesToEven();
    CheckClassification();
    return 0;
}
