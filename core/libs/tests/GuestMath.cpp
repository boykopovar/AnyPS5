#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
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
int* APS5_VABI __error_nid_postfix();
struct LibcFloatConstant { std::uint32_t bits[4]; };
extern LibcFloatConstant _FInf_nid_postfix;
extern LibcFloatConstant _FNan_nid_postfix;
short APS5_VABI _FDtest_nid_postfix(const float*);
int APS5_VABI __fpclassifyf_nid_postfix(float);
float APS5_VABI fmodf_nid_postfix(float, float);
float APS5_VABI asinf_nid_postfix(float);
float APS5_VABI acosf_nid_postfix(float);
float APS5_VABI atan2f_nid_postfix(float, float);
float APS5_VABI hypotf_nid_postfix(float, float);
double APS5_VABI hypot_nid_postfix(double, double);
float APS5_VABI tanf_nid_postfix(float);
float APS5_VABI log10f_nid_postfix(float);
float APS5_VABI logbf_nid_postfix(float);
double APS5_VABI exp2_nid_postfix(double);
double APS5_VABI ldexp_nid_postfix(double, int);
double APS5_VABI scalbn_nid_postfix(double, int);
float APS5_VABI scalbnf_nid_postfix(float, int);
double APS5_VABI frexp_nid_postfix(double, int*);
float APS5_VABI frexpf_nid_postfix(float, int*);
float APS5_VABI ldexpf_nid_postfix(float, int);
std::int64_t APS5_VABI lround_nid_postfix(double);
std::div_t APS5_VABI div_nid_postfix(int, int);
std::int64_t APS5_VABI lroundf_nid_postfix(float);
std::int64_t APS5_VABI llround_nid_postfix(double);
int APS5_VABI __isfinitef_nid_postfix(float);
int APS5_VABI __isnormal_nid_postfix(double);
int APS5_VABI __isnormalf_nid_postfix(float);
int APS5_VABI __isinff_nid_postfix(float);
std::lldiv_t APS5_VABI lldiv_nid_postfix(long long, long long);
std::lldiv_t APS5_VABI ldiv_nid_postfix(std::int64_t, std::int64_t);
}
static void Require(bool value) { if (!value) std::abort(); }

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

static void ReferenceFrexp(double x, double& fraction, int& exponent) {
    std::uint64_t bits = std::bit_cast<std::uint64_t>(x);
    std::int32_t hx = static_cast<std::int32_t>(bits >> 32);
    const std::int32_t lx = static_cast<std::int32_t>(bits);
    std::int32_t ix = 0x7fffffff & hx;
    exponent = 0;
    if (ix >= 0x7ff00000 || ((ix | lx) == 0)) {
        fraction = x;
        return;
    }
    if (ix < 0x00100000) {
        x *= 18014398509481984.0;
        bits = std::bit_cast<std::uint64_t>(x);
        hx = static_cast<std::int32_t>(bits >> 32);
        ix = hx & 0x7fffffff;
        exponent = -54;
    }
    exponent += (ix >> 20) - 1022;
    hx = (hx & 0x800fffff) | 0x3fe00000;
    bits = (bits & 0xffffffffull) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(hx)) << 32);
    fraction = std::bit_cast<double>(bits);
}

static void ReferenceFrexpf(float x, float& fraction, int& exponent) {
    std::int32_t hx = static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(x));
    std::int32_t ix = 0x7fffffff & hx;
    exponent = 0;
    if (ix >= 0x7f800000 || ix == 0) {
        fraction = x;
        return;
    }
    if (ix < 0x00800000) {
        x *= 33554432.0f;
        hx = static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(x));
        ix = hx & 0x7fffffff;
        exponent = -25;
    }
    exponent += (ix >> 23) - 126;
    hx = (hx & 0x807fffff) | 0x3f000000;
    fraction = std::bit_cast<float>(static_cast<std::uint32_t>(hx));
}

static void CheckFrexpDouble(std::uint64_t bits) {
    const double x = std::bit_cast<double>(bits);
    double expected;
    int expectedExponent;
    ReferenceFrexp(x, expected, expectedExponent);
    int exponent = 0x5a5a5a5a;
    const double actual = frexp_nid_postfix(x, &exponent);
    if (std::bit_cast<std::uint64_t>(actual) != std::bit_cast<std::uint64_t>(expected) || exponent != expectedExponent) {
        std::fprintf(stderr, "Guest frexp failed for %016llx\n", static_cast<unsigned long long>(bits));
        std::abort();
    }
    if (std::isfinite(x)) Require(std::bit_cast<std::uint64_t>(ldexp_nid_postfix(actual, exponent)) == bits);
}

