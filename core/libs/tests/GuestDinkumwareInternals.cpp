#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>

namespace {

constexpr int RangeError = 34;
constexpr int DomainError = 33;
constexpr int InvalidArgument = 22;
constexpr short Denormal = -2;
constexpr short Finite = -1;
constexpr short Zero = 0;
constexpr short Infinite = 1;
constexpr short NotANumber = 2;
constexpr int FloatingExceptions = FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT;

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

template<typename TFloat>
void CheckHexRange(TFloat (APS5_VABI *parse)(const char*, char**, std::int64_t, int*), TFloat (APS5_VABI *simple)(const char*, char**, std::int64_t), const char* overflow, const char* underflow) {
    for (const bool negative : {false, true}) for (const bool overflows : {false, true}) for (const int saved : {7, RangeError}) {
        const std::string text = std::string(negative ? " \t-" : "+") + (overflows ? overflow : underflow) + "tail";
        const auto matches = [&](TFloat value) {
            return (overflows ? std::isinf(value) : value == 0) && std::signbit(value) == negative;
        };
        char* end = nullptr;
        int error = 123;
        *__error_nid_postfix() = saved;
        const auto value = parse(text.c_str(), &end, 0, &error);
        Require(matches(value) && end == text.c_str() + text.size() - 4 && error == 0 && *__error_nid_postfix() == saved, "unscaled hexadecimal range result preserves errno and clears error");
        end = nullptr;
        *__error_nid_postfix() = saved;
        const auto plain = simple(text.c_str(), &end, 0);
        Require(matches(plain) && end == text.c_str() + text.size() - 4 && *__error_nid_postfix() == saved, "unscaled hexadecimal range wrapper preserves errno");
    }
}

void TestHexRange() {
    std::fenv_t saved;
    Require(std::fegetenv(&saved) == 0 && std::fesetround(FE_TONEAREST) == 0, "save hexadecimal range test environment");
    CheckHexRange(_Stofx_nid_postfix, _Stof_nid_postfix, "0X1p128", "0X1p-150");
    CheckHexRange(_Stodx_nid_postfix, _Stod_nid_postfix, "0x1p1024", "0x1p-1075");
    CheckHexRange(_Stoldx_nid_postfix, _Stold_nid_postfix, "0x1p16384", "0x1p-16446");
    Require(std::fesetenv(&saved) == 0, "restore hexadecimal range test environment");
}

void CheckExp(double x, double y, short offset, double expected, short expectedKind, int expectedErrno, int expectedFlags, const char* message) {
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    double result = x;
    const short kind = _Exp_nid_postfix(&result, y, offset);
    const bool same = std::isnan(expected) ? std::isnan(result) : result == expected && std::signbit(result) == std::signbit(expected);
    Require(kind == expectedKind && same && errno == expectedErrno && (std::fetestexcept(FE_ALL_EXCEPT) & FloatingExceptions) == expectedFlags, message);
}

void CheckFExp(float x, float y, short offset, float expected, short expectedKind, int expectedErrno, int expectedFlags, const char* message) {
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    float result = x;
    const short kind = _FExp_nid_postfix(&result, y, offset);
    const bool same = std::isnan(expected) ? std::isnan(result) : result == expected && std::signbit(result) == std::signbit(expected);
    Require(kind == expectedKind && same && errno == expectedErrno && (std::fetestexcept(FE_ALL_EXCEPT) & FloatingExceptions) == expectedFlags, message);
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

void TestFloatingRounds() {
    GuestFloatingPointEnvironment saved;
    Require(fegetenv_nid_postfix(&saved) == 0, "save floating environment");
    constexpr int Expected[]{1, 3, 2, 0};
    const int savedErrno = *__error_nid_postfix();
    for (const unsigned precision : {0u, 0x200u, 0x300u}) {
        for (unsigned x87 = 0; x87 != 4; ++x87) {
            for (unsigned sse = 0; sse != 4; ++sse) {
                for (const unsigned flush : {0u, 0x40u, 0x8000u, 0x8040u}) {
                    auto input = saved;
                    input.control = (input.control & 0xffff0000u) | 0x7fu | precision | (x87 << 10u);
                    input.status = (input.status & ~0x3fu) | 0x25u;
                    input.mxcsr = 0x1fa5u | (sse << 13u) | flush;
                    asm volatile("fldenv %0" : : "m"(input));
                    asm volatile("ldmxcsr %0" : : "m"(input.mxcsr));
                    *__error_nid_postfix() = 97;
                    Require(_Fltrounds_nid_postfix() == Expected[x87], "_Fltrounds follows x87 independently of SSE");
                    GuestFloatingPointEnvironment after;
                    Require(fegetenv_nid_postfix(&after) == 0, "read floating environment");
                    Require(after.control == input.control && (after.status & 0x3fu) == 0x25u && after.mxcsr == input.mxcsr, "_Fltrounds preserves controls and flags");
                    Require(*__error_nid_postfix() == 97, "_Fltrounds preserves errno");
                }
            }
        }
    }
    asm volatile("fldenv %0" : : "m"(saved));
    asm volatile("ldmxcsr %0" : : "m"(saved.mxcsr));
    *__error_nid_postfix() = savedErrno;
}

void TestDoubleHelpers() {
    const double e = 0x1.5bf0a8b145769p+1;
    CheckExp(1.0, 1.0, 0, e, Finite, 0, 0, "_Exp e");
    CheckExp(0.0, 1.0, 0, 1.0, Finite, 0, 0, "_Exp zero exponent");
    CheckExp(0.0, 5.0, 0, 5.0, Finite, 0, 0, "_Exp multiplier at zero exponent");
    CheckExp(1.0, -0.0, 0, -0.0, Zero, 0, 0, "_Exp keeps a negative zero multiplier");
    CheckExp(1.0, 0.0, 0, 0.0, Zero, 0, 0, "_Exp zero multiplier");
    CheckExp(-1.0, -0.0, 0, -0.0, Zero, 0, 0, "_Exp negative zero multiplier");
    CheckExp(INFINITY, -0.0, 0, std::numeric_limits<double>::quiet_NaN(), NotANumber, EDOM, FE_INVALID, "_Exp positive infinity with zero multiplier is invalid");
    CheckExp(INFINITY, 0.0, 0, std::numeric_limits<double>::quiet_NaN(), NotANumber, EDOM, FE_INVALID, "_Exp positive infinity with positive zero is invalid");
    CheckExp(INFINITY, 1.0, 0, INFINITY, Infinite, 0, 0, "_Exp positive infinity");
    CheckExp(INFINITY, -1.0, 0, -INFINITY, Infinite, 0, 0, "_Exp positive infinity preserves the multiplier sign");
    CheckExp(INFINITY, INFINITY, 0, INFINITY, Infinite, 0, 0, "_Exp positive infinity with infinite multiplier");
    CheckExp(INFINITY, -INFINITY, 0, -INFINITY, Infinite, 0, 0, "_Exp positive infinity with negative infinite multiplier");
    CheckExp(-INFINITY, 1.0, 0, 0.0, Zero, 0, 0, "_Exp negative infinity");
    CheckExp(-INFINITY, -1.0, 0, -0.0, Zero, 0, 0, "_Exp negative infinity preserves the multiplier sign");
    CheckExp(-INFINITY, 0.0, 0, 0.0, Zero, 0, 0, "_Exp negative infinity with positive zero");
    CheckExp(-INFINITY, -0.0, 0, -0.0, Zero, 0, 0, "_Exp negative infinity with negative zero");
    CheckExp(-INFINITY, INFINITY, 0, std::numeric_limits<double>::quiet_NaN(), NotANumber, EDOM, FE_INVALID, "_Exp negative infinity with infinite multiplier is invalid");
    CheckExp(-INFINITY, -INFINITY, 0, std::numeric_limits<double>::quiet_NaN(), NotANumber, EDOM, FE_INVALID, "_Exp negative infinity with negative infinite multiplier is invalid");
    CheckExp(0.0, INFINITY, 0, INFINITY, Infinite, 0, 0, "_Exp zero exponent with infinite multiplier");
    CheckExp(0.0, -INFINITY, 0, -INFINITY, Infinite, 0, 0, "_Exp zero exponent with negative infinite multiplier");
    CheckExp(1.0, INFINITY, 0, INFINITY, Infinite, 0, 0, "_Exp finite exponent with infinite multiplier");
    CheckExp(1.0, -INFINITY, 0, -INFINITY, Infinite, 0, 0, "_Exp finite exponent with negative infinite multiplier");
    CheckExp(1.0, std::numeric_limits<double>::quiet_NaN(), 0, 1.0, NotANumber, 0, 0, "_Exp NaN multiplier leaves the exponent");
    CheckExp(INFINITY, std::numeric_limits<double>::quiet_NaN(), 0, INFINITY, NotANumber, 0, 0, "_Exp NaN multiplier leaves an infinite exponent");
    CheckExp(std::numeric_limits<double>::quiet_NaN(), 0.0, 0, std::numeric_limits<double>::quiet_NaN(), NotANumber, 0, 0, "_Exp NaN exponent with zero multiplier");
    CheckExp(std::numeric_limits<double>::quiet_NaN(), 1.0, 0, std::numeric_limits<double>::quiet_NaN(), NotANumber, 0, 0, "_Exp NaN exponent");
    CheckExp(std::numeric_limits<double>::quiet_NaN(), INFINITY, 0, std::numeric_limits<double>::quiet_NaN(), NotANumber, 0, 0, "_Exp NaN exponent with infinite multiplier");
    CheckExp(1843.0, 1.0, 0, INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_Exp positive cutoff");
    CheckExp(1843.0, -1.0, 0, -INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_Exp positive cutoff preserves the multiplier sign");
    CheckExp(1843.0, 1.0, -2660, INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_Exp positive cutoff ignores a compensating offset");
    CheckExp(-1843.0, 1.0, 0, 0.0, Zero, 0, 0, "_Exp negative cutoff");
    CheckExp(-1843.0, -1.0, 0, -0.0, Zero, 0, 0, "_Exp negative cutoff preserves the multiplier sign");
    CheckExp(-1843.0, 1.0, 2660, 0.0, Zero, 0, 0, "_Exp negative cutoff ignores a compensating offset");
    CheckExp(1843.0, 0.0, 0, 0.0, Zero, 0, 0, "_Exp positive cutoff with zero multiplier");
    CheckExp(1843.0, std::numeric_limits<double>::quiet_NaN(), 0, 1843.0, NotANumber, 0, 0, "_Exp positive cutoff with NaN multiplier");
    CheckExp(0.0, 1.0, -1074, std::numeric_limits<double>::denorm_min(), Finite, 0, 0, "_Exp exact minimum subnormal");
    CheckExp(0.0, 1.0, -1075, 0.0, Zero, ERANGE, FE_UNDERFLOW | FE_INEXACT, "_Exp scaled underflow");
    CheckExp(0.0, -1.0, -1075, -0.0, Zero, ERANGE, FE_UNDERFLOW | FE_INEXACT, "_Exp scaled underflow preserves the multiplier sign");
    CheckExp(0.0, 1.0, 100, std::scalbn(1.0, 100), Finite, 0, 0, "_Exp scaled finite result");
    CheckExp(0.0, 1.0, 2000, INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_Exp scaled overflow");
    CheckExp(1842.0, 1.0, 0, INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_Exp inside the positive cutoff overflows");
    CheckExp(-1842.0, 1.0, 0, 0.0, Zero, ERANGE, FE_UNDERFLOW | FE_INEXACT, "_Exp inside the negative cutoff underflows");
    CheckExp(-0x1.a3ae147ae147bp+3, 1.0, 3, 0x1.0e6b5a0a65117p-16, Finite, 0, FE_INEXACT, "_Exp rounds a negative exponent as titles do");
    CheckExp(-0x1.e1fae147ae148p+6, 2.0, 3, 0x1.1e838b63af9c1p-170, Finite, 0, FE_INEXACT, "_Exp rounds a scaled small result as titles do");
    CheckExp(0x1.b073b645a1cacp+7, -1.0, -1, -0x1.edef67d6786cep+310, Finite, 0, FE_INEXACT, "_Exp rounds a large negative product as titles do");
    CheckExp(0x1.5c523eb57b55fp-53, 2.0, 0, 0x1.0000000000001p+1, Finite, 0, FE_INEXACT, "_Exp evaluates arguments above half an epsilon");

    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    double result = 1.0;
    Require(_Exp_nid_postfix(&result, 3.0, 0) == Finite && std::fabs(result - 3.0 * e) <= 3.0 * e * 1e-14, "_Exp multiplier");
    result = 1.0;
    Require(_Exp_nid_postfix(&result, -2.0, 0) == Finite && std::fabs(result + 2.0 * e) <= 2.0 * e * 1e-14, "_Exp negative multiplier");
    result = 1.0;
    Require(_Exp_nid_postfix(&result, 1.0, 10) == Finite && std::fabs(result - e * 1024.0) <= e * 1024.0 * 1e-14, "_Exp scale");
    for (int i = -200; i <= 200; ++i) {
        const double x = static_cast<double>(i) / 16.0;
        result = x;
        Require(_Exp_nid_postfix(&result, 1.0, 0) == Finite && std::fabs(result - std::exp(x)) <= std::fabs(std::exp(x)) * 1e-13, "_Exp finite sweep");
    }

    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = 1.0;
    Require(_Exp_nid_postfix(&result, std::numeric_limits<double>::denorm_min(), 0) == Finite && std::fpclassify(result) == FP_SUBNORMAL && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0, "_Exp subnormal result classification");
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = -740.0;
    Require(_Exp_nid_postfix(&result, 1.0, 0) == Finite && std::fpclassify(result) == FP_SUBNORMAL && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0, "_Exp negative exponent subnormal result");
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = 1.0;
    Require(_Exp_nid_postfix(&result, std::numeric_limits<double>::max(), 0) == Infinite && std::isinf(result) && errno == ERANGE && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_OVERFLOW | FE_INEXACT)) == (FE_OVERFLOW | FE_INEXACT), "_Exp finite multiplier overflow");

    const double largeExpected = std::scalbn(std::exp(710.0 - 1024.0 * std::log(2.0)) * 1e-100, 1024);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = 710.0;
    Require(_Exp_nid_postfix(&result, 1e-100, 0) == Finite && std::isfinite(result) && std::fabs(result - largeExpected) <= largeExpected * 1e-13 && errno == 0 && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_OVERFLOW | FE_UNDERFLOW)) == 0, "_Exp large exponent stays finite");
    const double smallExpected = std::scalbn(std::exp(-710.0 + 1024.0 * std::log(2.0)) * 1e300, -1024);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = -710.0;
    Require(_Exp_nid_postfix(&result, 1e300, 0) == Finite && std::isfinite(result) && std::fabs(result - smallExpected) <= smallExpected * 1e-13 && errno == 0 && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_OVERFLOW | FE_UNDERFLOW)) == 0, "_Exp small exponent stays finite");
    const double multiplierExpected = std::exp(-0.5) * std::numeric_limits<double>::max();
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    result = -0.5;
    Require(_Exp_nid_postfix(&result, std::numeric_limits<double>::max(), 0) == Finite && std::isfinite(result) && std::fabs(result - multiplierExpected) <= multiplierExpected * 1e-13 && errno == 0 && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_OVERFLOW | FE_UNDERFLOW)) == 0, "_Exp large multiplier stays finite");

    const float ef = 0x1.5bf0a8p+1f;
    CheckFExp(1.0f, 1.0f, 0, ef, Finite, 0, 0, "_FExp e");
    CheckFExp(0.0f, 1.0f, 0, 1.0f, Finite, 0, 0, "_FExp zero exponent");
    CheckFExp(0.0f, 5.0f, 0, 5.0f, Finite, 0, 0, "_FExp multiplier at zero exponent");
    CheckFExp(1.0f, -0.0f, 0, -0.0f, Zero, 0, 0, "_FExp keeps a negative zero multiplier");
    CheckFExp(1.0f, 0.0f, 0, 0.0f, Zero, 0, 0, "_FExp zero multiplier");
    CheckFExp(-1.0f, -0.0f, 0, -0.0f, Zero, 0, 0, "_FExp negative zero multiplier");
    CheckFExp(INFINITY, -0.0f, 0, std::numeric_limits<float>::quiet_NaN(), NotANumber, EDOM, FE_INVALID, "_FExp positive infinity with zero multiplier is invalid");
    CheckFExp(INFINITY, 0.0f, 0, std::numeric_limits<float>::quiet_NaN(), NotANumber, EDOM, FE_INVALID, "_FExp positive infinity with positive zero is invalid");
    CheckFExp(INFINITY, 1.0f, 0, INFINITY, Infinite, 0, 0, "_FExp positive infinity");
    CheckFExp(INFINITY, -1.0f, 0, -INFINITY, Infinite, 0, 0, "_FExp positive infinity preserves the multiplier sign");
    CheckFExp(INFINITY, INFINITY, 0, INFINITY, Infinite, 0, 0, "_FExp positive infinity with infinite multiplier");
    CheckFExp(INFINITY, -INFINITY, 0, -INFINITY, Infinite, 0, 0, "_FExp positive infinity with negative infinite multiplier");
    CheckFExp(-INFINITY, 1.0f, 0, 0.0f, Zero, 0, 0, "_FExp negative infinity");
    CheckFExp(-INFINITY, -1.0f, 0, -0.0f, Zero, 0, 0, "_FExp negative infinity preserves the multiplier sign");
    CheckFExp(-INFINITY, 0.0f, 0, 0.0f, Zero, 0, 0, "_FExp negative infinity with positive zero");
    CheckFExp(-INFINITY, -0.0f, 0, -0.0f, Zero, 0, 0, "_FExp negative infinity with negative zero");
    CheckFExp(-INFINITY, INFINITY, 0, std::numeric_limits<float>::quiet_NaN(), NotANumber, EDOM, FE_INVALID, "_FExp negative infinity with infinite multiplier is invalid");
    CheckFExp(-INFINITY, -INFINITY, 0, std::numeric_limits<float>::quiet_NaN(), NotANumber, EDOM, FE_INVALID, "_FExp negative infinity with negative infinite multiplier is invalid");
    CheckFExp(0.0f, INFINITY, 0, INFINITY, Infinite, 0, 0, "_FExp zero exponent with infinite multiplier");
    CheckFExp(0.0f, -INFINITY, 0, -INFINITY, Infinite, 0, 0, "_FExp zero exponent with negative infinite multiplier");
    CheckFExp(1.0f, INFINITY, 0, INFINITY, Infinite, 0, 0, "_FExp finite exponent with infinite multiplier");
    CheckFExp(1.0f, -INFINITY, 0, -INFINITY, Infinite, 0, 0, "_FExp finite exponent with negative infinite multiplier");
    CheckFExp(1.0f, std::numeric_limits<float>::quiet_NaN(), 0, 1.0f, NotANumber, 0, 0, "_FExp NaN multiplier leaves the exponent");
    CheckFExp(INFINITY, std::numeric_limits<float>::quiet_NaN(), 0, INFINITY, NotANumber, 0, 0, "_FExp NaN multiplier leaves an infinite exponent");
    CheckFExp(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0, std::numeric_limits<float>::quiet_NaN(), NotANumber, 0, 0, "_FExp NaN exponent with zero multiplier");
    CheckFExp(std::numeric_limits<float>::quiet_NaN(), 1.0f, 0, std::numeric_limits<float>::quiet_NaN(), NotANumber, 0, 0, "_FExp NaN exponent");
    CheckFExp(std::numeric_limits<float>::quiet_NaN(), INFINITY, 0, std::numeric_limits<float>::quiet_NaN(), NotANumber, 0, 0, "_FExp NaN exponent with infinite multiplier");
    CheckFExp(230.0f, 1.0f, 0, INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_FExp positive cutoff");
    CheckFExp(230.0f, -1.0f, 0, -INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_FExp positive cutoff preserves the multiplier sign");
    CheckFExp(230.0f, 1.0f, -332, INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_FExp positive cutoff ignores the offset");
    CheckFExp(-230.0f, 1.0f, 0, 0.0f, Zero, 0, 0, "_FExp negative cutoff");
    CheckFExp(-230.0f, -1.0f, 0, -0.0f, Zero, 0, 0, "_FExp negative cutoff preserves the multiplier sign");
    CheckFExp(-230.0f, 1.0f, 332, 0.0f, Zero, 0, 0, "_FExp negative cutoff ignores the offset");
    CheckFExp(230.0f, 0.0f, 0, 0.0f, Zero, 0, 0, "_FExp positive cutoff with zero multiplier");
    CheckFExp(230.0f, std::numeric_limits<float>::quiet_NaN(), 0, 230.0f, NotANumber, 0, 0, "_FExp positive cutoff with NaN multiplier");
    CheckFExp(0.0f, 1.0f, -149, std::numeric_limits<float>::denorm_min(), Finite, 0, 0, "_FExp exact minimum subnormal");
    CheckFExp(0.0f, 1.0f, -150, 0.0f, Zero, ERANGE, FE_UNDERFLOW | FE_INEXACT, "_FExp scaled underflow");
    CheckFExp(0.0f, -1.0f, -150, -0.0f, Zero, ERANGE, FE_UNDERFLOW | FE_INEXACT, "_FExp scaled underflow preserves the multiplier sign");
    CheckFExp(0.0f, 1.0f, 100, std::scalbn(1.0f, 100), Finite, 0, 0, "_FExp scaled finite result");
    CheckFExp(0.0f, 1.0f, 200, INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_FExp scaled overflow");
    CheckFExp(229.0f, 1.0f, 0, INFINITY, Infinite, ERANGE, FE_OVERFLOW | FE_INEXACT, "_FExp inside the positive cutoff overflows");
    CheckFExp(-229.0f, 1.0f, 0, 0.0f, Zero, ERANGE, FE_UNDERFLOW | FE_INEXACT, "_FExp inside the negative cutoff underflows");
    CheckFExp(-0x1.4fbe76p+0f, 1.0f, 3, 0x1.13e1bp+1f, Finite, 0, FE_INEXACT, "_FExp rounds a negative exponent as titles do");
    CheckFExp(-0x1.fd4b92p+5f, 1.0f, -3, 0x1.1d1806p-95f, Finite, 0, FE_INEXACT, "_FExp rounds a scaled small result as titles do");
    CheckFExp(0x1.5e147ap+4f, 2.0f, 3, 0x1.7b078cp+35f, Finite, 0, FE_INEXACT, "_FExp rounds a large product as titles do");
    CheckFExp(0x1.0644b8p-24f, 0.25f, 2, 0x1.000002p+0f, Finite, 0, FE_INEXACT, "_FExp evaluates arguments above half an epsilon");

    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    float resultf = 1.0f;
    Require(_FExp_nid_postfix(&resultf, 3.0f, 0) == Finite && std::fabs(resultf - 3.0f * ef) <= 3.0f * ef * 1e-6f, "_FExp multiplier");
    resultf = 1.0f;
    Require(_FExp_nid_postfix(&resultf, -2.0f, 0) == Finite && std::fabs(resultf + 2.0f * ef) <= 2.0f * ef * 1e-6f, "_FExp negative multiplier");
    resultf = 1.0f;
    Require(_FExp_nid_postfix(&resultf, 1.0f, 10) == Finite && std::fabs(resultf - ef * 1024.0f) <= ef * 1024.0f * 1e-6f, "_FExp scale");
    for (int i = -120; i <= 120; ++i) {
        const float x = static_cast<float>(i) / 16.0f;
        resultf = x;
        Require(_FExp_nid_postfix(&resultf, 1.0f, 0) == Finite && std::fabs(resultf - std::exp(x)) <= std::fabs(std::exp(x)) * 1e-6f, "_FExp finite sweep");
    }

    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    resultf = 1.0f;
    Require(_FExp_nid_postfix(&resultf, std::numeric_limits<float>::denorm_min(), 0) == Finite && std::fpclassify(resultf) == FP_SUBNORMAL && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0, "_FExp subnormal result classification");
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    resultf = -100.0f;
    Require(_FExp_nid_postfix(&resultf, 1.0f, 0) == Finite && std::fpclassify(resultf) == FP_SUBNORMAL && errno == 0 && std::fetestexcept(FE_ALL_EXCEPT) == 0, "_FExp negative exponent subnormal result");
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    resultf = 1.0f;
    Require(_FExp_nid_postfix(&resultf, std::numeric_limits<float>::max(), 0) == Infinite && std::isinf(resultf) && errno == ERANGE && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_OVERFLOW | FE_INEXACT)) == (FE_OVERFLOW | FE_INEXACT), "_FExp finite multiplier overflow");

    const float largeExpectedf = static_cast<float>(std::exp(100.0) * 1e-35);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    resultf = 100.0f;
    Require(_FExp_nid_postfix(&resultf, 1e-35f, 0) == Finite && std::isfinite(resultf) && std::fabs(resultf - largeExpectedf) <= largeExpectedf * 1e-6f && errno == 0 && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_OVERFLOW | FE_UNDERFLOW)) == 0, "_FExp large exponent stays finite");
    const float smallExpectedf = static_cast<float>(std::exp(-100.0) * 1e30);
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    resultf = -100.0f;
    Require(_FExp_nid_postfix(&resultf, 1e30f, 0) == Finite && std::isfinite(resultf) && std::fabs(resultf - smallExpectedf) <= smallExpectedf * 1e-6f && errno == 0 && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_OVERFLOW | FE_UNDERFLOW)) == 0, "_FExp small exponent stays finite");
    const float multiplierExpectedf = static_cast<float>(std::exp(-0.5) * std::numeric_limits<float>::max());
    errno = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    resultf = -0.5f;
    Require(_FExp_nid_postfix(&resultf, std::numeric_limits<float>::max(), 0) == Finite && std::isfinite(resultf) && std::fabs(resultf - multiplierExpectedf) <= multiplierExpectedf * 1e-6f && errno == 0 && (std::fetestexcept(FE_ALL_EXCEPT) & (FE_OVERFLOW | FE_UNDERFLOW)) == 0, "_FExp large multiplier stays finite");
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

