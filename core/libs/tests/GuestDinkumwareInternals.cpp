#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int RangeError = 34;
constexpr int InvalidArgument = 22;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Dinkumware internals: %s\n", message);
        std::exit(1);
    }
}

void CheckStod(const char* text, std::int64_t pten, double expected, long consumed, int error, const char* message) {
    *__error_nid_postfix() = 0;
    char* end = nullptr;
    const double value = _Stod_nid_postfix(text, &end, pten);
    const bool same = std::isnan(expected) ? std::isnan(value) : value == expected && std::signbit(value) == std::signbit(expected);
    Require(same && end - text == consumed && *__error_nid_postfix() == error, message);
}

void TestStod() {
    CheckStod("1.5", 0, 1.5, 3, 0, "unscaled decimal");
    CheckStod("1.5", 1, 15.0, 3, 0, "decimal scaled up");
    CheckStod("12", -2, 0.12, 2, 0, "decimal scaled down");
    CheckStod("  -2.25e3xyz", 300, -0x1.a3f7f806a8135p+1007, 9, 0, "the scale is applied before rounding");
    CheckStod("1e-400", 300, 1e-100, 6, 0, "an underflowing text scaled back into range");
    CheckStod("1e-400", 0, 0.0, 6, RangeError, "an unscaled underflow");
    CheckStod("1e300", 100, HUGE_VAL, 5, RangeError, "a scale that overflows");
    CheckStod("1e300", 8, 1e308, 5, 0, "a scale up to the largest decades");
    CheckStod("5e-324", -1, 0.0, 6, RangeError, "a scale that underflows to zero");
    CheckStod("1", 2147483647, HUGE_VAL, 1, RangeError, "the largest scale");
    CheckStod("5", std::int64_t{1} << 40, HUGE_VAL, 1, RangeError, "a scale above 32 bits");
    CheckStod("1e400", -100, 1e300, 5, 0, "an overflowing text scaled back into range");
    CheckStod("0.01e41", -2, 1e37, 7, 0, "a negative scale from leading fraction zeros");
    CheckStod("1e-2147483648", -1, 0.0, 13, RangeError, "the smallest exponent and scale");
    CheckStod("-0", 5, -0.0, 2, 0, "negative zero keeps its sign");
    CheckStod("infinity", 3, HUGE_VAL, 8, 0, "infinity is not scaled");
    CheckStod("nan", 3, NAN, 3, 0, "NaN is not scaled");
    CheckStod("1e", 2, 100.0, 1, 0, "an exponent marker without digits");
    CheckStod(".5", 2, 50.0, 2, 0, "a fraction without integer digits");
    CheckStod("0.3", -1, 0.03, 3, 0, "0.3 scaled down rounds as 0.03");
    CheckStod("123456789012345678901234567890", -20, 1234567890.1234567, 30, 0, "a long digit string");
    CheckStod("abc", 4, 0.0, 0, 0, "no number");
    CheckStod("0x1.8p0", 0, 1.5, 7, 0, "unscaled hexadecimal");
}

void TestStoll() {
    char* end = nullptr;
    *__error_nid_postfix() = 0;
    Require(_Stoll_nid_postfix("-0x1F", &end, 0) == -31 && std::strcmp(end, "") == 0, "_Stoll base detection");
    Require(_Stoll_nid_postfix("+42abc", &end, 10) == 42 && std::strcmp(end, "abc") == 0, "_Stoll stops at the first non-digit");
    Require(_Stoll_nid_postfix("  z", &end, 36) == 35, "_Stoll base 36");
    Require(*__error_nid_postfix() == 0, "_Stoll set errno without an overflow");
    Require(_Stoll_nid_postfix("9223372036854775808", &end, 10) == 9223372036854775807ll && *__error_nid_postfix() == RangeError, "_Stoll saturates");
}

