#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal quadrant trig: %s\n", message);
    std::abort();
}

template <typename T>
bool Near(T value, T expected, T ulps) {
    return std::fabs(value - expected) <= ulps * std::numeric_limits<T>::epsilon() * std::fmax(std::fabs(expected), T(1));
}

void TestAtanDouble() {
    const double pi = 3.14159265358979323846;
    Require(_Atan_nid_postfix(0.0, 0x2) == pi / 2, "_Atan 0 0x2 is pi/2");
    Require(_Atan_nid_postfix(0.0, 0x6) == pi, "_Atan 0 0x6 is pi");
    Require(_Atan_nid_postfix(0.0, 0xe) == -pi, "_Atan 0 0xe is -pi");
    const double zero = _Atan_nid_postfix(0.0, 0x8);
    Require(zero == 0.0 && std::signbit(zero), "_Atan 0 0x8 is -0");
    Require(Near(_Atan_nid_postfix(1.0, 0x0), pi / 4, 1.0), "_Atan 1 0 is pi/4");
    const double samples[] = {1e-300, 1e-8, 0.1, 0.25, 0.5, 0.75, 0.9999, 1.0};
    for (double z : samples) {
        const double angle = atan_nid_postfix(z);
        Require(_Atan_nid_postfix(z, 0x0) == angle, "_Atan 0 is atan");
        Require(_Atan_nid_postfix(z, 0x8) == -angle, "_Atan 0x8 negates atan");
        Require(Near(_Atan_nid_postfix(z, 0x2), std::atan2(1.0, z), 2.0), "_Atan 0x2 is pi/2 - atan, the atan of the inverted argument");
        Require(Near(_Atan_nid_postfix(z, 0x4), std::atan2(1.0, -z), 2.0), "_Atan 0x4 is pi/2 + atan");
        Require(Near(_Atan_nid_postfix(z, 0x6), std::atan2(z, -1.0), 2.0), "_Atan 0x6 is pi - atan");
        Require(Near(_Atan_nid_postfix(z, 0xe), std::atan2(-z, -1.0), 2.0), "_Atan 0xe is -(pi - atan)");
        Require(Near(_Atan_nid_postfix(z, 0xa), -std::atan2(1.0, z), 2.0), "_Atan 0xa is -(pi/2 - atan)");
    }
    const double cosines[] = {0.0, 0.2, 0.5, 0.7};
    for (double x : cosines) {
        const double z = x / std::sqrt((1.0 - x) * (1.0 + x));
        Require(Near(_Atan_nid_postfix(z, 0x2), std::acos(x), 4.0), "_Atan 0x2 gives acos as Dinkumware's acos calls it");
        Require(Near(_Atan_nid_postfix(z, 0x4), std::acos(-x), 4.0), "_Atan 0x4 gives acos of a negative argument");
    }
}

void TestAtanOtherWidths() {
    const float samples[] = {0.0f, 0.1f, 0.5f, 1.0f};
    for (float z : samples) {
        Require(_FAtan_nid_postfix(z, 0x0) == static_cast<float>(atan_nid_postfix(z)), "_FAtan 0 is atan rounded to float");
        Require(Near(_FAtan_nid_postfix(z, 0x6), std::atan2(z, -1.0f), 1.0f), "_FAtan 0x6 is pi - atan");
        Require(Near(_FAtan_nid_postfix(z, 0xa), -std::atan2(1.0f, z), 1.0f), "_FAtan 0xa is -(pi/2 - atan)");
        const long double wide = z;
        Require(_LAtan_nid_postfix(wide, 0x0) == atanl_nid_postfix(wide), "_LAtan 0 is atanl");
        Require(Near(_LAtan_nid_postfix(wide, 0x4), std::atan2(1.0L, -wide), 4.0L), "_LAtan 0x4 is pi/2 + atan");
        Require(Near(_LAtan_nid_postfix(wide, 0xe), std::atan2(-wide, -1.0L), 4.0L), "_LAtan 0xe is -(pi - atan)");
    }
}

void TestTan() {
    const double samples[] = {0.0, -0.0, 0.5, -1.25, 3.0, 1e10, 1e-300};
    for (double x : samples) {
        const double value = _Tan_nid_postfix(x, 0);
        const double expected = tan_nid_postfix(x);
        Require(value == expected && std::signbit(value) == std::signbit(expected), "_Tan 0 is tan");
        Require(_FTan_nid_postfix(static_cast<float>(x), 0) == tanf_nid_postfix(static_cast<float>(x)), "_FTan 0 is tanf");
        Require(_LTan_nid_postfix(x, 0) == tanl_nid_postfix(x), "_LTan 0 is tanl");
    }
    Require(std::isnan(_Tan_nid_postfix(std::numeric_limits<double>::quiet_NaN(), 0)), "_Tan NaN is NaN");
    Require(std::isnan(_Tan_nid_postfix(std::numeric_limits<double>::infinity(), 0)), "_Tan infinity is NaN");
}

}

int main() {
    TestAtanDouble();
    TestAtanOtherWidths();
    TestTan();
    return 0;
}
