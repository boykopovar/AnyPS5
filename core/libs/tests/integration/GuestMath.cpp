#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>

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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int erange = 34;

class GuestErrnoFixture {
public:
    GuestErrnoFixture() : saved(*__error_nid_postfix()) {}

    ~GuestErrnoFixture() {
        *__error_nid_postfix() = saved;
    }

    GuestErrnoFixture(const GuestErrnoFixture&) = delete;
    GuestErrnoFixture& operator=(const GuestErrnoFixture&) = delete;

private:
    int saved;
};

int& GuestErrno() {
    return *__error_nid_postfix();
}

struct SignedCase {
    const char* text;
    int base;
    std::int64_t value;
    std::ptrdiff_t consumed;
    int error;
};

const SignedCase signedCases[] = {
    {"-42tail", 10, -42, 3, 0},
    {"2147483648!", 10, INT64_C(2147483648), 10, 0},
    {"-2147483649!", 10, -INT64_C(2147483649), 11, 0},
    {"4294967296!", 10, INT64_C(4294967296), 10, 0},
    {"9223372036854775807!", 10, INT64_MAX, 19, 0},
    {"-9223372036854775808!", 10, INT64_MIN, 20, 0},
    {"9223372036854775808!", 10, INT64_MAX, 19, erange},
    {"-9223372036854775809!", 10, INT64_MIN, 20, erange},
    {"18446744073709551616000!", 10, INT64_MAX, 23, erange},
    {" \t+0x100000000z", 0, INT64_C(4294967296), 14, 0},
    {"-0x8000000000000000!", 0, INT64_MIN, 19, 0},
    {"0x8000000000000000!", 16, INT64_MAX, 18, erange},
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

struct UnsignedCase {
    const char* text;
    int base;
    std::uint64_t value;
    std::ptrdiff_t consumed;
    int error;
};

const UnsignedCase unsignedCases[] = {
    {"4294967296!", 10, UINT64_C(4294967296), 10, 0},
    {"9223372036854775808!", 10, UINT64_C(9223372036854775808), 19, 0},
    {"18446744073709551615!", 10, UINT64_MAX, 20, 0},
    {"18446744073709551616!", 10, UINT64_MAX, 20, erange},
    {"18446744073709551616000!", 10, UINT64_MAX, 23, erange},
    {"-1!", 10, UINT64_MAX, 2, 0},
    {"-4294967296!", 10, UINT64_MAX - UINT64_C(4294967295), 11, 0},
    {"-18446744073709551615!", 10, 1, 21, 0},
    {"-18446744073709551616!", 10, UINT64_MAX, 21, erange},
    {" \t+0xffffffffffffffffz", 0, UINT64_MAX, 21, 0},
    {"0x10000000000000000!", 16, UINT64_MAX, 19, erange},
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

std::string Label(const char* function, const char* text, int base) {
    return std::string(function) + "(\"" + text + "\", base " + std::to_string(base) + ")";
}

template<typename TParser>
void RequireSignedTable(TParser parse, const char* function) {
    const GuestErrnoFixture errnoGuard;
    for (const auto& test : signedCases) {
        const std::string label = Label(function, test.text, test.base);
        char* end = nullptr;
        GuestErrno() = 0;
        const auto value = parse(test.text, &end, test.base);
        RequireEqual(static_cast<std::int64_t>(value), test.value, label + " value");
        RequireEqual(end - test.text, test.consumed, label + " end offset");
        RequireEqual(GuestErrno(), test.error, label + " errno");
    }
}

std::uint32_t ExpectedFpclass(short code) {
    return code == 0 ? 0x10 : code == -2 ? 0x08 : code == -1 ? 0x04 : code == 1 ? 0x01 : 0x02;
}

struct ClassificationCase {
    std::uint32_t bits;
    short code;
};

const ClassificationCase classificationCases[] = {
    {0x00000000u, 0}, {0x80000000u, 0}, {0x00000001u, -2}, {0x807fffffu, -2}, {0x00800000u, -1},
    {0xbf800000u, -1}, {0x7f7fffffu, -1}, {0x7f800000u, 1}, {0xff800000u, 1}, {0x7f800001u, 2},
    {0x7fc00000u, 2}, {0xff810000u, 2}, {0x7f810000u, 2},
};

std::string Hex(std::uint32_t bits) {
    char text[16];
    std::snprintf(text, sizeof(text), "%08x", static_cast<unsigned>(bits));
    return text;
}

const Case lldivCase{"Lldiv_LargeOperandsAllSigns_MatchesHostDivision", [] {
    for (const long long numerator : {4294967301LL, -4294967301LL}) {
        for (const long long denominator : {3LL, -3LL}) {
            const std::string label = std::to_string(numerator) + " / " + std::to_string(denominator);
            const auto result = lldiv_nid_postfix(numerator, denominator);
            RequireEqual(result.quot, numerator / denominator, label + " quot");
            RequireEqual(result.rem, numerator % denominator, label + " rem");
            RequireEqual(result.quot * denominator + result.rem, numerator, label + " identity");
        }
    }
}};

const Case ldivCase{"Ldiv_LargeOperandsAllSigns_MatchesLldiv", [] {
    for (const long long numerator : {4294967301LL, -4294967301LL}) {
        for (const long long denominator : {3LL, -3LL}) {
            const std::string label = std::to_string(numerator) + " / " + std::to_string(denominator);
            const auto expected = lldiv_nid_postfix(numerator, denominator);
            const auto wide = ldiv_nid_postfix(numerator, denominator);
            RequireEqual(wide.quot, expected.quot, label + " quot");
            RequireEqual(wide.rem, expected.rem, label + " rem");
        }
    }
}};

const Case ldivMinimum{"Ldiv_Int64MinByTwo_ReturnsExactQuotient", [] {
    const auto extreme = ldiv_nid_postfix(INT64_MIN, 2);
    RequireEqual(extreme.quot, static_cast<long long>(INT64_MIN / 2), "quot");
    RequireEqual(extreme.rem, 0LL, "rem");
}};

const Case strtolTable{"Strtol_Table_ParsesValueEndAndErrno", [] {
    RequireSignedTable(strtol_nid_postfix, "strtol");
}};

const Case strtoimaxTable{"Strtoimax_Table_ParsesValueEndAndErrno", [] {
    RequireSignedTable(strtoimax_nid_postfix, "strtoimax");
}};

const Case strtoulTable{"Strtoul_Table_ParsesValueEndAndErrno", [] {
    const GuestErrnoFixture errnoGuard;
    for (const auto& test : unsignedCases) {
        const std::string label = Label("strtoul", test.text, test.base);
        char* end = nullptr;
        GuestErrno() = 0;
        const auto value = strtoul_nid_postfix(test.text, &end, test.base);
        RequireEqual(value, test.value, label + " value");
        RequireEqual(end - test.text, test.consumed, label + " end offset");
        RequireEqual(GuestErrno(), test.error, label + " errno");
    }
}};

const Case binaryPrefix{"IntegerParsers_SignedBinaryPrefix_StopAfterZero", [] {
    const char text[] = " -0B11";
    for (const int base : {0, 2}) {
        const std::string label = " in base " + std::to_string(base);
        char* end = nullptr;
        RequireEqual(strtoll_nid_postfix(text, &end, base), 0LL, "strtoll" + label);
        RequireEqual(end - text, std::ptrdiff_t{3}, "strtoll end" + label);
        RequireEqual(strtoull_nid_postfix(text, &end, base), 0ULL, "strtoull" + label);
        RequireEqual(end - text, std::ptrdiff_t{3}, "strtoull end" + label);
        RequireEqual(strtoumax_nid_postfix(text, &end, base), std::uintmax_t{0}, "strtoumax" + label);
        RequireEqual(end - text, std::ptrdiff_t{3}, "strtoumax end" + label);
        RequireEqual(_Stoull_nid_postfix(text, &end, base), 0ULL, "_Stoull" + label);
        RequireEqual(end - text, std::ptrdiff_t{3}, "_Stoull end" + label);
        RequireEqual(_Stoul_nid_postfix(text, &end, base), std::uint64_t{0}, "_Stoul" + label);
        RequireEqual(end - text, std::ptrdiff_t{3}, "_Stoul end" + label);
    }
}};

const Case keepsErrno{"IntegerParsers_InRangeValues_KeepErrno", [] {
    const GuestErrnoFixture errnoGuard;
    GuestErrno() = 13;
    RequireEqual(strtol_nid_postfix("-4294967296", nullptr, 10), -INT64_C(4294967296), "strtol value");
    RequireEqual(GuestErrno(), 13, "errno after strtol");
    RequireEqual(strtoul_nid_postfix("4294967296", nullptr, 10), UINT64_C(4294967296), "strtoul value");
    RequireEqual(GuestErrno(), 13, "errno after strtoul");
    RequireEqual(_Stoul_nid_postfix("4294967296", nullptr, 10), UINT64_C(4294967296), "_Stoul value");
    RequireEqual(GuestErrno(), 13, "errno after _Stoul");
    RequireEqual(_Stoul_nid_postfix("18446744073709551615", nullptr, 10), UINT64_MAX, "_Stoul maximum");
    RequireEqual(_Stoul_nid_postfix("-1", nullptr, 10), UINT64_MAX, "_Stoul -1");
    RequireEqual(GuestErrno(), 13, "errno after _Stoul -1");
}};

const Case floatConstants{"FloatConstants_InfAndNan_HaveSinglePrecisionBits", [] {
    RequireEqual(_FInf_nid_postfix.bits[0], 0x7f800000u, "_FInf word 0");
    RequireEqual(_FNan_nid_postfix.bits[0], 0x7fc00000u, "_FNan word 0");
    for (int word = 1; word < 4; ++word) {
        RequireEqual(_FInf_nid_postfix.bits[word], 0u, "_FInf word " + std::to_string(word));
        RequireEqual(_FNan_nid_postfix.bits[word], 0u, "_FNan word " + std::to_string(word));
    }
}};

const Case fdtestTable{"FDtest_Table_ClassifiesBitPatterns", [] {
    for (const auto& test : classificationCases) {
        const float value = std::bit_cast<float>(test.bits);
        RequireEqual(_FDtest_nid_postfix(&value), test.code, "_FDtest " + Hex(test.bits));
    }
}};

const Case fpclassifyTable{"Fpclassifyf_Table_ReturnsMatchingClass", [] {
    for (const auto& test : classificationCases) {
        const float value = std::bit_cast<float>(test.bits);
        RequireEqual(static_cast<std::uint32_t>(__fpclassifyf_nid_postfix(value)), ExpectedFpclass(test.code),
            "__fpclassifyf " + Hex(test.bits));
    }
}};

const Case fdtestConstants{"FDtest_LibraryConstants_ClassifyAsInfinityAndNan", [] {
    RequireEqual(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FInf_nid_postfix)), short{1}, "_FInf");
    RequireEqual(_FDtest_nid_postfix(reinterpret_cast<const float*>(&_FNan_nid_postfix)), short{2}, "_FNan");
}};

