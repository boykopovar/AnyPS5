#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cfenv>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <limits>

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
    Require(SameBits(fmal_nid_postfix(Extended(0x4000, 0xe000000000000000ull), Extended(0x4001, 0xda76b022816f6a94ull), Extended(0x9b45, 0xd3b49eac84283c95ull)), Extended(0x4003, 0xbf27da1e31417d41ull)));
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
        for (const double small : {0.0, 0.1, 0.5, 0x1.8p-1030}) {
            Require(floor_nid_postfix(small) == 0.0 && !std::signbit(floor_nid_postfix(small)));
            Require(floorf_nid_postfix(static_cast<float>(small)) == 0.0f && !std::signbit(floorf_nid_postfix(static_cast<float>(small))));
        }
        for (const float small : {std::numeric_limits<float>::denorm_min(), 0x1p-130f, std::numeric_limits<float>::min()})
            Require(floorf_nid_postfix(small) == 0.0f && !std::signbit(floorf_nid_postfix(small)));
        Require(floor_nid_postfix(-0.5) == -1.0 && floorf_nid_postfix(-0.5f) == -1.0f);
        Require(std::signbit(floor_nid_postfix(-0.0)) && std::signbit(floorf_nid_postfix(-0.0f)));
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

static void CheckLogSpecialValues() {
    Require(log_nid_postfix(1.0) == 0.0 && !std::signbit(log_nid_postfix(1.0)));
    errno = 0;
    Require(log_nid_postfix(0.0) == -INFINITY && errno == ERANGE);
    errno = 0;
    Require(log_nid_postfix(-0.0) == -INFINITY && errno == ERANGE);
    errno = 0;
    Require(std::isnan(log_nid_postfix(-1.0)) && errno == EDOM);
    errno = 0;
    Require(std::isnan(log_nid_postfix(-INFINITY)) && errno == EDOM);
    Require(log_nid_postfix(INFINITY) == INFINITY);
    Require(std::isnan(log_nid_postfix(NAN)));
    Require(std::abs(log_nid_postfix(2.0) - 0.6931471805599453) <= 0.6931471805599453 * 1e-15);
    Require(std::abs(log_nid_postfix(0.5) + 0.6931471805599453) <= 0.6931471805599453 * 1e-15);
    Require(std::abs(log_nid_postfix(2.718281828459045) - 1.0) <= 1e-15);

    Require(logf_nid_postfix(1.0f) == 0.0f && !std::signbit(logf_nid_postfix(1.0f)));
    errno = 0;
    Require(logf_nid_postfix(0.0f) == -INFINITY && errno == ERANGE);
    errno = 0;
    Require(logf_nid_postfix(-0.0f) == -INFINITY && errno == ERANGE);
    errno = 0;
    Require(std::isnan(logf_nid_postfix(-1.0f)) && errno == EDOM);
    errno = 0;
    Require(std::isnan(logf_nid_postfix(-INFINITY)) && errno == EDOM);
    Require(logf_nid_postfix(INFINITY) == INFINITY);
    Require(std::isnan(logf_nid_postfix(NAN)));
    Require(std::abs(logf_nid_postfix(2.0f) - 0.6931472f) <= 0.6931472f * 1e-6f);
    Require(std::abs(logf_nid_postfix(0.5f) + 0.6931472f) <= 0.6931472f * 1e-6f);
    Require(std::abs(logf_nid_postfix(1.4012985e-45f) + 103.27893f) <= 103.27893f * 1e-6f);
}

static void CheckRoundHalfwayAwayFromZero() {
    const double doubleCases[][2] = {{0.5, 1.0}, {-0.5, -1.0}, {1.5, 2.0}, {2.5, 3.0}, {-1.5, -2.0}, {-2.5, -3.0}, {0.4, 0.0}, {-0.6, -1.0}, {4503599627370495.5, 4503599627370496.0}, {-4503599627370495.5, -4503599627370496.0}};
    for (const auto& item : doubleCases) Require(round_nid_postfix(item[0]) == item[1]);
    Require(round_nid_postfix(0.49999999999999994) == 0.0 && !std::signbit(round_nid_postfix(0.49999999999999994)));
    Require(std::signbit(round_nid_postfix(-0.4)) && round_nid_postfix(-0.4) == 0.0);
    Require(!std::signbit(round_nid_postfix(0.0)) && std::signbit(round_nid_postfix(-0.0)));
    Require(round_nid_postfix(INFINITY) == INFINITY && round_nid_postfix(-INFINITY) == -INFINITY);
    Require(std::isnan(round_nid_postfix(NAN)));
    Require(round_nid_postfix(4503599627370497.0) == 4503599627370497.0 && round_nid_postfix(1e300) == 1e300);

    const float floatCases[][2] = {{0.5f, 1.0f}, {-0.5f, -1.0f}, {1.5f, 2.0f}, {2.5f, 3.0f}, {-1.5f, -2.0f}, {-2.5f, -3.0f}, {0.4f, 0.0f}, {-0.6f, -1.0f}, {8388607.5f, 8388608.0f}, {-8388607.5f, -8388608.0f}};
    for (const auto& item : floatCases) Require(roundf_nid_postfix(item[0]) == item[1]);
    Require(roundf_nid_postfix(0.49999997f) == 0.0f && !std::signbit(roundf_nid_postfix(0.49999997f)));
    Require(std::signbit(roundf_nid_postfix(-0.4f)) && roundf_nid_postfix(-0.4f) == 0.0f);
    Require(!std::signbit(roundf_nid_postfix(0.0f)) && std::signbit(roundf_nid_postfix(-0.0f)));
    Require(roundf_nid_postfix(INFINITY) == INFINITY && roundf_nid_postfix(-INFINITY) == -INFINITY);
    Require(std::isnan(roundf_nid_postfix(NAN)));
    Require(roundf_nid_postfix(8388609.0f) == 8388609.0f && roundf_nid_postfix(1e38f) == 1e38f);
}

