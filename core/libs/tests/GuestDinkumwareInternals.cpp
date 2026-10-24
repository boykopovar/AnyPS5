#include "prx/libc/include/general/VabiMacros.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int* APS5_VABI __error_nid_postfix();
double APS5_VABI _Stod_nid_postfix(const char* text, char** end, std::int64_t pten);
long long APS5_VABI _Stoll_nid_postfix(const char* text, char** end, int base);
double APS5_VABI _Sin_nid_postfix(double x, unsigned int quadrant);
float APS5_VABI _FSin_nid_postfix(float x, unsigned int quadrant);
long double APS5_VABI _LSin_nid_postfix(long double x, unsigned int quadrant);
double APS5_VABI _Log_nid_postfix(double x, int base);
float APS5_VABI _FLog_nid_postfix(float x, int base);
}

namespace {

constexpr int RangeError = 34;

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

void TestDispatch() {
    Require(_Sin_nid_postfix(0.5, 0) == std::sin(0.5) && _Sin_nid_postfix(0.5, 1) == std::cos(0.5) && _Sin_nid_postfix(0.5, 7) == std::cos(0.5), "_Sin");
    Require(_FSin_nid_postfix(0.5f, 0) == std::sin(0.5f) && _FSin_nid_postfix(0.5f, 2) == std::cos(0.5f), "_FSin");
    Require(_LSin_nid_postfix(0.5L, 0) == std::sin(0.5L) && _LSin_nid_postfix(0.5L, 3) == std::cos(0.5L), "_LSin");
    Require(_Log_nid_postfix(8.0, 0) == std::log(8.0) && _Log_nid_postfix(1000.0, 1) == 3.0 && _Log_nid_postfix(1000.0, 5) == 3.0 && _Log_nid_postfix(8.0, -1) == 3.0, "_Log");
    Require(_FLog_nid_postfix(8.0f, 0) == std::log(8.0f) && _FLog_nid_postfix(100.0f, 1) == 2.0f && _FLog_nid_postfix(8.0f, -3) == 3.0f, "_FLog");
}

}

int main() {
    TestStod();
    TestStoll();
    TestDispatch();
}