const Case atofCase{"Atof_LeadingSpaceAndTail_ParsesPrefix", [] {
    RequireEqual(atof_nid_postfix(" -12.5tail"), -12.5, "value");
}};

const Case strtofHex{"Strtof_HexFloat_ParsesValueAndEnd", [] {
    char* end = nullptr;
    const char input[] = "0x1.8p+2 remainder";
    RequireEqual(strtof_nid_postfix(input, &end), 6.f, "value");
    RequireEqual(end - input, std::ptrdiff_t{8}, "end offset");
}};

const Case strtofInvalid{"Strtof_NoDigits_ReturnsZeroAndStart", [] {
    char* end = nullptr;
    const char invalid[] = "invalid";
    RequireEqual(strtof_nid_postfix(invalid, &end), 0.f, "value");
    RequireEqual(end - invalid, std::ptrdiff_t{0}, "end offset");
}};

const Case strtofOverflow{"Strtof_Overflow_ReturnsInfinityAndSetsErange", [] {
    const GuestErrnoFixture errnoGuard;
    GuestErrno() = 0;
    Require(std::isinf(strtof_nid_postfix("1e1000", nullptr)), "result is infinite");
    RequireEqual(GuestErrno(), erange, "errno");
}};

const Case strtoldPrecision{"Strtold_ExtraDigits_KeepsExtendedPrecision", [] {
    char* end = nullptr;
    Require(strtold_nid_postfix("1.0000000000000000001!", &end) > 1.L, "value exceeds one");
    RequireEqual(*end, '!', "end character");
}};

