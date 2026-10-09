#include "prx/libc/include/general/VabiMacros.hpp"

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

extern "C" {
double APS5_VABI rint_nid_postfix(double);
float APS5_VABI rintf_nid_postfix(float);
long double APS5_VABI rintl_nid_postfix(long double);
std::int64_t APS5_VABI lrint_nid_postfix(double);
std::int64_t APS5_VABI lrintf_nid_postfix(float);
std::int64_t APS5_VABI lrintl_nid_postfix(long double);
std::int64_t APS5_VABI llrint_nid_postfix(double);
std::int64_t APS5_VABI llrintf_nid_postfix(float);
std::int64_t APS5_VABI llrintl_nid_postfix(long double);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Guest rounding: %s\n", message);
        std::abort();
    }
}

struct RoundingCase {
    long double input;
    std::int64_t expected[4];
};

constexpr int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
constexpr RoundingCase cases[] = {
    {1.5L, {2, 1, 2, 1}},
    {2.5L, {2, 2, 3, 2}},
    {-1.5L, {-2, -2, -1, -1}},
    {-2.5L, {-2, -3, -2, -2}},
    {0.25L, {0, 0, 1, 0}},
    {-0.25L, {0, -1, 0, 0}},
    {0.L, {0, 0, 0, 0}},
    {3.L, {3, 3, 3, 3}},
};

template<typename TValue>
void CheckFloating(TValue (APS5_VABI *function)(TValue)) {
    for (std::size_t mode = 0; mode < 4; ++mode) {
        Require(std::fesetround(modes[mode]) == 0, "cannot set rounding mode");
        for (const auto& test : cases) {
            const auto result = function(static_cast<TValue>(test.input));
            if (result != static_cast<TValue>(test.expected[mode])) {
                std::fprintf(stderr, "floating width %zu, mode %d: input %g, expected %lld, got %g\n", sizeof(TValue), modes[mode], static_cast<double>(test.input), static_cast<long long>(test.expected[mode]), static_cast<double>(result));
            }
            Require(result == static_cast<TValue>(test.expected[mode]), "floating result ignores rounding mode");
            if (result == 0) Require(std::signbit(result) == std::signbit(test.input), "rounded zero lost its sign");
        }
        const auto negativeZero = function(static_cast<TValue>(-0.L));
        Require(negativeZero == 0 && std::signbit(negativeZero), "negative zero not preserved");
        const auto infinity = std::numeric_limits<TValue>::infinity();
        Require(function(infinity) == infinity && function(-infinity) == -infinity, "infinity not preserved");
        Require(std::isnan(function(std::numeric_limits<TValue>::quiet_NaN())), "NaN not preserved");
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(function(static_cast<TValue>(1.5L)) == static_cast<TValue>(cases[0].expected[mode]), "inexact result incorrect");
        Require(std::fetestexcept(FE_INEXACT) != 0, "rint did not raise inexact");
        Require(std::fegetround() == modes[mode], "rounding mode changed");
    }
}

template<typename TValue>
void CheckInteger(std::int64_t (APS5_VABI *function)(TValue)) {
    for (std::size_t mode = 0; mode < 4; ++mode) {
        Require(std::fesetround(modes[mode]) == 0, "cannot set rounding mode");
        for (const auto& test : cases) {
            Require(function(static_cast<TValue>(test.input)) == test.expected[mode], "integer result ignores rounding mode");
        }
        Require(function(static_cast<TValue>(0x1p40L)) == INT64_C(1099511627776), "positive result narrowed to 32 bits");
        Require(function(static_cast<TValue>(-0x1p40L)) == -INT64_C(1099511627776), "negative result narrowed to 32 bits");
        Require(function(static_cast<TValue>(-0x1p63L)) == INT64_MIN, "minimum 64-bit result incorrect");
        std::feclearexcept(FE_ALL_EXCEPT);
        (void)function(static_cast<TValue>(1.5L));
        Require(std::fetestexcept(FE_INEXACT) != 0, "integer conversion did not raise inexact");
        for (const auto invalid : {std::numeric_limits<TValue>::infinity(), std::numeric_limits<TValue>::quiet_NaN(), static_cast<TValue>(0x1p63L)}) {
            std::feclearexcept(FE_ALL_EXCEPT);
            (void)function(invalid);
            Require(std::fetestexcept(FE_INVALID) != 0, "invalid conversion did not raise invalid");
        }
        Require(std::fegetround() == modes[mode], "rounding mode changed");
    }
}

void CheckExtendedPrecision() {
    Require(std::fesetround(FE_TONEAREST) == 0, "cannot set nearest rounding");
    const long double input = 0x1p62L + 1.5L;
    constexpr auto expected = INT64_C(4611686018427387906);
    Require(rintl_nid_postfix(input) == 0x1p62L + 2.L, "rintl narrowed its input to double");
    Require(lrintl_nid_postfix(input) == expected && llrintl_nid_postfix(input) == expected, "extended conversion narrowed its input to double");
    const long double maximum = 0x1p63L - 1.L;
    Require(lrintl_nid_postfix(maximum) == INT64_MAX && llrintl_nid_postfix(maximum) == INT64_MAX, "maximum 64-bit integer conversion incorrect");
}

}

int main() {
    std::fenv_t saved;
    Require(std::fegetenv(&saved) == 0, "cannot save floating environment");
    CheckFloating(rint_nid_postfix);
    CheckFloating(rintf_nid_postfix);
    CheckFloating(rintl_nid_postfix);
    CheckInteger(lrint_nid_postfix);
    CheckInteger(lrintf_nid_postfix);
    CheckInteger(lrintl_nid_postfix);
    CheckInteger(llrint_nid_postfix);
    CheckInteger(llrintf_nid_postfix);
    CheckInteger(llrintl_nid_postfix);
    CheckExtendedPrecision();
    Require(std::fesetenv(&saved) == 0, "cannot restore floating environment");
    return 0;
}
