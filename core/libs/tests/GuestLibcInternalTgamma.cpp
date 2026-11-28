#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal tgamma: %s\n", message);
    std::abort();
}

template <typename T>
bool Same(T left, T right) {
    if (std::isnan(left) && std::isnan(right)) return true;
    return left == right && std::signbit(left) == std::signbit(right);
}

template <typename T>
bool Near(T left, T right, T tolerance) {
    return std::abs(left - right) <= tolerance;
}

void TestDouble() {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    Require(Same(_Tgamma_nid_postfix(1.0), 1.0), "_Tgamma 1.0");
    Require(Same(_Tgamma_nid_postfix(2.0), 1.0), "_Tgamma 2.0");
    Require(Same(_Tgamma_nid_postfix(3.0), 2.0), "_Tgamma 3.0");
    Require(Same(_Tgamma_nid_postfix(4.0), 6.0), "_Tgamma 4.0");
    Require(Same(_Tgamma_nid_postfix(5.0), 24.0), "_Tgamma 5.0");
    Require(Near(_Tgamma_nid_postfix(0.5), 1.772453850905516027, 1e-15), "_Tgamma 0.5");
    Require(Near(_Tgamma_nid_postfix(1.5), 0.886226925452758013, 1e-15), "_Tgamma 1.5");

    Require(std::isinf(_Tgamma_nid_postfix(0.0)), "_Tgamma 0.0");
    Require(std::isnan(_Tgamma_nid_postfix(-1.0)), "_Tgamma -1.0");
    Require(std::isnan(_Tgamma_nid_postfix(-2.0)), "_Tgamma -2.0");
    Require(Same(_Tgamma_nid_postfix(infinity), infinity), "_Tgamma +inf");
    Require(std::isnan(_Tgamma_nid_postfix(-infinity)), "_Tgamma -inf");
    Require(std::isnan(_Tgamma_nid_postfix(nan)), "_Tgamma nan");

    const double values[] = {0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0, -0.5, -1.5, -2.5};
    for (double v : values) {
        Require(Same(_Tgamma_nid_postfix(v), tgamma_nid_postfix(v)), "_Tgamma matches tgamma");
    }
}

void TestFloat() {
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    Require(Same(_FTgamma_nid_postfix(1.0f), 1.0f), "_FTgamma 1.0");
    Require(Same(_FTgamma_nid_postfix(2.0f), 1.0f), "_FTgamma 2.0");
    Require(Same(_FTgamma_nid_postfix(3.0f), 2.0f), "_FTgamma 3.0");
    Require(Same(_FTgamma_nid_postfix(4.0f), 6.0f), "_FTgamma 4.0");
    Require(Same(_FTgamma_nid_postfix(5.0f), 24.0f), "_FTgamma 5.0");
    Require(Near(_FTgamma_nid_postfix(0.5f), 1.77245385f, 1e-6f), "_FTgamma 0.5");
    Require(Near(_FTgamma_nid_postfix(1.5f), 0.8862269f, 1e-6f), "_FTgamma 1.5");

    Require(std::isinf(_FTgamma_nid_postfix(0.0f)), "_FTgamma 0.0");
    Require(std::isnan(_FTgamma_nid_postfix(-1.0f)), "_FTgamma -1.0");
    Require(std::isnan(_FTgamma_nid_postfix(-2.0f)), "_FTgamma -2.0");
    Require(Same(_FTgamma_nid_postfix(infinity), infinity), "_FTgamma +inf");
    Require(std::isnan(_FTgamma_nid_postfix(-infinity)), "_FTgamma -inf");
    Require(std::isnan(_FTgamma_nid_postfix(nan)), "_FTgamma nan");

    const float values[] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, -0.5f, -1.5f, -2.5f};
    for (float v : values) {
        Require(Same(_FTgamma_nid_postfix(v), tgammaf_nid_postfix(v)), "_FTgamma matches tgammaf");
    }
}

void TestLongDouble() {
    const long double infinity = std::numeric_limits<long double>::infinity();
    const long double nan = std::numeric_limits<long double>::quiet_NaN();

    Require(Same(_LTgamma_nid_postfix(1.0L), 1.0L), "_LTgamma 1.0");
    Require(Same(_LTgamma_nid_postfix(2.0L), 1.0L), "_LTgamma 2.0");
    Require(Same(_LTgamma_nid_postfix(3.0L), 2.0L), "_LTgamma 3.0");
    Require(Same(_LTgamma_nid_postfix(4.0L), 6.0L), "_LTgamma 4.0");
    Require(Same(_LTgamma_nid_postfix(5.0L), 24.0L), "_LTgamma 5.0");
    Require(Near(_LTgamma_nid_postfix(0.5L), 1.772453850905516027L, 1e-15L), "_LTgamma 0.5");
    Require(Near(_LTgamma_nid_postfix(1.5L), 0.886226925452758013L, 1e-15L), "_LTgamma 1.5");

    Require(std::isinf(_LTgamma_nid_postfix(0.0L)), "_LTgamma 0.0");
    Require(std::isnan(_LTgamma_nid_postfix(-1.0L)), "_LTgamma -1.0");
    Require(std::isnan(_LTgamma_nid_postfix(-2.0L)), "_LTgamma -2.0");
    Require(Same(_LTgamma_nid_postfix(infinity), infinity), "_LTgamma +inf");
    Require(std::isnan(_LTgamma_nid_postfix(-infinity)), "_LTgamma -inf");
    Require(std::isnan(_LTgamma_nid_postfix(nan)), "_LTgamma nan");

    const long double values[] = {0.5L, 1.0L, 1.5L, 2.0L, 2.5L, 3.0L, 3.5L, 4.0L, 5.0L, -0.5L, -1.5L, -2.5L};
    for (long double v : values) {
        Require(Same(_LTgamma_nid_postfix(v), tgammal_nid_postfix(v)), "_LTgamma matches tgammal");
    }
}

}

int main() {
    TestDouble();
    TestFloat();
    TestLongDouble();
    return 0;
}
