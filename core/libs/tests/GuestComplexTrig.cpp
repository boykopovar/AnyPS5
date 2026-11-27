#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cmath>
#include <cstdlib>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

__complex__ double Make(double real, double imaginary) {
    __complex__ double result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

bool Close(double value, double expected) {
    return std::fabs(value - expected) <= 1e-14 * std::fabs(expected);
}

bool Near(__complex__ double value, double real, double imaginary) {
    constexpr double tolerance = 1e-12;
    return std::fabs(__real__ value - real) < tolerance && std::fabs(__imag__ value - imaginary) < tolerance;
}

}

int main() {
    const double pi = std::acos(-1.0);
    const __complex__ double zero = Make(0.0, 0.0);
    const __complex__ double one = Make(1.0, 0.0);
    const __complex__ double i = Make(0.0, 1.0);

    Require(Near(cacos_nid_postfix(zero), pi / 2, 0.0));
    Require(Near(casin_nid_postfix(zero), 0.0, 0.0));
    Require(Near(catan_nid_postfix(zero), 0.0, 0.0));
    Require(Near(ccos_nid_postfix(zero), 1.0, 0.0));
    Require(Near(csin_nid_postfix(zero), 0.0, 0.0));
    Require(Near(ctan_nid_postfix(zero), 0.0, 0.0));
    Require(Near(ccosh_nid_postfix(zero), 1.0, 0.0));
    Require(Near(csinh_nid_postfix(zero), 0.0, 0.0));
    Require(Near(ctanh_nid_postfix(zero), 0.0, 0.0));
    Require(Near(cacosh_nid_postfix(one), 0.0, 0.0));
    Require(Near(cexp_nid_postfix(zero), 1.0, 0.0));
    Require(Near(clog_nid_postfix(one), 0.0, 0.0));
    Require(Near(csqrt_nid_postfix(Make(-4.0, 0.0)), 0.0, 2.0));
    Require(Near(cexp_nid_postfix(Make(0.0, pi)), -1.0, 0.0));
    Require(Near(csin_nid_postfix(i), 0.0, std::sinh(1.0)));
    Require(Near(clog_nid_postfix(Make(-1.0, 0.0)), 0.0, pi));
    struct Logarithm {
        double x;
        double y;
        double real;
        double imaginary;
    };
    for (const Logarithm& logarithm : {Logarithm{1.0, 2.140527919234565e-113, 2.290929886511328e-226, 2.140527919234565e-113},
                                       Logarithm{-4.4683417560330284e-126, 1.0, 9.983039024354164e-252, 1.5707963267948966},
                                       Logarithm{-4.1447581995199045e-28, -1.0, 8.58951026624374e-56, -1.5707963267948966},
                                       Logarithm{0.6, 0.8, 2.2204460492503132e-17, 0.9272952180016123},
                                       Logarithm{-0.28, 0.96, -2.6645352591003756e-17, 1.8545904360032246},
                                       Logarithm{0.5403023058681398, 0.8414709848078965, 2.4228383963033683e-17, 1.0},
                                       Logarithm{-0.8011436155469337, -0.5984721441039564, -4.836084537015055e-17, -2.5},
                                       Logarithm{0.9999999999999999, 1e-08, -6.102230246251566e-17, 1e-08},
                                       Logarithm{0.93030200452232636, 0.36679446566520901, -1.6955763147152913e-10, 0.3755609779960376},
                                       Logarithm{1e308, 1e308, 709.542782232446, 0.7853981633974483},
                                       Logarithm{1.7976931348623157e308, 1.7976931348623157e308, 710.1292864836639, 0.7853981633974483},
                                       Logarithm{4e-320, 3e-320, -735.2178029785398, 0.6435011087932844},
                                       Logarithm{1e-300, 2e-300, -689.9708089419967, 1.1071487177940904},
                                       Logarithm{3.0, 4.0, 1.6094379124341003, 0.9272952180016122}}) {
        const __complex__ double value = clog_nid_postfix(Make(logarithm.x, logarithm.y));
        Require(Close(__real__ value, logarithm.real) && Close(__imag__ value, logarithm.imaginary));
    }
    const __complex__ double logZero = clog_nid_postfix(Make(-0.0, 0.0));
    Require(std::isinf(__real__ logZero) && __real__ logZero < 0 && Close(__imag__ logZero, pi));
    const __complex__ double logInfiniteNan = clog_nid_postfix(Make(HUGE_VAL, std::nan("")));
    Require(std::isinf(__real__ logInfiniteNan) && __real__ logInfiniteNan > 0 && std::isnan(__imag__ logInfiniteNan));
    const __complex__ double logNanInfinite = clog_nid_postfix(Make(std::nan(""), -HUGE_VAL));
    Require(std::isinf(__real__ logNanInfinite) && __real__ logNanInfinite > 0 && std::isnan(__imag__ logNanInfinite));
    const __complex__ double logNegativeInfinite = clog_nid_postfix(Make(-HUGE_VAL, -2.0));
    Require(std::isinf(__real__ logNegativeInfinite) && Close(__imag__ logNegativeInfinite, -pi));
    Require(Near(cpow_nid_postfix(Make(2.0, 0.0), Make(2.0, 0.0)), 4.0, 0.0));
    Require(Near(cpow_nid_postfix(i, Make(2.0, 0.0)), -1.0, 0.0));
    Require(Near(csqrt_nid_postfix(Make(-4.0, -0.0)), 0.0, -2.0));
    const __complex__ double point = Make(0.5, 0.25);
    Require(Near(casinh_nid_postfix(point), 0.49267568342077056, 0.2243284526346675));
    Require(Near(catanh_nid_postfix(point), 0.5003700000525311, 0.3143981432077165));
    const __complex__ double negative = Make(-0.5, -0.75);
    Require(Near(casinh_nid_postfix(negative), -0.6063349998873513, -0.6822039655834323));
    Require(Near(catanh_nid_postfix(negative), -0.3104282830771958, -0.7232206661240675));
    return 0;
}
