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
    std::fprintf(stderr, "libc internal math: %s\n", message);
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

void TestNonNullExponent() {
    struct Storage {
        short exponent = 1234;
        short marker = 5678;
    } storage;

    const double resultD = _Pow_nid_postfix(2.0, 10.0, &storage.exponent);
    Require(resultD == 1024.0, "_Pow 2^10 with exponent pointer");
    Require(storage.exponent == 1234, "_Pow exponent unchanged");
    Require(storage.marker == 5678, "_Pow marker unchanged");

    storage.exponent = -7;
    storage.marker = 99;
    const float resultF = _FPow_nid_postfix(3.0f, 2.0f, &storage.exponent);
    Require(resultF == 9.0f, "_FPow 3^2 with exponent pointer");
    Require(storage.exponent == -7, "_FPow exponent unchanged");
    Require(storage.marker == 99, "_FPow marker unchanged");

    short lpowExp = 0;
    bool lpowThrew = false;
    try {
        _LPow_nid_postfix(2.0L, 10.0L, &lpowExp);
    } catch (const std::runtime_error&) {
        lpowThrew = true;
    }
    Require(lpowThrew, "_LPow non-null exponent throws NotImplemented");
}

template<typename T, typename Evaluate>
void TestErfcTail(Evaluate evaluate) {
    constexpr auto tailStart = static_cast<T>(1.523L);
    constexpr auto erfcOfTwo = static_cast<T>(0.00467773498104726583793074363L);
    constexpr auto tolerance = 8 * std::numeric_limits<T>::epsilon() * erfcOfTwo;
    const T atStart = evaluate(tailStart);
    const T atTwo = evaluate(static_cast<T>(2));
    Require(std::abs(atTwo - erfcOfTwo) <= tolerance, "erfc tail agrees with erfc(2)");
    Require(atStart > atTwo && atStart < static_cast<T>(0.04L), "erfc tail includes its lower boundary");
    Require(evaluate(static_cast<T>(150)) == 0, "erfc tail underflows to zero for a large finite input");

    const T outside[] = {std::nextafter(tailStart, static_cast<T>(0)), static_cast<T>(-2),
                         std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN()};
    for (T x : outside) {
        bool rejected = false;
        try {
            evaluate(x);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected, "inputs outside the verified erfc tail must throw");
    }
}

}

int main() {
    TestNullExponent();
    TestNonNullExponent();
    TestErfcTail<double>(_Erfc_nid_postfix);
    TestErfcTail<float>(_FErfc_nid_postfix);
    TestErfcTail<long double>(_LErfc_nid_postfix);
    return 0;
}