long double Extended(std::uint64_t mantissa, std::uint16_t signExponent) {
    unsigned char bytes[sizeof(long double)]{};
    std::memcpy(bytes, &mantissa, sizeof(mantissa));
    std::memcpy(bytes + sizeof(mantissa), &signExponent, sizeof(signExponent));
    long double value;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

bool SameBits(long double value, std::uint64_t mantissa, std::uint16_t signExponent) {
    unsigned char bytes[sizeof(long double)];
    std::memcpy(bytes, &value, sizeof(value));
    std::uint64_t actualMantissa;
    std::uint16_t actualSignExponent;
    std::memcpy(&actualMantissa, bytes, sizeof(actualMantissa));
    std::memcpy(&actualSignExponent, bytes + sizeof(actualMantissa), sizeof(actualSignExponent));
    return actualMantissa == mantissa && actualSignExponent == signExponent;
}

void TestLongLog() {
    const struct { std::uint64_t inputMantissa; std::uint16_t inputSignExponent; int base; std::uint64_t outputMantissa; std::uint16_t outputSignExponent; } cases[] = {
        {0x8000000000000000ull, 0x4002, 0, 0x851591f9dd5b9b41ull, 0x4000}, {0xfa00000000000000ull, 0x4008, 1, 0xc000000000000000ull, 0x4000},
        {0xfa00000000000000ull, 0x4008, 5, 0xc000000000000000ull, 0x4000}, {0x8000000000000000ull, 0x4002, -1, 0xc000000000000000ull, 0x4000},
        {0x8000000000000000ull, 0x3fff, 0, 0, 0x0000}, {0x8000000000000000ull, 0x4000, 0, 0xb17217f7d1cf79acull, 0x3ffe},
        {0xa000000000000000ull, 0x4002, 1, 0x8000000000000000ull, 0x3fff}, {0x8000000000000000ull, 0x3ffe, -1, 0x8000000000000000ull, 0xbfff},
        {0xc000000000000000ull, 0x4000, 0, 0x8c9f53d5681854bbull, 0x3fff}, {0xb333333333333333ull, 0x3ffe, 0, 0xb69e192709fa762aull, 0xbffd},
        {0x9c3d73864f3805c0ull, 0x0c17, 0, 0x8fe95c8a78a8b80eull, 0xc00c}, {0xd1ba8323fe558c61ull, 0x73e6, 1, 0xfa00000000000000ull, 0x400a},
        {0xe000000000000000ull, 0x4001, -1, 0xb3abb3faa02166cdull, 0x4000},
        {0xb9ac82c8410c3c68ull, 0x3ff2, 1, 0xf01e4bc814a4fb4cull, 0xc000}, {0x98a9e5bdab7504d1ull, 0x3ff5, 1, 0xbbc2f513876414b1ull, 0xc000},
        {0xcb41eecb51a96e38ull, 0x3ff8, 1, 0xf40409ebb181947aull, 0xbfff}, {0xecad15fd3a0eaf66ull, 0x3ffd, 0, 0xc589acfa7c6c1534ull, 0xbffe},
        {0xc86e97283b961889ull, 0x3ffe, 1, 0xd9a59da3f27f263eull, 0xbffb}, {0xf164446eb384ba65ull, 0x4001, 1, 0xe0a894064e71356aull, 0x3ffe},
        {0x8000000000000000ull, 0x7fff, 0, 0x8000000000000000ull, 0x7fff}, {0xc000000000000001ull, 0xffff, 1, 0xc000000000000001ull, 0xffff},
    };
    for (const auto& test : cases) {
        *__error_nid_postfix() = 12345;
        const long double result = _LLog_nid_postfix(Extended(test.inputMantissa, test.inputSignExponent), test.base);
        if (!SameBits(result, test.outputMantissa, test.outputSignExponent) || *__error_nid_postfix() != 12345) {
            std::fprintf(stderr, "Dinkumware internals: _LLog failed for %016llx:%04x base %d\n", static_cast<unsigned long long>(test.inputMantissa), test.inputSignExponent, test.base);
            std::exit(1);
        }
    }
    for (const std::uint16_t signExponent : {0x0000, 0x8000, 0x3fff, 0xbfff}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        *__error_nid_postfix() = 0;
        const long double zero = _LLog_nid_postfix(Extended(0, signExponent), 0);
        Require(SameBits(zero, 0x8000000000000000ull, 0xffff) && *__error_nid_postfix() == RangeError && std::fetestexcept(FE_DIVBYZERO) != 0, "_LLog of a zero mantissa");
    }
    for (const long double input : {-1.0L, -0x1p-16445L, Extended(0x8000000000000000ull, 0xffff)}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        *__error_nid_postfix() = 0;
        Require(SameBits(_LLog_nid_postfix(input, -1), 0xc000000000000000ull, 0x7fff) && *__error_nid_postfix() == DomainError && std::fetestexcept(FE_INVALID) != 0, "_LLog of a negative argument");
    }
    *__error_nid_postfix() = 0;
}

std::uint16_t X87ControlWord() {
    std::uint16_t control;
    asm volatile("fnstcw %0" : "=m"(control));
    return control;
}

void TestLongLogAt53Bits() {
    const struct { std::uint64_t inputMantissa; std::uint16_t inputSignExponent; int base; std::uint64_t outputMantissa; std::uint16_t outputSignExponent; } cases[] = {
        {0x8000000000000000ull, 0x4002, 0, 0x851591f9dd5b9800ull, 0x4000}, {0xfa00000000000000ull, 0x4008, 1, 0xc000000000000000ull, 0x4000},
        {0x8000000000000000ull, 0x4000, 0, 0xb17217f7d1cf7800ull, 0x3ffe}, {0xb333333333333333ull, 0x3ffe, 0, 0xb69e192709fa8000ull, 0xbffd},
        {0x9c3d73864f3805c0ull, 0x0c17, 0, 0x8fe95c8a78a8b800ull, 0xc00c}, {0xe000000000000000ull, 0x4001, -1, 0xb3abb3faa0216800ull, 0x4000},
        {0xf164446eb384ba65ull, 0x4001, 1, 0xe0a894064e713800ull, 0x3ffe},
    };
    const std::uint16_t saved = X87ControlWord();
    const auto control = static_cast<std::uint16_t>((saved & ~0x0300u) | 0x0200u);
    for (const auto& test : cases) {
        const long double input = Extended(test.inputMantissa, test.inputSignExponent);
        asm volatile("fldcw %0" : : "m"(control) : "memory");
        const long double result = _LLog_nid_postfix(input, test.base);
        const std::uint16_t after = X87ControlWord();
        asm volatile("fldcw %0" : : "m"(saved) : "memory");
        if (!SameBits(result, test.outputMantissa, test.outputSignExponent) || after != control) {
            std::fprintf(stderr, "Dinkumware internals: _LLog at 53 bits failed for %016llx:%04x base %d\n", static_cast<unsigned long long>(test.inputMantissa), test.inputSignExponent, test.base);
            std::exit(1);
        }
    }
}

}

int main() {
    TestStod();
    TestHexRange();
    TestStoll();
    TestDoubleHelpers();
    TestFloatingRounds();
    TestFloatErrors();
    TestIntegerErrors();
    TestArithmetic();
    TestDispatch();
    TestLongDoubleHelpers();
    TestLongLog();
    TestLongLogAt53Bits();
}