static void CheckFmalSpecials() {
    const long double infinity = INFINITY;
    Require(std::isnan(fmal_nid_postfix(infinity, 0.0L, 1.0L)) && std::isnan(fmal_nid_postfix(0.0L, -infinity, 1.0L)));
    Require(std::isnan(fmal_nid_postfix(infinity, 2.0L, -infinity)) && std::isnan(fmal_nid_postfix(-infinity, 2.0L, infinity)));
    Require(fmal_nid_postfix(infinity, 2.0L, 3.0L) == infinity && fmal_nid_postfix(infinity, -2.0L, 3.0L) == -infinity);
    Require(fmal_nid_postfix(2.0L, 3.0L, infinity) == infinity && fmal_nid_postfix(2.0L, 3.0L, -infinity) == -infinity);
    Require(std::isnan(fmal_nid_postfix(static_cast<long double>(NAN), 1.0L, 1.0L)) && std::isnan(fmal_nid_postfix(1.0L, static_cast<long double>(NAN), 1.0L)));
    Require(std::isnan(fmal_nid_postfix(1.0L, 1.0L, static_cast<long double>(NAN))) && std::isnan(fmal_nid_postfix(0.0L, 1.0L, static_cast<long double>(NAN))));
    Require(!std::signbit(fmal_nid_postfix(0.0L, 1.0L, -0.0L)) && std::signbit(fmal_nid_postfix(-0.0L, 1.0L, -0.0L)));
    Require(!std::signbit(fmal_nid_postfix(0.0L, -1.0L, 0.0L)) && std::signbit(fmal_nid_postfix(0.0L, -1.0L, -0.0L)));
    Require(std::signbit(fmal_nid_postfix(0x1p-9000L, -0x1p-9000L, 0.0L)) && fmal_nid_postfix(0x1p-9000L, -0x1p-9000L, 0.0L) == 0.0L);
    Require(fmal_nid_postfix(0x1p9000L, 0x1p9000L, 1.0L) == infinity && fmal_nid_postfix(0x1p9000L, -0x1p9000L, 1.0L) == -infinity);
    Require(fmal_nid_postfix(LDBL_MAX, 1.0L, LDBL_MAX) == infinity && fmal_nid_postfix(-LDBL_MAX, 1.0L, -LDBL_MAX) == -infinity);
    Require(fmal_nid_postfix(3.0L, 5.0L, -15.0L) == 0.0L && !std::signbit(fmal_nid_postfix(3.0L, 5.0L, -15.0L)));
    Require(fmal_nid_postfix(3.0L, 5.0L, 1.0L) == 16.0L && fmal_nid_postfix(-3.0L, 5.0L, 1.0L) == -14.0L);
}

