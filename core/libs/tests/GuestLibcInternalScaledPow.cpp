#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal scaled pow: %s\n", message);
    std::abort();
}

template <typename T>
bool Same(T left, T right) {
    if (std::isnan(left) && std::isnan(right)) return true;
    return left == right && std::signbit(left) == std::signbit(right);
}

void TestNullExponent() {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double pairs[][2] = {
        {2.0, 10.0}, {2.0, 0.5}, {-8.0, 1.0 / 3.0}, {-2.0, 3.0}, {-0.0, -1.0}, {0.0, -2.0}, {10.0, 400.0}, {10.0, -400.0},
        {1.0, nan}, {nan, 0.0}, {-1.0, infinity}, {0.5, -infinity}, {infinity, -0.5}, {-infinity, 3.0}, {1.0000001, 1e9}, {3.7, -2.25}};
    for (const auto& pair : pairs) {
        Require(Same(_Pow_nid_postfix(pair[0], pair[1], nullptr), pow_nid_postfix(pair[0], pair[1])), "_Pow with a null exponent is pow");
        const float fx = static_cast<float>(pair[0]);
        const float fy = static_cast<float>(pair[1]);
        Require(Same(_FPow_nid_postfix(fx, fy, nullptr), powf_nid_postfix(fx, fy)), "_FPow with a null exponent is powf");
        const long double lx = pair[0];
        const long double ly = pair[1];
        Require(Same(_LPow_nid_postfix(lx, ly, nullptr), powl_nid_postfix(lx, ly)), "_LPow with a null exponent is powl");
    }
    Require(_Pow_nid_postfix(2.0, 10.0, nullptr) == 1024.0, "_Pow 2 10");
    Require(_FPow_nid_postfix(3.0f, 2.0f, nullptr) == 9.0f, "_FPow 3 2");
    Require(_LPow_nid_postfix(2.0L, -2.0L, nullptr) == 0.25L, "_LPow 2 -2");
}

}

int main() {
    TestNullExponent();
    return 0;
}