const Case fmodfValues{"Fmodf_FiniteOperands_KeepsDividendSign", [] {
    RequireEqual(fmodf_nid_postfix(5.5f, 2.f), 1.5f, "5.5 mod 2");
    RequireEqual(fmodf_nid_postfix(-5.5f, 2.f), -1.5f, "-5.5 mod 2");
    Require(std::signbit(fmodf_nid_postfix(-4.f, 2.f)), "-4 mod 2 is negative zero");
}};

const Case fmodfZero{"Fmodf_ZeroDivisor_ReturnsNan", [] {
    Require(std::isnan(fmodf_nid_postfix(1.f, 0.f)), "1 mod 0 is NaN");
}};

const Case trigonometryCase{"InverseTrigonometry_HalfAndDiagonal_MatchReferenceValues", [] {
    Require(std::abs(asinf_nid_postfix(0.5f) - 0.5235988f) < 0.000001f, "asinf(0.5)");
    Require(std::abs(acosf_nid_postfix(0.5f) - 1.0471976f) < 0.000001f, "acosf(0.5)");
    Require(std::abs(atan2f_nid_postfix(1.f, -1.f) - 2.3561945f) < 0.000001f, "atan2f(1, -1)");
}};

const Case tanfZero{"Tanf_Zero_ReturnsZero", [] {
    RequireEqual(tanf_nid_postfix(0.f), 0.f, "tanf(0)");
}};