static void CheckFmalExactResults() {
    struct Case {
        int mode;
        long double x, y, z, expected;
    };
    const Case cases[] = {
    {FE_TONEAREST, Extended(0xbffc, 0xd80abeb4fa460f27ull), Extended(0xbff4, 0xe21454d37039f87cull), Extended(0xbff2, 0xbecaa4c8a13997a9ull), Extended(0x3fb2, 0x93815b7c884c1ee4ull)},
    {FE_TONEAREST, Extended(0xc00c, 0xfbfa38f8e24505bfull), Extended(0xbff3, 0xbaa8b99a281385abull), Extended(0xc001, 0xb7b9e049c6df14bfull), Extended(0xbfbd, 0x86a9447babbee6b0ull)},
    {FE_TONEAREST, Extended(0x4041, 0xca977067e19ffc40ull), Extended(0x3fc9, 0x897333cfc0d89bc6ull), Extended(0xc00b, 0xd98c6c5f55bd1c52ull), Extended(0x3fcd, 0x81b5f4f09abbecc0ull)},
    {FE_TONEAREST, Extended(0x4014, 0xa013da8f7a470de0ull), Extended(0xbfe4, 0x8efa42465cc0f73full), Extended(0x3ff9, 0xb2cf0020476ebfc2ull), Extended(0xbfba, 0x961201d08ddf8a20ull)},
    {FE_TONEAREST, Extended(0x4029, 0x89ad940732133183ull), Extended(0xc056, 0x86a099af14b744a5ull), Extended(0x4080, 0x90ce753e5df3eaa8ull), Extended(0x4042, 0xc7949f75ffeaa548ull)},
    {FE_TONEAREST, Extended(0xc048, 0xef4b213fa2b63867ull), Extended(0xbfb1, 0x8e171685d5f8012aull), Extended(0xbffb, 0x84d1413ed7ddc43cull), Extended(0x3fb8, 0xd208c855f72d3f30ull)},
    {FE_TONEAREST, Extended(0x4007, 0xd03cf422e6575c25ull), Extended(0xbfdb, 0xee7295f74dc53630ull), Extended(0x3fe4, 0xc1f5e019cdda68bcull), Extended(0x3fa6, 0xcd62790e962d7ac4ull)},
    {FE_TONEAREST, Extended(0xc012, 0xf14835fab77dbc4full), Extended(0x404a, 0xdca17c4f21fc0d40ull), Extended(0x405e, 0xcff241f6f7d36ba6ull), Extended(0xc022, 0x849f16f6bcec316cull)},
    {FE_TONEAREST, Extended(0xbfc5, 0xb53104b42a9af36eull), Extended(0x3fca, 0xbedc7fb078dcbaccull), Extended(0x3f91, 0x871671fdfb076f66ull), Extended(0x3f54, 0xd01ca59296a4030bull)},
    {FE_TONEAREST, Extended(0xc09b, 0xd5b9751b695e254cull), Extended(0xbf48, 0xcd568f82235407aeull), Extended(0x403b, 0x928550f167132575ull), Extended(0x403b, 0x928550f167132575ull)},
    {FE_TONEAREST, Extended(0xbfbf, 0x83e23860f45034eaull), Extended(0x3ff9, 0xd9ccf714a34e100dull), Extended(0x3fc3, 0xdddbb142e21aed36ull), Extended(0x3fc3, 0xdda397163599a645ull)},
    {FE_TONEAREST, Extended(0xc033, 0xb900850a31d7ac94ull), Extended(0xc036, 0xd40db34669ed51edull), Extended(0x4121, 0x821bb72b386c067bull), Extended(0x4121, 0x821bb72b386c067bull)},
    {FE_TONEAREST, Extended(0xbfe8, 0xbd3f21178b084a27ull), Extended(0x3f3a, 0x9ab4d36caabd87f1ull), Extended(0x412f, 0xb3eecd7ccfe5ea51ull), Extended(0x412f, 0xb3eecd7ccfe5ea51ull)},
    {FE_TONEAREST, Extended(0xc0be, 0xc171b11da0ebaaabull), Extended(0xbf72, 0xc611832361235edcull), Extended(0x3f91, 0xb6b4c6577e51fb9full), Extended(0x4032, 0x95ab2aa197022462ull)},
    {FE_TONEAREST, Extended(0xbf5d, 0x8c5346aaa0282839ull), Extended(0x4078, 0xf31d390ccf1ac9f1ull), Extended(0xbee9, 0xe87f7050df5f0037ull), Extended(0xbfd7, 0x854310c8878395aeull)},
    {FE_TONEAREST, Extended(0x3fb6, 0xf247542bcef67a9eull), Extended(0xc011, 0xc2ea9b8a2bc30e00ull), Extended(0xc00d, 0xd1c2c4cde46ec3bcull), Extended(0xc00d, 0xd1c2c4cde46ec3bcull)},
    {FE_TONEAREST, Extended(0xc045, 0x974138b3ec515ce1ull), Extended(0xc026, 0xfa46df20fedc1225ull), Extended(0xc0ac, 0x8dbc3567446b1441ull), Extended(0xc0ac, 0x8dbc3567446b1440ull)},
    {FE_TONEAREST, Extended(0xc01b, 0xcbda8dff9323de3full), Extended(0xbfc9, 0xaa56557d45ab6fd8ull), Extended(0x4024, 0xc3aa7455bb918474ull), Extended(0x4024, 0xc3aa7455bb918476ull)},
    {FE_TONEAREST, Extended(0x3fd2, 0xf72a29d05371e85aull), Extended(0x3ff8, 0x859e13ea8ac8c416ull), Extended(0xc009, 0xe33f531cce0cf537ull), Extended(0xc009, 0xe33f531cce0cf533ull)},
    {FE_TONEAREST, Extended(0x4020, 0xfca5799a9dac8333ull), Extended(0xc04e, 0x9511e5c84878b1fdull), Extended(0xc06f, 0xeb9c34f863630e06ull), Extended(0xc071, 0x84760a02f8a7af8cull)},
    {FE_TONEAREST, Extended(0x4046, 0x9fe22e574150cea5ull), Extended(0x4033, 0xc191d982ca48d91dull), Extended(0x4079, 0x9ca7f0999bee251cull), Extended(0x407b, 0xa00e980ad71eea4aull)},
    {FE_TONEAREST, Extended(0xc050, 0xab5d2958a326c563ull), Extended(0x3fd3, 0x9afbee8297d6a9c9ull), Extended(0xbfe3, 0xbe90417e2726bc3full), Extended(0xc024, 0xcf7d5dbdc6e0aabaull)},
    {FE_TONEAREST, Extended(0xc01a, 0xcb704db85c7b9124ull), Extended(0x3fc4, 0xc1656e9ae83616feull), Extended(0xbf5f, 0x80362eb7abbf3dadull), Extended(0xbfe0, 0x99b045cc02f26036ull)},
    {FE_TONEAREST, Extended(0xbfe6, 0xd532ec3ca0f15229ull), Extended(0x4038, 0xa37997610595e01aull), Extended(0xbf9e, 0x9ffa19dc3c729dddull), Extended(0xc020, 0x8824af8e196bcf2aull)},
    {FE_TONEAREST, Extended(0x4024, 0xdd83379f4d22ef7eull), Extended(0xbfed, 0x83219a1d864de66cull), Extended(0xbf90, 0xc76afada3f709245ull), Extended(0xc012, 0xe2ee7176571614efull)},
    {FE_TONEAREST, Extended(0xbfc1, 0x8f8647a42df8115cull), Extended(0x4017, 0x976306403a6287a7ull), Extended(0xbf56, 0x9ac92aecc9caba83ull), Extended(0xbfd9, 0xa9bf7161672d09f7ull)},
    {FE_TONEAREST, Extended(0xbfb2, 0xe5ddc8ab8fd7a0b4ull), Extended(0x3fc6, 0xb1f405d1c0759131ull), Extended(0x3eb0, 0xfba6821d0c21ba7bull), Extended(0xbf7a, 0x9fc9745b8dc544bdull)},
    {FE_TONEAREST, Extended(0xa000, 0xc8f2a8b820e31c42ull), Extended(0x9ff7, 0xacab7e42703eb4aaull), Extended(0x0000, 0x010f134d1bfd9821ull), Extended(0x0000, 0x01969cf3a9fc6432ull)},
    {FE_TONEAREST, Extended(0x9ffa, 0xcfb84dc1429a1414ull), Extended(0x2000, 0xf94f5cff80cdaefeull), Extended(0x0000, 0x12f6ffd71c8300cfull), Extended(0x0000, 0x0ca4aa8f685755dfull)},
    {FE_TONEAREST, Extended(0x9ffe, 0x929d48cc73b1ed54ull), Extended(0x9ffb, 0x928d616b9a4d98a1ull), Extended(0x0000, 0x014fbaafc3ad2499ull), Extended(0x0000, 0x029f755f875a4932ull)},
    {FE_TONEAREST, Extended(0x2000, 0xfdc8d0a372e6014bull), Extended(0xa000, 0xa69bbf6b83f4e69dull), Extended(0x8001, 0xf7bfec7595521be3ull), Extended(0x8003, 0x908549ef41c53aefull)},
    {FE_TONEAREST, Extended(0x9ff7, 0xa68300ecc218f434ull), Extended(0x1ffd, 0xb720e2abd529478cull), Extended(0x8000, 0x000ee3a12f18ea13ull), Extended(0x8000, 0x001dc7425e31d426ull)},
    {FE_TONEAREST, Extended(0x9ff9, 0xe3cb8043e5b986b8ull), Extended(0x1ff7, 0x9fbb11d47d8bdba2ull), Extended(0x0000, 0x0002388768b9d422ull), Extended(0x0000, 0x00011c43b45cea11ull)},
    {FE_TONEAREST, Extended(0x2000, 0x82321d10b547177dull), Extended(0x1ff9, 0xfa26b249a297ca7eull), Extended(0x8000, 0x007f389e7eecb326ull), Extended(0x0000, 0x017da9db7cc61973ull)},
    {FE_TONEAREST, Extended(0x1ffd, 0xb31e32560626e102ull), Extended(0x9ff8, 0xe6afa658d31d4600ull), Extended(0x0000, 0x0000000000000000ull), Extended(0x8000, 0x00285a01d05b4426ull)},
    {FE_TONEAREST, Extended(0xa000, 0x8064f16f65f7e987ull), Extended(0x9ff8, 0xea259b277d36d2e6ull), Extended(0x0000, 0x01604d6346edff14ull), Extended(0x0000, 0x024b2ba5763753ccull)},
    {FE_TONEAREST, Extended(0x1ffa, 0xb48d58549b59a92cull), Extended(0x9ff7, 0xb4f13c91ceb03c11ull), Extended(0x8000, 0x0001fe76178e6da9ull), Extended(0x8000, 0x0003fcec2f1cdb52ull)},
    {FE_TONEAREST, Extended(0x9fff, 0xf24ba118bbf44f57ull), Extended(0x9ffc, 0x9113683c10ebf587ull), Extended(0x8000, 0x19bed9d8a9f71905ull), Extended(0x8000, 0x1129e690714f6603ull)},
    {FE_TONEAREST, Extended(0x1ffb, 0xc479b795fac8dd9bull), Extended(0x9ffd, 0xc607393498fb719eull), Extended(0x0000, 0x00655274d0fe0d6eull), Extended(0x8000, 0x00caa4e9a1fc1addull)},
    {FE_TONEAREST, Extended(0x9ffa, 0xd6ded64af6118caaull), Extended(0x9ffa, 0x9a2857d349e4483aull), Extended(0x0000, 0x0000000000000000ull), Extended(0x0000, 0x00102c7d2e34c808ull)},
    {FE_TONEAREST, Extended(0x1ff8, 0x8b15c03f1f67368cull), Extended(0x2000, 0xb5d7eaf0c2b5bc5cull), Extended(0x0000, 0x00943187c0a92dbfull), Extended(0x0000, 0x0159c8e76c356abeull)},
    {FE_TONEAREST, Extended(0x9ffb, 0xdb171ce9ba5e8e51ull), Extended(0x3ffe, 0x8000000000000000ull), Extended(0x0000, 0x757ba1abd04c631full), Extended(0x9ffa, 0xdb171ce9ba5e8e51ull)},
    {FE_TONEAREST, Extended(0x1ffc, 0xad790ff272534fbfull), Extended(0x3ffe, 0x8000000000000000ull), Extended(0x0000, 0x2dd34eb31c790077ull), Extended(0x1ffb, 0xad790ff272534fbfull)},
    {FE_TONEAREST, Extended(0x9ffd, 0xa472284e265f00e3ull), Extended(0x3ffe, 0x8000000000000000ull), Extended(0x0000, 0x43e044db7ff9a127ull), Extended(0x9ffc, 0xa472284e265f00e3ull)},
    {FE_TONEAREST, Extended(0x1ff7, 0xee190990e6506720ull), Extended(0x3ffe, 0x8000000000000000ull), Extended(0x0000, 0x6203d87f89e848bdull), Extended(0x1ff6, 0xee190990e6506720ull)},
    {FE_TONEAREST, Extended(0x7ffe, 0xffffffffffffffffull), Extended(0x4000, 0x8000000000000000ull), Extended(0xfffe, 0xffffffffffffffffull), Extended(0x7ffe, 0xffffffffffffffffull)},
    {FE_TONEAREST, Extended(0x7ffe, 0xc000000000000000ull), Extended(0x4000, 0x8000000000000000ull), Extended(0xfffe, 0xe000000000000000ull), Extended(0x7ffe, 0xa000000000000000ull)},
    {FE_TONEAREST, Extended(0x0001, 0x8000000000000000ull), Extended(0x3ffe, 0x8000000000000000ull), Extended(0x0000, 0x0000000000000001ull), Extended(0x0000, 0x4000000000000001ull)},
    {FE_TONEAREST, Extended(0x0001, 0x8000000000000000ull), Extended(0x3ffe, 0x8000000000000001ull), Extended(0x0000, 0x0000000000000001ull), Extended(0x0000, 0x4000000000000002ull)},
    {FE_TONEAREST, Extended(0x0001, 0x8000000000000003ull), Extended(0x3ffe, 0x8000000000000000ull), Extended(0x8000, 0x0000000000000001ull), Extended(0x0000, 0x4000000000000000ull)},
    {FE_DOWNWARD, Extended(0x3fda, 0xd9171b020910d45eull), Extended(0xc005, 0xbfa779f30b7dbb70ull), Extended(0x3f05, 0xb9c2f8b4bce1c4e1ull), Extended(0xbfe1, 0xa28642a32988af09ull)},
    {FE_DOWNWARD, Extended(0x4003, 0xa40b3864e8120d29ull), Extended(0x403a, 0xe91d326d43189835ull), Extended(0xc03f, 0x9560ebe96f7022a8ull), Extended(0x3ffe, 0xedcf5c8d74ca22faull)},
    {FE_DOWNWARD, Extended(0x4013, 0xbe9cc80c8d69cc26ull), Extended(0x3f9e, 0xeeef21bf61c81fe7ull), Extended(0xbfb3, 0xb1e7cf7b24c1ab32ull), Extended(0x3f74, 0x8be7e7e7a6efe825ull)},
    {FE_DOWNWARD, Extended(0x4062, 0xec4657b995453880ull), Extended(0xbfc5, 0x92b5655adcfbef68ull), Extended(0x4029, 0x8767894f7aa40ca1ull), Extended(0x3feb, 0xc70a3974d5a86300ull)},
    {FE_DOWNWARD, Extended(0x3fff, 0x8000000000000000ull), Extended(0x3fff, 0x8000000000000000ull), Extended(0xbfff, 0x8000000000000000ull), Extended(0x8000, 0x0000000000000000ull)},
    {FE_DOWNWARD, Extended(0x3fbb, 0xa2ddea924f80c173ull), Extended(0xbfbb, 0x829bcc6bc058857aull), Extended(0x3e4b, 0xaf233e925eea33bdull), Extended(0xbf77, 0xa62f9f116e03d1aaull)},
    {FE_DOWNWARD, Extended(0x3fc3, 0xcd3443785a1dcfafull), Extended(0xbfc4, 0xd352f7ff5675d727ull), Extended(0x0005, 0x83621216ee2d520cull), Extended(0xbf89, 0xa9649523e849c792ull)},
    {FE_DOWNWARD, Extended(0xbfe3, 0xb7619875671f976dull), Extended(0xbff7, 0xa08e99e35e392d6eull), Extended(0x3f05, 0x91b42ef51519dd60ull), Extended(0x3fdb, 0xe6064b504974304eull)},
    {FE_DOWNWARD, Extended(0x4000, 0xa1146ce2d8ccb32bull), Extended(0x3fee, 0xfcced78dd88776ccull), Extended(0xbff0, 0x9f12414053e4ea80ull), Extended(0x3faf, 0xd9ac5701c2c33088ull)},
    {FE_DOWNWARD, Extended(0xc007, 0xb9d90e0f1c3dd525ull), Extended(0x4051, 0x914b2586bc270f77ull), Extended(0x4059, 0xd2f4fd97b95330e5ull), Extended(0xc01a, 0xf7c60bfe64de3f33ull)},
    {FE_DOWNWARD, Extended(0xc010, 0xb1a048fc33863d1dull), Extended(0xc04e, 0xcc5a1bd01b8585a0ull), Extended(0xc060, 0x8dca3fceeebf88f0ull), Extended(0xc022, 0xa4eeedf6cfd22f38ull)},
    {FE_DOWNWARD, Extended(0x3fff, 0x8000000000000000ull), Extended(0xbfff, 0x8000000000000000ull), Extended(0x3fff, 0x8000000000000000ull), Extended(0x8000, 0x0000000000000000ull)},
    {FE_DOWNWARD, Extended(0xbfb4, 0xe95b49445f62ca3cull), Extended(0x3fc1, 0xda3b96429d716c1eull), Extended(0x3e4a, 0xfcd920a41fdfe1acull), Extended(0xbf77, 0xc6ee0d6651f76f49ull)},
    {FE_DOWNWARD, Extended(0x3fc9, 0xd610479a233061b0ull), Extended(0x3fcc, 0xc5f2406c1489da94ull), Extended(0x0005, 0xc4007754b5ca9f30ull), Extended(0x3f97, 0xa585185bbebac7cfull)},
    {FE_UPWARD, Extended(0x4029, 0xd55672c0b187ca85ull), Extended(0x3ff7, 0xbef5267eac55d125ull), Extended(0x3f05, 0x963e38a5d3f10bf3ull), Extended(0x4022, 0x9f2274fb3b5f5d8eull)},
    {FE_UPWARD, Extended(0xbfa4, 0xd4ac3a67e3ec814dull), Extended(0xbfce, 0xcd03ef17ccf17c7dull), Extended(0xbf74, 0xaa512f6a70fe2888ull), Extended(0x3f33, 0xfb5cc175dd32dd32ull)},
    {FE_UPWARD, Extended(0xc051, 0xb2b17872b6cab913ull), Extended(0x400d, 0xd7533416db373d6eull), Extended(0x4060, 0x964d200e65718d33ull), Extended(0x4021, 0x88ecb9d2115af96bull)},
    {FE_UPWARD, Extended(0xbfc2, 0xdfeae1ed39ff1bcdull), Extended(0xbff0, 0x802e9fe500cc52bbull), Extended(0xbfb3, 0xe03c720ccb8242fcull), Extended(0x3f76, 0xaa125b620ee73e30ull)},
    {FE_UPWARD, Extended(0x3fff, 0x8000000000000000ull), Extended(0x3fff, 0x8000000000000000ull), Extended(0xbfff, 0x8000000000000000ull), Extended(0x0000, 0x0000000000000000ull)},
    {FE_UPWARD, Extended(0xbfb5, 0xeee495a2df00a020ull), Extended(0x3fc0, 0xe4244daded8b9e24ull), Extended(0x3e4a, 0xfb07323507fbff5dull), Extended(0xbf77, 0xd4e575e728aaef0eull)},
    {FE_UPWARD, Extended(0x3fc7, 0xf3f45c56e1705264ull), Extended(0xbfc9, 0x9dd6861be9884538ull), Extended(0x0005, 0xf79e010f1022df59ull), Extended(0xbf92, 0x96694ab2fa88d169ull)},
    {FE_UPWARD, Extended(0x4028, 0x80b74ed7333cb7cdull), Extended(0xc017, 0xf70b73724bc2f829ull), Extended(0xbf05, 0xeb24d9af5f893033ull), Extended(0xc040, 0xf86d3dfbaba49929ull)},
    {FE_UPWARD, Extended(0x4012, 0x91ce666500a37241ull), Extended(0xbfa1, 0x9a614f1679605445ull), Extended(0x3fb4, 0xafdb2bb21a3de8aaull), Extended(0xbf71, 0xc571c505eeb1f850ull)},
    {FE_UPWARD, Extended(0x3fac, 0xc78fa0c2e46ed14dull), Extended(0x402a, 0xf69db87d2483aa8cull), Extended(0xbfd8, 0xc03ef761a82cfe7full), Extended(0xbf9a, 0x9184e93f238059f9ull)},
    {FE_UPWARD, Extended(0x3fac, 0xf0422d95600216ecull), Extended(0xbfcd, 0x8e68b907038fe61dull), Extended(0x3f7b, 0x85a6fdd1c8082e7bull), Extended(0xbf3d, 0xcf60cc8454aa282full)},
    {FE_UPWARD, Extended(0x3fff, 0x8000000000000000ull), Extended(0xbfff, 0x8000000000000000ull), Extended(0x3fff, 0x8000000000000000ull), Extended(0x0000, 0x0000000000000000ull)},
    {FE_UPWARD, Extended(0xbfba, 0xaa5cc7d9e99ce8efull), Extended(0xbfb8, 0xdde989df9497db48ull), Extended(0x3e47, 0x921ccc1b42addec5ull), Extended(0x3f74, 0x93ad82b974aa7befull)},
    {FE_UPWARD, Extended(0x3fcc, 0xef961ab339e79452ull), Extended(0x3fc4, 0xf9d5ab3259c0ccbdull), Extended(0x8005, 0xa19dfed9769b2c08ull), Extended(0x3f92, 0xe9d0f814ed97fb1aull)},
    {FE_TOWARDZERO, Extended(0x3fda, 0x967a4b4b23b59e66ull), Extended(0xc013, 0xd0fd6fdd7cb28222ull), Extended(0xbf05, 0xf9efdc539dac1068ull), Extended(0xbfee, 0xf5b0ab955b953311ull)},
    {FE_TOWARDZERO, Extended(0xbfd7, 0xf0f4fa025ee4eeb7ull), Extended(0xc00b, 0xe287f2f25d247570ull), Extended(0xbfe4, 0xd5383a91a29c0939ull), Extended(0xbfa0, 0xb2d5fafce01ecf00ull)},
    {FE_TOWARDZERO, Extended(0xc01a, 0x8fe31c6623a93fcaull), Extended(0x3fed, 0xc1b8eae02de00c4bull), Extended(0x4008, 0xd9c44f52058694abull), Extended(0x3fc9, 0xd9e77b594bacd7d2ull)},
    {FE_TOWARDZERO, Extended(0x3fcf, 0xc02e6ed30d213eb8ull), Extended(0x404f, 0xa69d768a8d9e6010ull), Extended(0xc01f, 0xfa28a2a878c0335cull), Extended(0x3fe2, 0xcfa806d755ca3ae0ull)},
    {FE_TOWARDZERO, Extended(0x3fff, 0x8000000000000000ull), Extended(0x3fff, 0x8000000000000000ull), Extended(0xbfff, 0x8000000000000000ull), Extended(0x0000, 0x0000000000000000ull)},
    {FE_TOWARDZERO, Extended(0x3fb4, 0x86479c1667360c7dull), Extended(0xbfb3, 0x9b3cd70c4aa01ebaull), Extended(0x3e3c, 0x808a76d608aac473ull), Extended(0xbf68, 0xa2da8a2d78a977e7ull)},
    {FE_TOWARDZERO, Extended(0xbfc9, 0xc361140585f6b6f8ull), Extended(0xbfc7, 0x9915907602766c03ull), Extended(0x0005, 0xce88fac844e786c0ull), Extended(0x3f91, 0xe9aaf45d3acfd3a8ull)},
    {FE_TOWARDZERO, Extended(0x3ff3, 0xb42a54873155966cull), Extended(0x4007, 0xec4789a8c5cd9fc1ull), Extended(0xbf05, 0xc5bc80bf8a45f13aull), Extended(0x3ffc, 0xa6495e8b8132b0e7ull)},
    {FE_TOWARDZERO, Extended(0xc022, 0xe350e55a911161e4ull), Extended(0x4037, 0xd20d073a8fa7dbcaull), Extended(0x405b, 0xba83edab26c24db8ull), Extended(0xc018, 0xfac1a9ec718a4f40ull)},
    {FE_TOWARDZERO, Extended(0xbfe7, 0xc5a784f2dc4b3a5aull), Extended(0x3ffe, 0xc73c772afb8f0d6eull), Extended(0x3fe7, 0x99d3e79e00ac8288ull), Extended(0xbfa7, 0x90f1ae8a508fa4acull)},
    {FE_TOWARDZERO, Extended(0xc03d, 0x8e8340934f486be2ull), Extended(0x3ff5, 0xea0eea1c9fadcb70ull), Extended(0x4034, 0x824c468816cc8433ull), Extended(0xbff6, 0xba13fffed8bd5a38ull)},
    {FE_TOWARDZERO, Extended(0x3fff, 0x8000000000000000ull), Extended(0xbfff, 0x8000000000000000ull), Extended(0x3fff, 0x8000000000000000ull), Extended(0x0000, 0x0000000000000000ull)},
    {FE_TOWARDZERO, Extended(0x3fb9, 0x80c3a8586630712full), Extended(0x3fbf, 0xbdeeec34001e87a0ull), Extended(0xbe4d, 0xdcc3f576895a1ab2ull), Extended(0x3f79, 0xbf113ffc993f9fcfull)},
    {FE_TOWARDZERO, Extended(0x3fcd, 0xaad4b2287fd9d00eull), Extended(0xbfcc, 0xcebd399b32cec4b4ull), Extended(0x8005, 0xa06403ef3e80afdeull), Extended(0xbf9b, 0x89f56cd50b22116dull)},
    {FE_TONEAREST, Extended(0xc005, 0xb4032800a9921defull), Extended(0xbfeb, 0xf0ecdfe8a4f87beeull), Extended(0xbf05, 0xd49f319c60223531ull), Extended(0x3ff2, 0xa96985dbd64ae2e8ull)},
    {FE_TONEAREST, Extended(0x403f, 0xe1d1a22901d01a89ull), Extended(0x4011, 0x83c7e96083191083ull), Extended(0xc051, 0xe87d3b99f14cd4c9ull), Extended(0xc010, 0xcfe4968179e36f94ull)},
    {FE_TONEAREST, Extended(0x3fa1, 0xf637b860a7e455edull), Extended(0x3ffb, 0xafe0527e9ac26468ull), Extended(0xbf9e, 0xa927d72304ac2f97ull), Extended(0x3f5e, 0xa12f743915ed7c48ull)},
    {FE_TONEAREST, Extended(0xc01f, 0x8b778b511da22a3dull), Extended(0xc006, 0xb28762937ac575bcull), Extended(0xc026, 0xc285c139f47bf4a6ull), Extended(0x3fe9, 0xad1d84c3dcd57973ull)},
    {FE_TONEAREST, Extended(0x3fff, 0x8000000000000000ull), Extended(0x3fff, 0x8000000000000000ull), Extended(0xbfff, 0x8000000000000000ull), Extended(0x0000, 0x0000000000000000ull)},
    {FE_TONEAREST, Extended(0x3fc1, 0xa5251da30e2cd711ull), Extended(0x3fb2, 0xb403ca18280dffabull), Extended(0x3e48, 0xdf9016b44400bed8ull), Extended(0x3f74, 0xe8411549bb29ed53ull)},
    {FE_TONEAREST, Extended(0xbfc9, 0xe8e6928c2aa9ce78ull), Extended(0x3fc5, 0xc5712971aec9d160ull), Extended(0x8005, 0xde7c82079a859945ull), Extended(0xbf90, 0xb3a06240e6f6f6aaull)},
    {FE_TONEAREST, Extended(0x3fe7, 0xd35934d983109a21ull), Extended(0xbfd2, 0x81d9b89529b2744eull), Extended(0xbf05, 0x8869208c7a18febbull), Extended(0xbfba, 0xd667654555805a1aull)},
    {FE_TONEAREST, Extended(0x3fd1, 0xee0a46bf57c42d7bull), Extended(0xbfa5, 0xc4b62ea1ffc525f9ull), Extended(0x3f78, 0xb6e944d5463b0616ull), Extended(0xbf31, 0xf10fb59e8501d180ull)},
    {FE_TONEAREST, Extended(0xbfb6, 0xc1458ad48daa1600ull), Extended(0xc02d, 0xae95ae864781d1d5ull), Extended(0xbfe5, 0x83ce4598eabf7e76ull), Extended(0xbfa5, 0xf26ae2af2f85b200ull)},
    {FE_TONEAREST, Extended(0xc018, 0xd4ed1650c3be919aull), Extended(0xc034, 0xb9f26361aef8bb4aull), Extended(0xc04e, 0x9aa8efe864cc784eull), Extended(0xc010, 0xfd726b2f5f579adfull)},
    {FE_TONEAREST, Extended(0x3fff, 0x8000000000000000ull), Extended(0xbfff, 0x8000000000000000ull), Extended(0x3fff, 0x8000000000000000ull), Extended(0x0000, 0x0000000000000000ull)},
    {FE_TONEAREST, Extended(0xbfb1, 0xdf4eea570fbb004full), Extended(0x3fb5, 0x954162b3576a5f0cull), Extended(0x3e3b, 0x803f345c6deb3fd7ull), Extended(0xbf68, 0x8231f786d2669fc8ull)},
    {FE_TONEAREST, Extended(0x3fc4, 0xedc5e19f483b20e9ull), Extended(0x3fc6, 0xe69ba0de43a76befull), Extended(0x8005, 0x9ef493447e8b2cc5ull), Extended(0x3f8c, 0xd63054eed3c95b06ull)},
    };
    const int saved = std::fegetround();
    for (const auto& item : cases) {
        Require(std::fesetround(item.mode) == 0);
        const long double result = fmal_nid_postfix(item.x, item.y, item.z);
        std::fesetround(saved);
        Require(SameBits(result, item.expected));
    }
}