void TestArithmetic() {
    Require(_Divide_nid_postfix(3.0, 2.0) == 1.5, "_Divide quotient");
    Require(std::isinf(_Divide_nid_postfix(1.0, 0.0)) && !std::signbit(_Divide_nid_postfix(1.0, 0.0)), "_Divide positive infinity");
    Require(std::isinf(_Divide_nid_postfix(-1.0, 0.0)) && std::signbit(_Divide_nid_postfix(-1.0, 0.0)), "_Divide negative infinity");
    Require(std::isnan(_Divide_nid_postfix(0.0, 0.0)), "_Divide invalid");
    Require(_Divide_nid_postfix(-0.0, 2.0) == 0.0 && std::signbit(_Divide_nid_postfix(-0.0, 2.0)), "_Divide signed zero");
    Require(_Divide_nid_postfix(1.0, INFINITY) == 0.0 && !std::signbit(_Divide_nid_postfix(1.0, INFINITY)), "_Divide finite by infinity");
    Require(std::isnan(_Divide_nid_postfix(INFINITY, INFINITY)), "_Divide infinity by infinity");
    Require(_Recip_nid_postfix(4.0) == 0.25, "_Recip quotient");
    Require(_Recip_nid_postfix(-4.0) == -0.25, "_Recip negative quotient");
    Require(std::isinf(_Recip_nid_postfix(-0.0)) && std::signbit(_Recip_nid_postfix(-0.0)), "_Recip signed infinity");
    Require(_Recip_nid_postfix(INFINITY) == 0.0 && !std::signbit(_Recip_nid_postfix(INFINITY)), "_Recip infinity");
    Require(std::isnan(_Recip_nid_postfix(NAN)), "_Recip NaN");
    Require(_FDivide_nid_postfix(3.0f, 2.0f) == 1.5f, "_FDivide quotient");
    Require(std::isinf(_FDivide_nid_postfix(-1.0f, 0.0f)) && std::signbit(_FDivide_nid_postfix(-1.0f, 0.0f)), "_FDivide negative infinity");
    Require(std::isnan(_FDivide_nid_postfix(0.0f, 0.0f)), "_FDivide invalid");
    Require(_FDivide_nid_postfix(-0.0f, 2.0f) == 0.0f && std::signbit(_FDivide_nid_postfix(-0.0f, 2.0f)), "_FDivide signed zero");
    Require(_FRecip_nid_postfix(4.0f) == 0.25f, "_FRecip quotient");
    Require(_FRecip_nid_postfix(-4.0f) == -0.25f, "_FRecip negative quotient");
    Require(std::isinf(_FRecip_nid_postfix(-0.0f)) && std::signbit(_FRecip_nid_postfix(-0.0f)), "_FRecip signed infinity");
    Require(_FRecip_nid_postfix(INFINITY) == 0.0f && !std::signbit(_FRecip_nid_postfix(INFINITY)), "_FRecip infinity");
    Require(std::isnan(_FRecip_nid_postfix(NAN)), "_FRecip NaN");
}