const Case hypotExact{"Hypot_ThreeFourTriangles_ReturnFive", [] {
    RequireEqual(hypot_nid_postfix(3.0, 4.0), 5.0, "hypot(3, 4)");
    RequireEqual(hypot_nid_postfix(-3.0, -4.0), 5.0, "hypot(-3, -4)");
    RequireEqual(hypotf_nid_postfix(3.f, -4.f), 5.f, "hypotf(3, -4)");
}};

const Case hypotLarge{"Hypot_HugeOperands_DoNotOverflow", [] {
    Require(std::abs(hypot_nid_postfix(1e308, 1e308) / 1.4142135623730951e308 - 1.0) < 1e-15, "hypot(1e308, 1e308)");
    Require(std::abs(hypotf_nid_postfix(2e38f, 2e38f) / 2.8284271e38f - 1.f) < 1e-6f, "hypotf(2e38, 2e38)");
}};

const Case hypotInfinityNan{"Hypot_InfinityWithQuietNan_ReturnsInfinity", [] {
    Require(std::isinf(hypot_nid_postfix(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN())),
        "hypot(inf, nan)");
    Require(std::isinf(hypotf_nid_postfix(std::numeric_limits<float>::quiet_NaN(), -std::numeric_limits<float>::infinity())),
        "hypotf(nan, -inf)");
}};

const Case hypotFiniteNan{"Hypot_FiniteWithNan_ReturnsNan", [] {
    Require(std::isnan(hypot_nid_postfix(1.0, std::numeric_limits<double>::quiet_NaN())), "hypot(1, nan)");
}};

const Case hypotfSignaling{"Hypotf_SignalingNanWithInfinity_ReturnsInfinityAndRaisesInvalid", [] {
    for (const bool signalingFirst : {true, false}) {
        const std::string label = signalingFirst ? "hypotf(snan, inf)" : "hypotf(inf, snan)";
        const float signaling = std::bit_cast<float>(std::uint32_t{0x7f800001});
        const float infinity = std::numeric_limits<float>::infinity();
        std::feclearexcept(FE_ALL_EXCEPT);
        const float result = signalingFirst ? hypotf_nid_postfix(signaling, infinity) : hypotf_nid_postfix(infinity, signaling);
        const bool invalid = std::fetestexcept(FE_INVALID) != 0;
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::isinf(result) && result > 0.f, label + " is positive infinity");
        Require(invalid, label + " raises FE_INVALID");
    }
}};

const Case log10fCase{"Log10f_Hundred_ReturnsTwo", [] {
    RequireEqual(log10f_nid_postfix(100.f), 2.f, "log10f(100)");
}};

const Case logbfFinite{"Logbf_FiniteValues_ReturnUnbiasedExponent", [] {
    RequireEqual(logbf_nid_postfix(8.f), 3.f, "logbf(8)");
    RequireEqual(logbf_nid_postfix(-0.75f), -1.f, "logbf(-0.75)");
    RequireEqual(logbf_nid_postfix(std::numeric_limits<float>::denorm_min()), -149.f, "logbf(denorm_min)");
}};

const Case logbfSpecial{"Logbf_SpecialValues_ReturnIeeeResults", [] {
    RequireEqual(logbf_nid_postfix(0.f), -std::numeric_limits<float>::infinity(), "logbf(0)");
    RequireEqual(logbf_nid_postfix(-std::numeric_limits<float>::infinity()), std::numeric_limits<float>::infinity(), "logbf(-inf)");
    Require(std::isnan(logbf_nid_postfix(std::numeric_limits<float>::quiet_NaN())), "logbf(nan)");
}};

