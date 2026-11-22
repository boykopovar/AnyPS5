#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal rint: %s\n", message);
    std::abort();
}

template <typename T>
bool Same(T left, T right) {
    if (std::isnan(left) && std::isnan(right)) return true;
    return left == right && std::signbit(left) == std::signbit(right);
}

void TestDouble() {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    Require(Same(_Rint_nid_postfix(0.0), 0.0), "_Rint 0.0");
    Require(Same(_Rint_nid_postfix(-0.0), -0.0), "_Rint -0.0");
    Require(Same(_Rint_nid_postfix(1.0), 1.0), "_Rint 1.0");
    Require(Same(_Rint_nid_postfix(-1.0), -1.0), "_Rint -1.0");
    Require(Same(_Rint_nid_postfix(0.3), 0.0), "_Rint 0.3");
    Require(Same(_Rint_nid_postfix(0.7), 1.0), "_Rint 0.7");
    Require(Same(_Rint_nid_postfix(1.4), 1.0), "_Rint 1.4");
    Require(Same(_Rint_nid_postfix(1.5), 2.0), "_Rint 1.5");
    Require(Same(_Rint_nid_postfix(2.5), 2.0), "_Rint 2.5");
    Require(Same(_Rint_nid_postfix(2.6), 3.0), "_Rint 2.6");
    Require(Same(_Rint_nid_postfix(-0.3), -0.0), "_Rint -0.3");
    Require(Same(_Rint_nid_postfix(-0.7), -1.0), "_Rint -0.7");
    Require(Same(_Rint_nid_postfix(-1.4), -1.0), "_Rint -1.4");
    Require(Same(_Rint_nid_postfix(-1.5), -2.0), "_Rint -1.5");
    Require(Same(_Rint_nid_postfix(-2.5), -2.0), "_Rint -2.5");
    Require(Same(_Rint_nid_postfix(1e15), 1e15), "_Rint 1e15");
    Require(Same(_Rint_nid_postfix(infinity), infinity), "_Rint +inf");
    Require(Same(_Rint_nid_postfix(-infinity), -infinity), "_Rint -inf");
    Require(std::isnan(_Rint_nid_postfix(nan)), "_Rint nan");

    const double values[] = {0.1, 0.49, 0.5, 0.51, 1.49, 1.5, 1.51, 2.5, 3.5, -0.49, -0.5, -0.51, -1.5, -2.5};
    for (double v : values) {
        Require(Same(_Rint_nid_postfix(v), rint_nid_postfix(v)), "_Rint matches rint");
    }
}

void TestFloat() {
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    Require(Same(_FRint_nid_postfix(0.0f), 0.0f), "_FRint 0.0");
    Require(Same(_FRint_nid_postfix(-0.0f), -0.0f), "_FRint -0.0");
    Require(Same(_FRint_nid_postfix(1.0f), 1.0f), "_FRint 1.0");
    Require(Same(_FRint_nid_postfix(-1.0f), -1.0f), "_FRint -1.0");
    Require(Same(_FRint_nid_postfix(0.3f), 0.0f), "_FRint 0.3");
    Require(Same(_FRint_nid_postfix(0.7f), 1.0f), "_FRint 0.7");
    Require(Same(_FRint_nid_postfix(1.4f), 1.0f), "_FRint 1.4");
    Require(Same(_FRint_nid_postfix(1.5f), 2.0f), "_FRint 1.5");
    Require(Same(_FRint_nid_postfix(2.5f), 2.0f), "_FRint 2.5");
    Require(Same(_FRint_nid_postfix(2.6f), 3.0f), "_FRint 2.6");
    Require(Same(_FRint_nid_postfix(-0.3f), -0.0f), "_FRint -0.3");
    Require(Same(_FRint_nid_postfix(-0.7f), -1.0f), "_FRint -0.7");
    Require(Same(_FRint_nid_postfix(-1.4f), -1.0f), "_FRint -1.4");
    Require(Same(_FRint_nid_postfix(-1.5f), -2.0f), "_FRint -1.5");
    Require(Same(_FRint_nid_postfix(-2.5f), -2.0f), "_FRint -2.5");
    Require(Same(_FRint_nid_postfix(1e7f), 1e7f), "_FRint 1e7");
    Require(Same(_FRint_nid_postfix(infinity), infinity), "_FRint +inf");
    Require(Same(_FRint_nid_postfix(-infinity), -infinity), "_FRint -inf");
    Require(std::isnan(_FRint_nid_postfix(nan)), "_FRint nan");

    const float values[] = {0.1f, 0.49f, 0.5f, 0.51f, 1.49f, 1.5f, 1.51f, 2.5f, 3.5f, -0.49f, -0.5f, -0.51f, -1.5f, -2.5f};
    for (float v : values) {
        Require(Same(_FRint_nid_postfix(v), rintf_nid_postfix(v)), "_FRint matches rintf");
    }
}

void TestLongDouble() {
    const long double infinity = std::numeric_limits<long double>::infinity();
    const long double nan = std::numeric_limits<long double>::quiet_NaN();

    Require(Same(_LRint_nid_postfix(0.0L), 0.0L), "_LRint 0.0");
    Require(Same(_LRint_nid_postfix(-0.0L), -0.0L), "_LRint -0.0");
    Require(Same(_LRint_nid_postfix(1.0L), 1.0L), "_LRint 1.0");
    Require(Same(_LRint_nid_postfix(-1.0L), -1.0L), "_LRint -1.0");
    Require(Same(_LRint_nid_postfix(0.3L), 0.0L), "_LRint 0.3");
    Require(Same(_LRint_nid_postfix(0.7L), 1.0L), "_LRint 0.7");
    Require(Same(_LRint_nid_postfix(1.4L), 1.0L), "_LRint 1.4");
    Require(Same(_LRint_nid_postfix(1.5L), 2.0L), "_LRint 1.5");
    Require(Same(_LRint_nid_postfix(2.5L), 2.0L), "_LRint 2.5");
    Require(Same(_LRint_nid_postfix(2.6L), 3.0L), "_LRint 2.6");
    Require(Same(_LRint_nid_postfix(-0.3L), -0.0L), "_LRint -0.3");
    Require(Same(_LRint_nid_postfix(-0.7L), -1.0L), "_LRint -0.7");
    Require(Same(_LRint_nid_postfix(-1.4L), -1.0L), "_LRint -1.4");
    Require(Same(_LRint_nid_postfix(-1.5L), -2.0L), "_LRint -1.5");
    Require(Same(_LRint_nid_postfix(-2.5L), -2.0L), "_LRint -2.5");
    Require(Same(_LRint_nid_postfix(1e18L), 1e18L), "_LRint 1e18");
    Require(Same(_LRint_nid_postfix(infinity), infinity), "_LRint +inf");
    Require(Same(_LRint_nid_postfix(-infinity), -infinity), "_LRint -inf");
    Require(std::isnan(_LRint_nid_postfix(nan)), "_LRint nan");

    const long double values[] = {0.1L, 0.49L, 0.5L, 0.51L, 1.49L, 1.5L, 1.51L, 2.5L, 3.5L, -0.49L, -0.5L, -0.51L, -1.5L, -2.5L};
    for (long double v : values) {
        Require(Same(_LRint_nid_postfix(v), rintl_nid_postfix(v)), "_LRint matches rintl");
    }
}

}

int main() {
    TestDouble();
    TestFloat();
    TestLongDouble();
    return 0;
}