static void CheckFrexpFloat(std::uint32_t bits) {
    const float x = std::bit_cast<float>(bits);
    float expected;
    int expectedExponent;
    ReferenceFrexpf(x, expected, expectedExponent);
    int exponent = 0x5a5a5a5a;
    const float actual = frexpf_nid_postfix(x, &exponent);
    if (std::bit_cast<std::uint32_t>(actual) != std::bit_cast<std::uint32_t>(expected) || exponent != expectedExponent) {
        std::fprintf(stderr, "Guest frexpf failed for %08x\n", bits);
        std::abort();
    }
    if (std::isfinite(x)) Require(std::bit_cast<std::uint32_t>(ldexpf_nid_postfix(actual, exponent)) == bits);
}

static void CheckFrexpSpecialValues() {
    int exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint64_t>(frexp_nid_postfix(std::numeric_limits<double>::infinity(), &exponent)) == UINT64_C(0x7ff0000000000000) && exponent == 0);
    exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint64_t>(frexp_nid_postfix(-std::numeric_limits<double>::infinity(), &exponent)) == UINT64_C(0xfff0000000000000) && exponent == 0);
    exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint64_t>(frexp_nid_postfix(std::bit_cast<double>(UINT64_C(0x7ff0000000000001)), &exponent)) == UINT64_C(0x7ff0000000000001) && exponent == 0);
    exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint64_t>(frexp_nid_postfix(std::bit_cast<double>(UINT64_C(0xfff8000000012345)), &exponent)) == UINT64_C(0xfff8000000012345) && exponent == 0);
    exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint64_t>(frexp_nid_postfix(-0., &exponent)) == UINT64_C(0x8000000000000000) && exponent == 0);
    exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint32_t>(frexpf_nid_postfix(std::numeric_limits<float>::infinity(), &exponent)) == 0x7f800000u && exponent == 0);
    exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint32_t>(frexpf_nid_postfix(-std::numeric_limits<float>::infinity(), &exponent)) == 0xff800000u && exponent == 0);
    exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint32_t>(frexpf_nid_postfix(std::bit_cast<float>(0x7f800001u), &exponent)) == 0x7f800001u && exponent == 0);
    exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint32_t>(frexpf_nid_postfix(std::bit_cast<float>(0xffc12345u), &exponent)) == 0xffc12345u && exponent == 0);
    exponent = 0x5a5a5a5a;
    Require(std::bit_cast<std::uint32_t>(frexpf_nid_postfix(-0.f, &exponent)) == 0x80000000u && exponent == 0);
    exponent = 0;
    Require(frexp_nid_postfix(std::numeric_limits<double>::denorm_min(), &exponent) == 0.5 && exponent == -1073);
    Require(frexpf_nid_postfix(std::numeric_limits<float>::denorm_min(), &exponent) == 0.5f && exponent == -148);
    Require(frexp_nid_postfix(std::numeric_limits<double>::max(), &exponent) == std::nextafter(1., 0.) && exponent == 1024);
    Require(frexpf_nid_postfix(std::numeric_limits<float>::max(), &exponent) == std::nextafter(1.f, 0.f) && exponent == 128);
    for (int biased = 0; biased < 2048; ++biased) {
        for (const std::uint64_t fraction : {UINT64_C(0), UINT64_C(1), UINT64_C(2), UINT64_C(0x8000000000000), UINT64_C(0xffffffffffffe), UINT64_C(0xfffffffffffff)}) {
            for (const std::uint64_t sign : {UINT64_C(0), UINT64_C(1) << 63}) CheckFrexpDouble(sign | (static_cast<std::uint64_t>(biased) << 52) | fraction);
        }
    }
    for (int biased = 0; biased < 256; ++biased) {
        for (const std::uint32_t fraction : {0u, 1u, 2u, 0x400000u, 0x7ffffeu, 0x7fffffu}) {
            for (const std::uint32_t sign : {0u, 0x80000000u}) CheckFrexpFloat(sign | (static_cast<std::uint32_t>(biased) << 23) | fraction);
        }
    }
    std::uint64_t state = UINT64_C(0x9e3779b97f4a7c15);
    for (int i = 0; i < 1000000; ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        CheckFrexpDouble(state);
        CheckFrexpFloat(static_cast<std::uint32_t>(state >> 32));
    }
}
int main() {
    CheckFloatClassification();
    CheckFrexpSpecialValues();
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
