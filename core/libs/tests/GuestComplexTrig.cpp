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