static void CheckFmalDirectedZero() {
    const int saved = std::fegetround();
    for (const int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        Require(std::fesetround(mode) == 0);
        const long double result = fmal_nid_postfix(3.0L, 5.0L, -15.0L);
        std::fesetround(saved);
        Require(result == 0.0L && std::signbit(result) == (mode == FE_DOWNWARD));
    }
}

static void CheckFmalFarApartDirected() {
    struct Case {
        int mode;
        long double y, z, expected;
    };
    const long double one = Extended(0x3fff, 0x8000000000000000ull), above = Extended(0x3fff, 0x8000000000000001ull), below = Extended(0x3ffe, 0xffffffffffffffffull);
    const long double aboveNegative = Extended(0xbfff, 0x8000000000000001ull), belowNegative = Extended(0xbffe, 0xffffffffffffffffull);
    const long double tiny = 0x1p-200L;
    const Case cases[] = {
    {FE_TONEAREST, tiny, one, one}, {FE_TONEAREST, -tiny, -one, -one},
    {FE_TOWARDZERO, tiny, one, one}, {FE_TOWARDZERO, tiny, -one, belowNegative}, {FE_TOWARDZERO, -tiny, one, below}, {FE_TOWARDZERO, -tiny, -one, -one},
    {FE_DOWNWARD, tiny, one, one}, {FE_DOWNWARD, tiny, -one, -one}, {FE_DOWNWARD, -tiny, one, below}, {FE_DOWNWARD, -tiny, -one, aboveNegative},
    {FE_UPWARD, tiny, one, above}, {FE_UPWARD, tiny, -one, belowNegative}, {FE_UPWARD, -tiny, one, one}, {FE_UPWARD, -tiny, -one, -one},
    };
    const int saved = std::fegetround();
    for (const auto& item : cases) {
        Require(std::fesetround(item.mode) == 0);
        const long double result = fmal_nid_postfix(1.0L, item.y, item.z);
        std::fesetround(saved);
        Require(SameBits(result, item.expected));
    }
}

