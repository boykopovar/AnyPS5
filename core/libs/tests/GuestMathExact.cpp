#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cfenv>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
long double APS5_VABI fdiml_nid_postfix(long double, long double);
long double APS5_VABI fmodl_nid_postfix(long double, long double);
long double APS5_VABI remquol_nid_postfix(long double, long double, int*);
int APS5_VABI ilogbl_nid_postfix(long double);
long double APS5_VABI roundl_nid_postfix(long double);
long double APS5_VABI modfl_nid_postfix(long double, long double*);
long double APS5_VABI nanl_nid_postfix(const char*);
long double APS5_VABI scalblnl_nid_postfix(long double, std::int64_t);
long double APS5_VABI fmal_nid_postfix(long double, long double, long double);
double APS5_VABI scalb_nid_postfix(double, double);
double APS5_VABI scalbln_nid_postfix(double, std::int64_t);
long double APS5_VABI remainderl_nid_postfix(long double, long double);
double APS5_VABI drem_nid_postfix(double, double);
float APS5_VABI dremf_nid_postfix(float, float);
std::int64_t APS5_VABI lrint_nid_postfix(double);
double APS5_VABI rint_nid_postfix(double);
float APS5_VABI rintf_nid_postfix(float);
long double APS5_VABI rintl_nid_postfix(long double);
long long APS5_VABI llrint_nid_postfix(double);
int APS5_VABI finite_nid_postfix(double);
int APS5_VABI __fpclassifyl_nid_postfix(long double);
int APS5_VABI __isnanl_nid_postfix(long double);
int APS5_VABI __isnormall_nid_postfix(long double);
short APS5_VABI _LDtest_nid_postfix(const long double*);
int APS5_VABI _Dclass_nid_postfix(double);
short APS5_VABI _LDsign_nid_postfix(long double);
}

static void Require(bool value) { if (!value) std::abort(); }

static long double Extended(std::uint16_t signExponent, std::uint64_t mantissa) {
    long double value = 0;
    unsigned char bytes[sizeof(long double)] = {};
    std::memcpy(bytes, &mantissa, sizeof(mantissa));
    std::memcpy(bytes + 8, &signExponent, sizeof(signExponent));
    std::memcpy(&value, bytes, sizeof(bytes));
    return value;
}

static bool SameBits(long double left, long double right) { return std::memcmp(&left, &right, 10) == 0; }

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

static void CheckClassification() {
    const long double pseudoDenormal = Extended(0x0000, 0x8000000000000000ull);
    Require(_LDtest_nid_postfix(&pseudoDenormal) == -1 && __fpclassifyl_nid_postfix(pseudoDenormal) == -2);
    const long double zero = 0.0L, infinity = INFINITY, nan = NAN, denormal = Extended(0, 1);
    Require(_LDtest_nid_postfix(&zero) == 0 && _LDtest_nid_postfix(&infinity) == 1 && _LDtest_nid_postfix(&nan) == 2 && _LDtest_nid_postfix(&denormal) == -2);
    Require(__fpclassifyl_nid_postfix(1.0L) == -1 && __fpclassifyl_nid_postfix(0.0L) == 0 && __fpclassifyl_nid_postfix(INFINITY) == 1);
    Require(__isnanl_nid_postfix(NAN) == 1 && __isnanl_nid_postfix(INFINITY) == 0);
    Require(__isnormall_nid_postfix(1.0L) == 1 && __isnormall_nid_postfix(denormal) == 0);
    Require(_Dclass_nid_postfix(5e-324) == -2 && _Dclass_nid_postfix(-0.0) == 0);
    Require(_LDsign_nid_postfix(-2.0L) == -32768 && _LDsign_nid_postfix(2.0L) == 0);
}

int main() {
    CheckSignalingNanPassThrough();
    CheckErrno();
    CheckConsoleQuirks();
    CheckRoundingModes();
    CheckClassification();
    return 0;
}
