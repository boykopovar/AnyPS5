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
    Require(Near(cpow_nid_postfix(Make(2.0, 0.0), Make(2.0, 0.0)), 4.0, 0.0));
    Require(Near(cpow_nid_postfix(i, Make(2.0, 0.0)), -1.0, 0.0));
    Require(Near(csqrt_nid_postfix(Make(-4.0, -0.0)), 0.0, -2.0));
    const __complex__ double point = Make(0.5, 0.25);
    Require(Near(casinh_nid_postfix(point), 0.49267568342077056, 0.2243284526346675));
    Require(Near(catanh_nid_postfix(point), 0.5003700000525311, 0.3143981432077165));
    const __complex__ double negative = Make(-0.5, -0.75);
    Require(Near(casinh_nid_postfix(negative), -0.6063349998873513, -0.6822039655834323));
    Require(Near(catanh_nid_postfix(negative), -0.3104282830771958, -0.7232206661240675));

    Require(Near(ctanh_nid_postfix(one + i), 1.0839233273386946, 0.2717525853195118));
    Require(Near(ctanh_nid_postfix(Make(-3.0, -2.0)), -1.00323862735361, 0.003764025641504249));
    for (const double x : {15.0, 21.5, 22.0, 22.5, 40.0, 300.0}) {
        for (const double y : {1.0, -2.5}) {
            const double denominator = std::cosh(2 * x) + std::cos(2 * y);
            const __complex__ double value = ctanh_nid_postfix(Make(x, y));
            Require(Close(__real__ value, std::sinh(2 * x) / denominator));
            Require(Close(__imag__ value, std::sin(2 * y) / denominator));
            const __complex__ double mirrored = ctan_nid_postfix(Make(y, x));
            Require(Close(__real__ mirrored, std::sin(2 * y) / denominator));
            Require(Close(__imag__ mirrored, std::sinh(2 * x) / denominator));
        }
    }
    for (const double x : {711.0, 1000.0, 1e5, 1e300}) {
        const __complex__ double positive = ctanh_nid_postfix(Make(x, 1.0));
        Require(__real__ positive == 1.0 && __imag__ positive == 0.0 && !std::signbit(__imag__ positive));
        const __complex__ double negativeReal = ctanh_nid_postfix(Make(-x, -1.0));
        Require(__real__ negativeReal == -1.0 && __imag__ negativeReal == 0.0 && std::signbit(__imag__ negativeReal));
        const __complex__ double tangent = ctan_nid_postfix(Make(1.0, x));
        Require(__real__ tangent == 0.0 && __imag__ tangent == 1.0);
    }
    const double hugeImaginary = 1e30;
    Require(Near(ctanh_nid_postfix(Make(1.0, hugeImaginary)), 0.7616220083332145, -0.003918950207184144));
    Require(Near(ctan_nid_postfix(Make(hugeImaginary, 1.0)), -0.003918950207184144, 0.7616220083332145));
    const __complex__ double hugeBoth = ctanh_nid_postfix(Make(30.0, hugeImaginary));
    Require(__real__ hugeBoth == 1.0 && std::fabs(__imag__ hugeBoth / -3.2683020199668726e-28 - 1) < 1e-12);
    const double infinity = HUGE_VAL;
    const double nan = std::nan("");
    const __complex__ double atInfinity = ctanh_nid_postfix(Make(-infinity, 2.0));
    Require(__real__ atInfinity == -1.0 && __imag__ atInfinity == 0.0 && std::signbit(__imag__ atInfinity));
    const __complex__ double bothInfinite = ctanh_nid_postfix(Make(infinity, infinity));
    Require(__real__ bothInfinite == 1.0 && __imag__ bothInfinite == 0.0);
    const __complex__ double nanReal = ctanh_nid_postfix(Make(nan, -0.0));
    Require(std::isnan(__real__ nanReal) && __imag__ nanReal == 0.0 && std::signbit(__imag__ nanReal));
    const __complex__ double infiniteImaginary = ctanh_nid_postfix(Make(1.0, infinity));
    Require(std::isnan(__real__ infiniteImaginary) && std::isnan(__imag__ infiniteImaginary));

    Require(Near(catanh_nid_postfix(Make(3.0, 4.0)), 0.1175009073114339, 1.4099210495965755));
    Require(Near(catanh_nid_postfix(Make(-0.25, 2.0)), -0.04954423214593717, 1.1121113339878292));
    Require(Near(catan_nid_postfix(Make(5.0, -0.5)), 1.375233603310382, -0.01905684134697106));
    struct Large {
        double x;
        double y;
        double real;
    };
    for (const Large& large : {Large{1e20, 1.0, 1e-20}, Large{-1e30, -2.0, -1e-30}, Large{-8.048749835924716e+49, -1.0, -1.2424289739216509e-50},
                               Large{1e300, 1.0, 1e-300}, Large{1.0, 1e150, 1e-300}, Large{1e300, 1e300, 5e-301}}) {
        const __complex__ double hyperbolic = catanh_nid_postfix(Make(large.x, large.y));
        Require(Close(__real__ hyperbolic, large.real) && Close(__imag__ hyperbolic, std::copysign(pi / 2, large.y)));
        const __complex__ double circular = catan_nid_postfix(Make(large.y, large.x));
        Require(Close(__imag__ circular, large.real) && Close(__real__ circular, std::copysign(pi / 2, large.y)));
    }
    const __complex__ double tiny = Make(1e-12, -1e-12);
    const __complex__ double tinyResult = catanh_nid_postfix(tiny);
    Require(__real__ tinyResult == __real__ tiny && __imag__ tinyResult == __imag__ tiny);
    const __complex__ double nearOne = catanh_nid_postfix(Make(1.0, 1e-30));
    Require(Close(__real__ nearOne, (std::log(2.0) - std::log(1e-30)) / 2) && Close(__imag__ nearOne, pi / 4));
    const __complex__ double infiniteReal = catanh_nid_postfix(Make(infinity, 1.0));
    Require(__real__ infiniteReal == 0.0 && Close(__imag__ infiniteReal, pi / 2));
    const __complex__ double infiniteImaginaryArc = catanh_nid_postfix(Make(2.0, -infinity));
    Require(__real__ infiniteImaginaryArc == 0.0 && Close(__imag__ infiniteImaginaryArc, -pi / 2));
    const __complex__ double infiniteNan = catanh_nid_postfix(Make(infinity, nan));
    Require(__real__ infiniteNan == 0.0 && std::isnan(__imag__ infiniteNan));

    Require(Near(cacos_nid_postfix(Make(-0.5, 3.0)), 1.7278009215838388, -1.8301947623375088));
    Require(Near(casinh_nid_postfix(Make(2.0, -0.75)), 1.4925210319880218, -0.3267704027823274));
    Require(Near(cacosh_nid_postfix(Make(-3.0, 0.5)), 1.7789904938267687, 2.9669461885043438));
    struct Small {
        __complex__ double (APS5_VABI* function)(__complex__ double);
        double x;
        double y;
        double real;
        double imaginary;
    };
    for (const Small& small : {Small{cacos_nid_postfix, 1.0000000171256545, 8.90447606042488e-273, 4.811381373109857e-269, -0.00018507109121216418},
                               Small{cacosh_nid_postfix, -1.1473276323882705e-255, -1.5542024766188596e-77, 1.5542024766188596e-77, -1.5707963267948966},
                               Small{casinh_nid_postfix, 6.893083138767318e-156, 1.0, 2.6254681751579696e-78, 1.5707963267948966},
                               Small{casin_nid_postfix, 1.0, 4.595663159569067e-264, 1.5707963267948966, 2.143749789403854e-132},
                               Small{catan_nid_postfix, 1.3165213440503372e-34, -1.2240878432285997e+251, 1.5707963267948966, -8.169348348092768e-252}}) {
        const __complex__ double value = small.function(Make(small.x, small.y));
        Require(Close(__real__ value, small.real) && Close(__imag__ value, small.imaginary));
    }
    const __complex__ double tinyArcsine = casin_nid_postfix(Make(-0.0, 1e-9));
    Require(__real__ tinyArcsine == 0.0 && std::signbit(__real__ tinyArcsine) && __imag__ tinyArcsine == 1e-9);
    const double big = 1e300;
    const __complex__ double largeHyperbolic = casinh_nid_postfix(Make(big, -big));
    Require(Close(__real__ largeHyperbolic, std::log(2.0) + std::log(std::hypot(big, big))) && Close(__imag__ largeHyperbolic, -pi / 4));
    const __complex__ double largeCosine = cacos_nid_postfix(Make(0.0, big));
    Require(Close(__real__ largeCosine, pi / 2) && Close(__imag__ largeCosine, -(std::log(2.0) + std::log(big))));
    const __complex__ double largeSine = casin_nid_postfix(Make(-big, 1.0));
    Require(Close(__real__ largeSine, -pi / 2) && Close(__imag__ largeSine, std::log(2.0) + std::log(big)));
    const __complex__ double largeHyperbolicCosine = cacosh_nid_postfix(Make(-big, -big));
    Require(Close(__real__ largeHyperbolicCosine, std::log(2.0) + std::log(std::hypot(big, big))) && Close(__imag__ largeHyperbolicCosine, -3 * pi / 4));
    const __complex__ double cosineAtOne = cacos_nid_postfix(Make(1.0, -0.0));
    Require(__real__ cosineAtOne == 0.0 && __imag__ cosineAtOne == 0.0 && !std::signbit(__imag__ cosineAtOne));
    const __complex__ double cosineNan = cacos_nid_postfix(Make(infinity, nan));
    Require(std::isnan(__real__ cosineNan) && std::isinf(__imag__ cosineNan));
    const __complex__ double sineNan = casinh_nid_postfix(Make(nan, -infinity));
    Require(std::isinf(__real__ sineNan) && std::isnan(__imag__ sineNan));
    return 0;
}
