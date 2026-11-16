#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal scaled hypot: %s\n", message);
    std::abort();
}

void RequireDouble(double x, double y, double result, int exponent, const char* message) {
    int written = 77;
    const double value = _Hypot_nid_postfix(x, y, &written);
    Require(value == result && !std::signbit(value) && written == exponent, message);
}

void TestDouble() {
    RequireDouble(3.0, 4.0, 1.25, 2, "_Hypot 3 4");
    RequireDouble(-5.0, 12.0, 3.25, 2, "_Hypot -5 12");
    RequireDouble(0.0, 0.0, 0.0, 0, "_Hypot 0 0");
    RequireDouble(-0.0, -0.0, 0.0, 0, "_Hypot -0 -0");
    RequireDouble(1e300, 1e300, 1.7281329749791539e150, 498, "_Hypot 1e300 1e300");
    RequireDouble(1e-310, 1e-310, 7.584601526487318e-156, -514, "_Hypot 1e-310 1e-310");
    int written = 77;
    const double tenth = _Hypot_nid_postfix(0.1, 0.2, &written);
    Require(written == 2 && std::ldexp(tenth, written) == hypot_nid_postfix(0.1, 0.2), "_Hypot 0.1 0.2 scales hypot");
    Require(std::fabs(tenth - 0.0559016994374947) < 1e-16, "_Hypot 0.1 0.2 measured value");
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    written = 77;
    Require(std::isnan(_Hypot_nid_postfix(infinity, nan, &written)) && written == 0, "_Hypot inf nan");
    written = 77;
    Require(std::isnan(_Hypot_nid_postfix(nan, infinity, &written)) && written == 0, "_Hypot nan inf");
    written = 77;
    Require(std::isnan(_Hypot_nid_postfix(nan, 1.0, &written)) && written == 0, "_Hypot nan 1");
    written = 77;
    Require(_Hypot_nid_postfix(-infinity, 1.0, &written) == infinity && written == 0, "_Hypot -inf 1");
    written = 77;
    Require(_Hypot_nid_postfix(1.0, infinity, &written) == infinity && written == 0, "_Hypot 1 inf");
    const double pairs[][2] = {{3.0, 4.0}, {1e308, 1e308}, {1e308, 1.0}, {5e-324, 5e-324}, {2.0, 0.0}, {0.0, -7.5}, {1e-200, 3e200}};
    for (const auto& pair : pairs) {
        written = 77;
        const double value = _Hypot_nid_postfix(pair[0], pair[1], &written);
        Require(std::ldexp(value, written) == hypot_nid_postfix(pair[0], pair[1]), "_Hypot result times two to the exponent is hypot");
        Require(written % 2 == 0 && written != 0, "_Hypot exponent is even and nonzero for a finite nonzero argument");
    }
}

void TestFloat() {
    int written = 77;
    Require(_FHypot_nid_postfix(3.0f, 4.0f, &written) == 1.25f && written == 2, "_FHypot 3 4");
    written = 77;
    Require(std::isnan(_FHypot_nid_postfix(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), &written)) && written == 0, "_FHypot inf nan");
    written = 77;
    Require(_FHypot_nid_postfix(0.0f, 0.0f, &written) == 0.0f && written == 0, "_FHypot 0 0");
    written = 77;
    const float large = _FHypot_nid_postfix(3e38f, 3e38f, &written);
    Require(written == 64 && std::ldexp(large, written) == hypotf_nid_postfix(3e38f, 3e38f), "_FHypot scales a result past FLT_MAX");
}

void TestLongDouble() {
    int written = 77;
    Require(_LHypot_nid_postfix(3.0L, 4.0L, &written) == 1.25L && written == 2, "_LHypot 3 4");
    written = 77;
    Require(std::isnan(_LHypot_nid_postfix(std::numeric_limits<long double>::infinity(), std::numeric_limits<long double>::quiet_NaN(), &written)) && written == 0, "_LHypot inf nan");
    written = 77;
    Require(_LHypot_nid_postfix(0.0L, 0.0L, &written) == 0.0L && written == 0, "_LHypot 0 0");
}

void TestUnwritableExponent() {
    bool thrown = false;
    try {
        _Hypot_nid_postfix(3.0, 4.0, nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown, "_Hypot null exponent throws");
}

}

int main() {
    TestDouble();
    TestFloat();
    TestLongDouble();
    TestUnwritableExponent();
    return 0;
}