const Case exp2Case{"Exp2_NegativeThree_ReturnsEighth", [] {
    RequireEqual(exp2_nid_postfix(-3.), 0.125, "exp2(-3)");
}};

const Case scalingCase{"ExponentScaling_PowersOfTwo_ScaleExactly", [] {
    RequireEqual(ldexp_nid_postfix(0.75, 4), 12., "ldexp(0.75, 4)");
    RequireEqual(scalbn_nid_postfix(0.75, -2), 0.1875, "scalbn(0.75, -2)");
    RequireEqual(scalbnf_nid_postfix(0.75f, 4), 12.f, "scalbnf(0.75, 4)");
}};

const Case frexpCase{"Frexp_Twelve_SplitsIntoMantissaAndExponent", [] {
    int exponent = 0;
    RequireEqual(frexp_nid_postfix(12., &exponent), 0.75, "frexp(12) mantissa");
    RequireEqual(exponent, 4, "frexp(12) exponent");
    exponent = 0;
    RequireEqual(frexpf_nid_postfix(-12.f, &exponent), -0.75f, "frexpf(-12) mantissa");
    RequireEqual(exponent, 4, "frexpf(-12) exponent");
}};

const Case roundingCase{"Lround_HalfwayAndLargeValues_RoundAwayFromZeroInSixtyFourBits", [] {
    RequireEqual(lround_nid_postfix(4294967296.5), INT64_C(4294967297), "lround(4294967296.5)");
    RequireEqual(lround_nid_postfix(-2.5), INT64_C(-3), "lround(-2.5)");
    RequireEqual(lroundf_nid_postfix(4294967296.f), INT64_C(4294967296), "lroundf(4294967296)");
    RequireEqual(lroundf_nid_postfix(2.5f), INT64_C(3), "lroundf(2.5)");
    RequireEqual(llround_nid_postfix(-4294967296.5), -INT64_C(4294967297), "llround(-4294967296.5)");
}};

const Case isinffCase{"Isinff_Values_DetectOnlyInfinities", [] {
    const auto infinity = std::numeric_limits<float>::infinity();
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    RequireEqual(__isinff_nid_postfix(infinity), 1, "inf");
    RequireEqual(__isinff_nid_postfix(-infinity), 1, "-inf");
    RequireEqual(__isinff_nid_postfix(nan), 0, "nan");
    RequireEqual(__isinff_nid_postfix(1.f), 0, "1");
}};

const Case isfinitefCase{"Isfinitef_Values_RejectInfinityAndNan", [] {
    RequireEqual(__isfinitef_nid_postfix(0.f), 1, "0");
    RequireEqual(__isfinitef_nid_postfix(std::numeric_limits<float>::infinity()), 0, "inf");
    RequireEqual(__isfinitef_nid_postfix(std::numeric_limits<float>::quiet_NaN()), 0, "nan");
}};

const Case isnormalfCase{"Isnormalf_Values_RejectZeroAndSubnormal", [] {
    RequireEqual(__isnormalf_nid_postfix(1.f), 1, "1");
    RequireEqual(__isnormalf_nid_postfix(0.f), 0, "0");
    RequireEqual(__isnormalf_nid_postfix(std::numeric_limits<float>::denorm_min()), 0, "denorm_min");
}};

const Case isnormalCase{"Isnormal_Values_RejectZeroAndSubnormal", [] {
    RequireEqual(__isnormal_nid_postfix(1.), 1, "1");
    RequireEqual(__isnormal_nid_postfix(0.), 0, "0");
    RequireEqual(__isnormal_nid_postfix(std::numeric_limits<double>::denorm_min()), 0, "denorm_min");
}};

const Case divCase{"Div_Operands_TruncateTowardZero", [] {
    struct DivCase {
        int numerator;
        int denominator;
        int quot;
        int rem;
    };
    const DivCase cases[] = {
        {7, 2, 3, 1},
        {-7, 2, -3, -1},
        {7, -2, -3, 1},
        {std::numeric_limits<int>::min(), 10, -214748364, -8},
    };
    for (const auto& test : cases) {
        const std::string label = std::to_string(test.numerator) + " / " + std::to_string(test.denominator);
        const auto result = div_nid_postfix(test.numerator, test.denominator);
        RequireEqual(result.quot, test.quot, label + " quot");
        RequireEqual(result.rem, test.rem, label + " rem");
    }
}};

} // namespace