static void CheckExpSpecialValues() {
    Require(exp_nid_postfix(0.0) == 1.0 && exp_nid_postfix(-0.0) == 1.0);
    Require(exp_nid_postfix(INFINITY) == INFINITY);
    Require(exp_nid_postfix(-INFINITY) == 0.0 && !std::signbit(exp_nid_postfix(-INFINITY)));
    Require(std::isnan(exp_nid_postfix(NAN)));
    Require(std::abs(exp_nid_postfix(1.0) - 2.718281828459045) <= 2.718281828459045 * 1e-15);
    Require(std::abs(exp_nid_postfix(-1.0) - 0.36787944117144233) <= 0.36787944117144233 * 1e-15);
    errno = 0;
    Require(exp_nid_postfix(710.0) == INFINITY && errno == ERANGE);
    Require(exp_nid_postfix(-746.0) == 0.0);

    Require(expf_nid_postfix(0.0f) == 1.0f && expf_nid_postfix(-0.0f) == 1.0f);
    Require(expf_nid_postfix(INFINITY) == INFINITY);
    Require(expf_nid_postfix(-INFINITY) == 0.0f && !std::signbit(expf_nid_postfix(-INFINITY)));
    Require(std::isnan(expf_nid_postfix(NAN)));
    Require(std::abs(expf_nid_postfix(1.0f) - 2.7182817f) <= 2.7182817f * 1e-6f);
    errno = 0;
    Require(expf_nid_postfix(89.0f) == INFINITY && errno == ERANGE);
    Require(expf_nid_postfix(-110.0f) == 0.0f);
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
    CheckRoundHalfwayAwayFromZero();
    CheckFmalSpecials();
    CheckFmalExactResults();
    CheckFmalDirectedZero();
    CheckFmalFarApartDirected();
    CheckExpSpecialValues();
    CheckLogSpecialValues();
    return 0;
}
