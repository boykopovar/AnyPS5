#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

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

    storage.exponent = 5;
    Require(_LPow_nid_postfix(7.0L, 1.0L, &storage.exponent) == 7.0L, "_LPow 7^1 returns x");
    Require(storage.exponent == 5, "_LPow 7^1 leaves exponent untouched");

    storage.exponent = 9;
    Require(_LPow_nid_postfix(-3.0L, 1.0L, &storage.exponent) == -3.0L, "_LPow (-3)^1 returns x");
    Require(storage.exponent == 9, "_LPow (-3)^1 leaves exponent untouched");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(2.0L, 10.0L, &storage.exponent) == 1.0L, "_LPow 2^10 result");
    Require(storage.exponent == 10, "_LPow 2^10 exponent");

    storage.exponent = 999;
    const long double r3 = _LPow_nid_postfix(3.0L, 0.5L, &storage.exponent);
    Require(std::fabs(std::ldexp(r3, storage.exponent) - 1.7320508075688772L) < 1e-12L, "_LPow 3^0.5 reconstructed");
    Require(storage.exponent == 1, "_LPow 3^0.5 exponent");
    Require(std::fabs(r3 - 0.8660254037844386L) < 1e-12L, "_LPow 3^0.5 result");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(3.0L, 1.0L, &storage.exponent) == 3.0L, "_LPow 3^1 returns x");
    Require(storage.exponent == 999, "_LPow 3^1 leaves exponent untouched");

    storage.exponent = 999;
    const long double r3_2 = _LPow_nid_postfix(3.0L, 2.0L, &storage.exponent);
    Require(std::fabs(r3_2 - 1.125L) < 1e-12L, "_LPow 3^2 result in [1, 2)");
    Require(storage.exponent == 3, "_LPow 3^2 exponent");
    Require(std::ldexp(r3_2, storage.exponent) == 9.0L, "_LPow 3^2 reconstructed");

    storage.exponent = 999;
    const long double nearOneX = 1.0L + std::ldexp(1.0L, -63);
    const long double nearOneY = std::ldexp(1.0L, 63);
    const long double nearOneR = _LPow_nid_postfix(nearOneX, nearOneY, &storage.exponent);
    const long double reconstructed = std::ldexp(nearOneR, storage.exponent);
    Require(std::fabs(reconstructed - 2.7182818284590452L) / 2.7182818284590452L < 1e-12L, "_LPow near-one cancellation 63");
    Require(nearOneR >= 1.0L && nearOneR < 2.0L, "_LPow near-one mantissa in [1, 2)");

    for (int k : {40, 60}) {
        storage.exponent = 999;
        const long double xk = 1.0L + std::ldexp(1.0L, -k);
        const long double yk = std::ldexp(1.0L, k);
        const long double rk = _LPow_nid_postfix(xk, yk, &storage.exponent);
        const long double recK = std::ldexp(rk, storage.exponent);
        Require(std::fabs(recK - 2.7182818284590452L) / 2.7182818284590452L < 1e-8L, "_LPow near-one family");
        Require(rk >= 1.0L && rk < 2.0L, "_LPow family mantissa in [1, 2)");
    }

    storage.exponent = 999;
    const long double r10 = _LPow_nid_postfix(10.0L, -3.0L, &storage.exponent);
    Require(std::fabs(r10 - 1.024L) < 1e-12L, "_LPow 10^-3 result");
    Require(storage.exponent == -10, "_LPow 10^-3 exponent");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(2.0L, 20000.0L, &storage.exponent) == 1.0L, "_LPow 2^20000 result");
    Require(storage.exponent == 20000, "_LPow 2^20000 exponent");

    storage.exponent = 999;
    const long double r5000 = _LPow_nid_postfix(10.0L, 5000.0L, &storage.exponent);
    Require(std::fabs(r5000 - 1.558841707209611L) < 1e-10L, "_LPow 10^5000 result");
    Require(storage.exponent == 16609, "_LPow 10^5000 exponent");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(0.5L, 2.0L, &storage.exponent) == 1.0L, "_LPow 0.5^2 result");
    Require(storage.exponent == -2, "_LPow 0.5^2 exponent");

    const long double nanVal = std::numeric_limits<long double>::quiet_NaN();
    const long double infVal = std::numeric_limits<long double>::infinity();

    storage.exponent = 999;
    Require(_LPow_nid_postfix(0.0L, 5.0L, &storage.exponent) == 0.0L, "_LPow 0^5");
    Require(storage.exponent == 0, "_LPow 0^5 exponent");

    storage.exponent = 999;
    Require(Same(_LPow_nid_postfix(-0.0L, 3.0L, &storage.exponent), -0.0L), "_LPow (-0)^3 returns -0");
    Require(storage.exponent == 0, "_LPow (-0)^3 exponent");

    storage.exponent = 999;
    Require(Same(_LPow_nid_postfix(-0.0L, 2.0L, &storage.exponent), 0.0L), "_LPow (-0)^2 returns +0");
    Require(storage.exponent == 0, "_LPow (-0)^2 exponent");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(_LPow_nid_postfix(0.0L, -5.0L, &storage.exponent) == infVal, "_LPow 0^-5");
    Require(storage.exponent == 0, "_LPow 0^-5 exponent");
    Require(*__error_nid_postfix() == 34, "_LPow 0^-5 errno 34");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(Same(_LPow_nid_postfix(-0.0L, -3.0L, &storage.exponent), -infVal), "_LPow (-0)^-3 returns -inf");
    Require(storage.exponent == 0, "_LPow (-0)^-3 exponent");
    Require(*__error_nid_postfix() == 34, "_LPow (-0)^-3 errno 34");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(Same(_LPow_nid_postfix(-0.0L, -2.0L, &storage.exponent), infVal), "_LPow (-0)^-2 returns +inf");
    Require(storage.exponent == 0, "_LPow (-0)^-2 exponent");
    Require(*__error_nid_postfix() == 34, "_LPow (-0)^-2 errno 34");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(0.0L, 0.0L, &storage.exponent) == 1.0L, "_LPow 0^0");
    Require(storage.exponent == 0, "_LPow 0^0 exponent");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(5.0L, 0.0L, &storage.exponent) == 1.0L, "_LPow 5^0");
    Require(storage.exponent == 0, "_LPow 5^0 exponent");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(1.0L, 1e300L, &storage.exponent) == 1.0L, "_LPow 1^1e300");
    Require(storage.exponent == 0, "_LPow 1^1e300 exponent");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(1.0L, nanVal, &storage.exponent) == 1.0L, "_LPow 1^nan");
    Require(storage.exponent == 0, "_LPow 1^nan exponent");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(infVal, 2.0L, &storage.exponent) == infVal, "_LPow inf^2");
    Require(storage.exponent == 0, "_LPow inf^2 exponent");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(infVal, -2.0L, &storage.exponent) == 0.0L, "_LPow inf^-2");
    Require(storage.exponent == 0, "_LPow inf^-2 exponent");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(-infVal, 3.0L, &storage.exponent) == -infVal, "_LPow (-inf)^3");
    Require(storage.exponent == 0, "_LPow (-inf)^3 exponent");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(_LPow_nid_postfix(-infVal, 0.5L, &storage.exponent) == infVal, "_LPow (-inf)^0.5");
    Require(storage.exponent == 0, "_LPow (-inf)^0.5 exponent");
    Require(*__error_nid_postfix() == 0, "_LPow (-inf)^0.5 errno 0");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(_LPow_nid_postfix(-infVal, -0.5L, &storage.exponent) == 0.0L, "_LPow (-inf)^-0.5");
    Require(storage.exponent == 0, "_LPow (-inf)^-0.5 exponent");
    Require(*__error_nid_postfix() == 0, "_LPow (-inf)^-0.5 errno 0");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(2.0L, 32767.0L, &storage.exponent) == 1.0L, "_LPow 2^32767");
    Require(storage.exponent == 32767, "_LPow 2^32767 exponent");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(_LPow_nid_postfix(2.0L, 32768.0L, &storage.exponent) == 1.0L, "_LPow 2^32768");
    Require(storage.exponent == -32768, "_LPow 2^32768 exponent");
    Require(*__error_nid_postfix() == 0, "_LPow 2^32768 errno 0");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(_LPow_nid_postfix(2.0L, 32769.0L, &storage.exponent) == infVal, "_LPow 2^32769");
    Require(storage.exponent == 0, "_LPow 2^32769 exponent");
    Require(*__error_nid_postfix() == 34, "_LPow 2^32769 errno 34");

    storage.exponent = 999;
    Require(_LPow_nid_postfix(2.0L, -32768.0L, &storage.exponent) == 1.0L, "_LPow 2^-32768");
    Require(storage.exponent == -32768, "_LPow 2^-32768 exponent");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(_LPow_nid_postfix(2.0L, -32769.0L, &storage.exponent) == 0.0L, "_LPow 2^-32769");
    Require(storage.exponent == 0, "_LPow 2^-32769 exponent");
    Require(*__error_nid_postfix() == 34, "_LPow 2^-32769 errno 34");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(Same(_LPow_nid_postfix(-2.0L, 32769.0L, &storage.exponent), -infVal), "_LPow (-2)^32769");
    Require(storage.exponent == 0, "_LPow (-2)^32769 exponent");
    Require(*__error_nid_postfix() == 34, "_LPow (-2)^32769 errno 34");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(Same(_LPow_nid_postfix(-2.0L, 32768.0L, &storage.exponent), 1.0L), "_LPow (-2)^32768");
    Require(storage.exponent == -32768, "_LPow (-2)^32768 exponent");
    Require(*__error_nid_postfix() == 0, "_LPow (-2)^32768 errno 0");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(Same(_LPow_nid_postfix(-2.0L, -32769.0L, &storage.exponent), -0.0L), "_LPow (-2)^-32769");
    Require(storage.exponent == 0, "_LPow (-2)^-32769 exponent");
    Require(*__error_nid_postfix() == 34, "_LPow (-2)^-32769 errno 34");

    storage.exponent = 999;
    Require(Same(_LPow_nid_postfix(-2.0L, -32768.0L, &storage.exponent), 1.0L), "_LPow (-2)^-32768");
    Require(storage.exponent == -32768, "_LPow (-2)^-32768 exponent");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(std::isnan(_LPow_nid_postfix(nanVal, 2.0L, &storage.exponent)), "_LPow nan^2");
    Require(storage.exponent == 0, "_LPow nan^2 exponent");
    Require(*__error_nid_postfix() == 0, "_LPow nan^2 errno 0");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(std::isnan(_LPow_nid_postfix(-2.0L, 0.5L, &storage.exponent)), "_LPow (-2)^0.5");
    Require(storage.exponent == 0, "_LPow (-2)^0.5 exponent");
    Require(*__error_nid_postfix() == 33, "_LPow (-2)^0.5 errno 33");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(_LPow_nid_postfix(1e300L, 1e300L, &storage.exponent) == infVal, "_LPow 1e300^1e300");
    Require(storage.exponent == 0, "_LPow 1e300^1e300 exponent");
    Require(*__error_nid_postfix() == 34, "_LPow 1e300^1e300 errno 34");

    *__error_nid_postfix() = 0;
    storage.exponent = 999;
    Require(_LPow_nid_postfix(0.999L, 1e30L, &storage.exponent) == 0.0L, "_LPow 0.999^1e30");
    Require(storage.exponent == 0, "_LPow 0.999^1e30 exponent");
    Require(*__error_nid_postfix() == 34, "_LPow 0.999^1e30 errno 34");
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