void TestFloatErrors() {
    char* end = nullptr;
    int error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stodx_nid_postfix("1.5", &end, 2, &error) == 150.0 && error == 0 && *__error_nid_postfix() == 0, "_Stodx in range");
    error = 99;
    Require(std::isinf(_Stodx_nid_postfix("1e400", &end, 0, &error)) && error == 1 && *__error_nid_postfix() == RangeError, "_Stodx overflow");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stodx_nid_postfix("1e-400", &end, 2, &error) == 0.0 && error == 1 && *__error_nid_postfix() == RangeError, "_Stodx underflow");
    error = 99;
    *__error_nid_postfix() = 7;
    Require(_Stodx_nid_postfix("1e-400", &end, 300, &error) == 1e-100 && error == 0 && *__error_nid_postfix() == 7, "_Stodx rescued by its scale");
    error = 99;
    *__error_nid_postfix() = RangeError;
    Require(_Stodx_nid_postfix("2.5", &end, 1, &error) == 25.0 && error == 0 && *__error_nid_postfix() == RangeError, "_Stodx kept a preset errno");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(std::isinf(_Stofx_nid_postfix("1e39", &end, 0, &error)) && error == 1 && *__error_nid_postfix() == RangeError, "_Stofx overflow");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stofx_nid_postfix("2.5e-1", &end, 3, &error) == 250.0f && error == 0 && *__error_nid_postfix() == 0, "_Stofx scaled");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stofx_nid_postfix("1e39", &end, -1, &error) == 1e38f && error == 0 && *__error_nid_postfix() == 0, "_Stofx overflow scaled back into range");
    error = 99;
    Require(_Stodx_nid_postfix("1e400", &end, -100, &error) == 1e300 && error == 0 && *__error_nid_postfix() == 0, "_Stodx overflow scaled back into range");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stoldx_nid_postfix("1e400", &end, 0, &error) == 1e400L && error == 0 && *__error_nid_postfix() == 0, "_Stoldx uses the extended range");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stodx_nid_postfix("1e-310", &end, 0, &error) == 1e-310 && error == 0 && *__error_nid_postfix() == 0 && end[0] == '\0', "_Stodx subnormal");
    error = 99;
    Require(_Stodx_nid_postfix("1e-300", &end, -10, &error) == 1e-310 && error == 0 && *__error_nid_postfix() == 0, "_Stodx scaled into the subnormal range");
    error = 99;
    *__error_nid_postfix() = 7;
    Require(_Stodx_nid_postfix("1e-310", &end, 0, &error) == 1e-310 && error == 0 && *__error_nid_postfix() == 7, "_Stodx subnormal kept a preset errno");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stodx_nid_postfix("2e-324", &end, 0, &error) == 0.0 && error == 1 && *__error_nid_postfix() == RangeError, "_Stodx below the smallest subnormal");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stofx_nid_postfix("1e-45", &end, 0, &error) == 0x1p-149f && error == 0 && *__error_nid_postfix() == 0 && end[0] == '\0', "_Stofx inexact subnormal");
    error = 99;
    Require(_Stofx_nid_postfix("1e-35", &end, -10, &error) == 0x1p-149f && error == 0 && *__error_nid_postfix() == 0, "_Stofx scaled into the subnormal range");
    error = 99;
    *__error_nid_postfix() = 7;
    const float subnormal = _Stofx_nid_postfix("1e-40", &end, 0, &error);
    Require(subnormal > 0x1p-134f && subnormal < 0x1p-132f && error == 0 && *__error_nid_postfix() == 7, "_Stofx subnormal kept a preset errno");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stofx_nid_postfix("1e-46", &end, 0, &error) == 0.0f && error == 1 && *__error_nid_postfix() == RangeError, "_Stofx underflow");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stofx_nid_postfix("1.17549435e-38", &end, 0, &error) == 0x1p-126f && error == 0 && *__error_nid_postfix() == 0, "_Stofx smallest normal");
    error = 99;
    Require(_Stoldx_nid_postfix("1e-4940", &end, 0, &error) == 1e-4940L && error == 0 && *__error_nid_postfix() == 0, "_Stoldx subnormal");
    error = 99;
    Require(_Stoldx_nid_postfix("1e-4930", &end, -10, &error) == 1e-4940L && error == 0 && *__error_nid_postfix() == 0, "_Stoldx scaled into the subnormal range");
    error = 99;
    Require(_Stoldx_nid_postfix("1e-5000", &end, 0, &error) == 0.0L && error == 1 && *__error_nid_postfix() == RangeError, "_Stoldx underflow");
    Require(_Stof_nid_postfix("1.5", &end, 1) == 15.0f, "_Stof");
    Require(_Stold_nid_postfix("1.5", &end, -1) == 0.15L, "_Stold");
    error = 99;
    Require(std::isnan(_Stodx_nid_postfix("nan", &end, 2, &error)) && error == 0 && end[0] == '\0', "_Stodx NaN");
}

void TestIntegerErrors() {
    char* end = nullptr;
    int error = 99;
    *__error_nid_postfix() = 5;
    Require(_Stollx_nid_postfix("123", &end, 0, &error) == 123 && error == 0 && *__error_nid_postfix() == 5, "_Stollx in range");
    Require(_Stollx_nid_postfix("9223372036854775808", &end, 10, &error) == 9223372036854775807ll && error == RangeError && *__error_nid_postfix() == RangeError, "_Stollx overflow");
    *__error_nid_postfix() = 5;
    const char* text = " 12";
    for (int base : {1, -1, 37}) {
        error = 99;
        Require(_Stollx_nid_postfix(text, &end, base, &error) == 0 && error == InvalidArgument && end == text && *__error_nid_postfix() == 5, "_Stollx invalid base");
        error = 99;
        Require(_Stolx_nid_postfix(text, &end, base, &error) == 0 && error == InvalidArgument && end == text && *__error_nid_postfix() == 5, "_Stolx invalid base");
    }
    Require(_Stoll_nid_postfix(text, &end, 1) == 0 && end == text && *__error_nid_postfix() == 5, "_Stoll invalid base");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stolx_nid_postfix("9223372036854775808", &end, 0, &error) == 9223372036854775807ll && error == RangeError && *__error_nid_postfix() == 0, "_Stolx overflow inside the unsigned range");
    Require(_Stolx_nid_postfix("-18446744073709551615", &end, 10, &error) == (-9223372036854775807ll - 1) && error == RangeError && *__error_nid_postfix() == 0, "_Stolx negative overflow inside the unsigned range");
    Require(_Stolx_nid_postfix("18446744073709551616", &end, 10, &error) == 9223372036854775807ll && error == RangeError && *__error_nid_postfix() == RangeError, "_Stolx overflow past the unsigned range");
    error = 99;
    *__error_nid_postfix() = 0;
    Require(_Stoulx_nid_postfix("-1", &end, 0, &error) == 18446744073709551615ull && error == 0, "_Stoulx wraps a negative value");
    Require(_Stoullx_nid_postfix("-18446744073709551615", &end, 10, &error) == 1 && error == 0 && *__error_nid_postfix() == 0, "_Stoullx wraps a negated maximum");
    Require(_Stoullx_nid_postfix("18446744073709551616", &end, 10, &error) == 18446744073709551615ull && error == RangeError && *__error_nid_postfix() == RangeError, "_Stoullx overflow");
}

void TestDispatch() {
    Require(_Sin_nid_postfix(0.5, 0) == std::sin(0.5) && _Sin_nid_postfix(0.5, 1) == std::cos(0.5) && _Sin_nid_postfix(0.5, 7) == std::cos(0.5), "_Sin");
    Require(_FSin_nid_postfix(0.5f, 0) == std::sin(0.5f) && _FSin_nid_postfix(0.5f, 2) == std::cos(0.5f), "_FSin");
    Require(_LSin_nid_postfix(0.5L, 0) == std::sin(0.5L) && _LSin_nid_postfix(0.5L, 3) == std::cos(0.5L), "_LSin");
    Require(_Log_nid_postfix(8.0, 0) == std::log(8.0) && _Log_nid_postfix(1000.0, 1) == 3.0 && _Log_nid_postfix(1000.0, 5) == 3.0 && _Log_nid_postfix(8.0, -1) == 3.0, "_Log");
    Require(_FLog_nid_postfix(8.0f, 0) == std::log(8.0f) && _FLog_nid_postfix(100.0f, 1) == 2.0f && _FLog_nid_postfix(8.0f, -3) == 3.0f, "_FLog");
}

void TestLongDoubleHelpers() {
    Require(_LDivide_nid_postfix(3.0L, 2.0L) == 1.5L, "_LDivide quotient");
    Require(std::isinf(_LDivide_nid_postfix(1.0L, 0.0L)), "_LDivide infinity");
    Require(std::isnan(_LDivide_nid_postfix(0.0L, 0.0L)), "_LDivide invalid");
    Require(_LRecip_nid_postfix(4.0L) == 0.25L, "_LRecip quotient");
    Require(std::isinf(_LRecip_nid_postfix(-0.0L)) && std::signbit(_LRecip_nid_postfix(-0.0L)), "_LRecip signed infinity");
    Require(_LLog_nid_postfix(8.0L, 0) == std::log(8.0L), "_LLog natural");
    Require(std::fabs(_LLog_nid_postfix(1000.0L, 1) - 3.0L) <= 1e-17L && std::fabs(_LLog_nid_postfix(1000.0L, 5) - 3.0L) <= 1e-17L, "_LLog base 10");
    Require(_LLog_nid_postfix(8.0L, -1) == 3.0L && _LLog_nid_postfix(8.0L, -3) == 3.0L, "_LLog base 2");
}

}

int main() {
    TestStod();
    TestStoll();
    TestFloatErrors();
    TestIntegerErrors();
    TestArithmetic();
    TestDispatch();
    TestLongDoubleHelpers();
}
