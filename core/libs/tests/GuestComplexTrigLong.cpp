#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cfloat>
#include <cmath>
#include <cstdlib>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

__complex__ long double Make(long double real, long double imaginary) {
    __complex__ long double result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

bool Close(long double value, long double expected) {
    return std::fabs(value - expected) <= 64 * LDBL_EPSILON * std::fabs(expected);
}

bool CloseTo(long double value, long double expected, long double tolerance) {
    return std::fabs(value - expected) <= tolerance * std::fabs(expected);
}

bool Near(__complex__ long double value, long double real, long double imaginary) {
    constexpr long double tolerance = 1e-12L;
    return std::fabs(__real__ value - real) < tolerance && std::fabs(__imag__ value - imaginary) < tolerance;
}

}

int main() {
    const long double pi = std::acos(-1.0L);
    const __complex__ long double zero = Make(0.0L, 0.0L);
    const __complex__ long double one = Make(1.0L, 0.0L);
    const __complex__ long double i = Make(0.0L, 1.0L);

    Require(Near(cacosl_nid_postfix(zero), pi / 2, 0.0L));
    Require(Near(casinl_nid_postfix(zero), 0.0L, 0.0L));
    Require(Near(catanl_nid_postfix(zero), 0.0L, 0.0L));
    Require(Near(ccosl_nid_postfix(zero), 1.0L, 0.0L));
    Require(Near(csinl_nid_postfix(zero), 0.0L, 0.0L));
    Require(Near(ctanl_nid_postfix(zero), 0.0L, 0.0L));
    Require(Near(ccoshl_nid_postfix(zero), 1.0L, 0.0L));
    Require(Near(csinhl_nid_postfix(zero), 0.0L, 0.0L));
    Require(Near(ctanhl_nid_postfix(zero), 0.0L, 0.0L));
    Require(Near(cacoshl_nid_postfix(one), 0.0L, 0.0L));
    Require(Near(cexpl_nid_postfix(zero), 1.0L, 0.0L));
    Require(Near(clogl_nid_postfix(one), 0.0L, 0.0L));
    Require(Near(csqrtl_nid_postfix(Make(-4.0L, 0.0L)), 0.0L, 2.0L));
    Require(Near(cexpl_nid_postfix(Make(0.0L, pi)), -1.0L, 0.0L));
    Require(Near(csinl_nid_postfix(i), 0.0L, std::sinh(1.0L)));
    Require(Near(clogl_nid_postfix(Make(-1.0L, 0.0L)), 0.0L, pi));
    Require(Near(cpowl_nid_postfix(Make(2.0L, 0.0L), Make(2.0L, 0.0L)), 4.0L, 0.0L));
    Require(Near(cpowl_nid_postfix(i, Make(2.0L, 0.0L)), -1.0L, 0.0L));
    Require(Near(csqrtl_nid_postfix(Make(-4.0L, -0.0L)), 0.0L, -2.0L));
    const __complex__ long double point = Make(0.5L, 0.25L);
    Require(Near(cacosl_nid_postfix(point), 1.069187473519396L, -0.28139605624529274L));
    Require(Near(casinl_nid_postfix(point), 0.5016088532755008L, 0.28139605624529274L));
    Require(Near(catanl_nid_postfix(point), 0.4842544903299662L, 0.20058661813123432L));
    Require(Near(ccosl_nid_postfix(point), 0.9051501505596068L, -0.12110879604381165L));
    Require(Near(csinl_nid_postfix(point), 0.494485780933195L, 0.22168816414957482L));
    Require(Near(ctanl_nid_postfix(point), 0.504500702698564L, 0.31242069250258875L));
    Require(Near(ccoshl_nid_postfix(point), 1.0925708047319176L, 0.12892104172809826L));
    Require(Near(csinhl_nid_postfix(point), 0.504895714387995L, 0.2789791283502615L));
    Require(Near(ctanhl_nid_postfix(point), 0.4854872810241353L, 0.19805544995134958L));
    Require(Near(cacoshl_nid_postfix(point), 0.28139605624529274L, 1.069187473519396L));
    Require(Near(cexpl_nid_postfix(point), 1.5974665191199127L, 0.4079001700783598L));
    Require(Near(clogl_nid_postfix(point), -0.5815754049028404L, 0.4636476090008061L));
    Require(Near(csqrtl_nid_postfix(point), 0.7276733451126774L, 0.17178037486125622L));
    Require(Near(cpowl_nid_postfix(point, Make(2.0L, 0.0L)), 0.1875L, 0.25L));
    Require(Near(casinhl_nid_postfix(point), 0.49267568342077056L, 0.2243284526346675L));
    Require(Near(catanhl_nid_postfix(point), 0.5003700000525311L, 0.3143981432077165L));
    const __complex__ long double negative = Make(-0.5L, -0.75L);
    Require(Near(casinhl_nid_postfix(negative), -0.6063349998873513L, -0.6822039655834323L));
    Require(Near(catanhl_nid_postfix(negative), -0.3104282830771958L, -0.7232206661240675L));

    Require(Near(ctanhl_nid_postfix(one + i), 1.0839233273386946L, 0.2717525853195118L));
    Require(Near(ctanhl_nid_postfix(Make(-3.0L, -2.0L)), -1.00323862735361L, 0.003764025641504249L));
    for (const long double x : {20.0L, 25.5L, 26.0L, 26.5L, 40.0L, 2000.0L}) {
        for (const long double y : {1.0L, -2.5L}) {
            const long double denominator = std::cosh(2 * x) + std::cos(2 * y);
            const long double real = std::tanh(2 * x) / (1 + std::cos(2 * y) / std::cosh(2 * x));
            const __complex__ long double value = ctanhl_nid_postfix(Make(x, y));
            Require(Close(__real__ value, real));
            Require(Close(__imag__ value, std::sin(2 * y) / denominator));
            const __complex__ long double mirrored = ctanl_nid_postfix(Make(y, x));
            Require(Close(__real__ mirrored, std::sin(2 * y) / denominator));
            Require(Close(__imag__ mirrored, real));
        }
    }
    for (const long double x : {6000.0L, 11357.0L, 1e5L, LDBL_MAX}) {
        const __complex__ long double positive = ctanhl_nid_postfix(Make(x, 1.0L));
        Require(__real__ positive == 1.0L && __imag__ positive == 0.0L && !std::signbit(__imag__ positive));
        const __complex__ long double negativeReal = ctanhl_nid_postfix(Make(-x, -1.0L));
        Require(__real__ negativeReal == -1.0L && __imag__ negativeReal == 0.0L && std::signbit(__imag__ negativeReal));
        const __complex__ long double tangent = ctanl_nid_postfix(Make(1.0L, x));
        Require(__real__ tangent == 0.0L && __imag__ tangent == 1.0L);
    }
    const long double hugeImaginary = 1e30;
    Require(Near(ctanhl_nid_postfix(Make(1.0L, hugeImaginary)), 0.7616220083332145L, -0.003918950207184144L));
    Require(Near(ctanl_nid_postfix(Make(hugeImaginary, 1.0L)), -0.003918950207184144L, 0.7616220083332145L));
    const __complex__ long double hugeBoth = ctanhl_nid_postfix(Make(30.0L, hugeImaginary));
    Require(std::fabs(__real__ hugeBoth - 1) < 1e-12L && std::fabs(__imag__ hugeBoth / -3.2683020199668726e-28L - 1) < 1e-12L);
    const long double infinity = HUGE_VALL;
    const long double nan = std::nanl("");
    const __complex__ long double atInfinity = ctanhl_nid_postfix(Make(-infinity, 2.0L));
    Require(__real__ atInfinity == -1.0L && __imag__ atInfinity == 0.0L && std::signbit(__imag__ atInfinity));
    const __complex__ long double bothInfinite = ctanhl_nid_postfix(Make(infinity, infinity));
    Require(__real__ bothInfinite == 1.0L && __imag__ bothInfinite == 0.0L);
    const __complex__ long double nanReal = ctanhl_nid_postfix(Make(nan, -0.0L));
    Require(std::isnan(__real__ nanReal) && __imag__ nanReal == 0.0L && std::signbit(__imag__ nanReal));
    const __complex__ long double infiniteImaginary = ctanhl_nid_postfix(Make(1.0L, infinity));
    Require(std::isnan(__real__ infiniteImaginary) && std::isnan(__imag__ infiniteImaginary));

    Require(Near(catanhl_nid_postfix(Make(3.0L, 4.0L)), 0.1175009073114339L, 1.4099210495965755L));
    Require(Near(catanhl_nid_postfix(Make(-0.25L, 2.0L)), -0.04954423214593717L, 1.1121113339878292L));
    Require(Near(catanl_nid_postfix(Make(5.0L, -0.5L)), 1.375233603310382L, -0.01905684134697106L));
    struct Large {
        long double x;
        long double y;
        long double real;
    };
    const long double belowSquareLimit = std::ldexp(1 - LDBL_EPSILON / 2, LDBL_MAX_EXP / 2 - LDBL_MANT_DIG / 2);
    const long double nearSquareLimit = std::ldexp(1 - LDBL_EPSILON / 2, LDBL_MAX_EXP / 2);
    for (const Large& large : {Large{1e20L, 1.0L, 1e-20L}, Large{-1e30L, -2.0L, -1e-30L},
                               Large{belowSquareLimit, nearSquareLimit, belowSquareLimit / nearSquareLimit / nearSquareLimit},
                               Large{std::ldexp(1.0L, LDBL_MAX_EXP * 3 / 4), 1.0L, std::ldexp(1.0L, -LDBL_MAX_EXP * 3 / 4)},
                               Large{1.0L, std::ldexp(1.0L, LDBL_MAX_EXP / 2 - 100), std::ldexp(1.0L, 200 - LDBL_MAX_EXP)},
                               Large{std::ldexp(1.0L, LDBL_MAX_EXP - 100), std::ldexp(1.0L, LDBL_MAX_EXP - 100), std::ldexp(1.0L, 99 - LDBL_MAX_EXP)}}) {
        const __complex__ long double hyperbolic = catanhl_nid_postfix(Make(large.x, large.y));
        Require(Close(__real__ hyperbolic, large.real) && Close(__imag__ hyperbolic, std::copysign(pi / 2, large.y)));
        const __complex__ long double circular = catanl_nid_postfix(Make(large.y, large.x));
        Require(Close(__imag__ circular, large.real) && Close(__real__ circular, std::copysign(pi / 2, large.y)));
    }
    const long double small = std::fmin(1e-12L, std::sqrt(LDBL_EPSILON) / 4);
    const __complex__ long double tiny = Make(small, -small);
    const __complex__ long double tinyResult = catanhl_nid_postfix(tiny);
    Require(__real__ tinyResult == __real__ tiny && __imag__ tinyResult == __imag__ tiny);
    const __complex__ long double nearOne = catanhl_nid_postfix(Make(1.0L, 1e-30L));
    Require(Close(__real__ nearOne, (std::log(2.0L) - std::log(1e-30L)) / 2) && Close(__imag__ nearOne, pi / 4 + std::atan(1e-30L / 2) / 2));
    const __complex__ long double infiniteReal = catanhl_nid_postfix(Make(infinity, 1.0L));
    Require(__real__ infiniteReal == 0.0L && Close(__imag__ infiniteReal, pi / 2));
    const __complex__ long double infiniteImaginaryArc = catanhl_nid_postfix(Make(2.0L, -infinity));
    Require(__real__ infiniteImaginaryArc == 0.0L && Close(__imag__ infiniteImaginaryArc, -pi / 2));
    const __complex__ long double infiniteNan = catanhl_nid_postfix(Make(infinity, nan));
    Require(__real__ infiniteNan == 0.0L && std::isnan(__imag__ infiniteNan));

    Require(Near(cacosl_nid_postfix(Make(-0.5L, 3.0L)), 1.7278009215838388L, -1.8301947623375088L));
    Require(Near(casinhl_nid_postfix(Make(2.0L, -0.75L)), 1.4925210319880218L, -0.3267704027823274L));
    Require(Near(cacoshl_nid_postfix(Make(-3.0L, 0.5L)), 1.7789904938267687L, 2.9669461885043438L));
    struct Small {
        __complex__ long double (APS5_VABI* function)(__complex__ long double);
        long double x;
        long double y;
        long double real;
        long double imaginary;
    };
    for (const Small& small : {Small{cacosl_nid_postfix, 1.0L, 9.4595955858256635e-168L, 3.0756455559484843e-84L, -3.0756455559484852e-84L},
                               Small{cacoshl_nid_postfix, 1.0L, 1.6876135465855033e-40L, 1.299081809042642e-20L, 1.2990818090426418e-20L},
                               Small{casinhl_nid_postfix, 4.6089293270250819e-19L, -1.0L, 6.788909578883107e-10L, -1.5707963261160056L},
                               Small{casinl_nid_postfix, 0.25L, 1e-10L, 0.25268025514207865L, 1.0327955589886446e-10L}}) {
        const __complex__ long double value = small.function(Make(small.x, small.y));
        Require(CloseTo(__real__ value, small.real, 1e-14L) && CloseTo(__imag__ value, small.imaginary, 1e-14L));
    }
    const __complex__ long double tinyArcsine = casinl_nid_postfix(Make(-0.0L, 1.6822750314810844e-09L));
    Require(__real__ tinyArcsine == 0.0L && std::signbit(__real__ tinyArcsine) && CloseTo(__imag__ tinyArcsine, 1.6822750314810844e-09L, 1e-15L));
    const long double big = std::ldexp(1.0L, LDBL_MAX_EXP * 3 / 5);
    const long double huge = std::ldexp(1.0L, LDBL_MAX_EXP * 4 / 5);
    const long double nearMax = std::ldexp(1.0L, LDBL_MAX_EXP - 100);
    const __complex__ long double largeHyperbolic = casinhl_nid_postfix(Make(big, -big));
    Require(Close(__real__ largeHyperbolic, std::log(2.0L) + std::log(std::hypot(big, big))) && Close(__imag__ largeHyperbolic, -pi / 4));
    const __complex__ long double largeCosine = cacosl_nid_postfix(Make(0.0L, huge));
    Require(Close(__real__ largeCosine, pi / 2) && Close(__imag__ largeCosine, -(std::log(2.0L) + std::log(huge))));
    const __complex__ long double largeSine = casinl_nid_postfix(Make(-nearMax, 1.0L));
    Require(Close(__real__ largeSine, -pi / 2) && Close(__imag__ largeSine, std::log(2.0L) + std::log(nearMax)));
    const __complex__ long double largeHyperbolicCosine = cacoshl_nid_postfix(Make(-nearMax, -nearMax));
    Require(Close(__real__ largeHyperbolicCosine, std::log(2.0L) + std::log(std::hypot(nearMax, nearMax))) && Close(__imag__ largeHyperbolicCosine, -3 * pi / 4));
    const __complex__ long double cosineAtOne = cacosl_nid_postfix(Make(1.0L, -0.0L));
    Require(__real__ cosineAtOne == 0.0L && __imag__ cosineAtOne == 0.0L && !std::signbit(__imag__ cosineAtOne));
    const __complex__ long double cosineNan = cacosl_nid_postfix(Make(infinity, nan));
    Require(std::isnan(__real__ cosineNan) && std::isinf(__imag__ cosineNan));
    const __complex__ long double sineNan = casinhl_nid_postfix(Make(nan, -infinity));
    Require(std::isinf(__real__ sineNan) && std::isnan(__imag__ sineNan));
    const __complex__ long double coshLarge = ccoshl_nid_postfix(Make(-12000.0L, 0.0L));
    Require(__real__ coshLarge == infinity && __imag__ coshLarge == 0 && std::signbit(__imag__ coshLarge));
    const __complex__ long double sinhLarge = csinhl_nid_postfix(Make(12000.0L, 0.0L));
    Require(__real__ sinhLarge == infinity && __imag__ sinhLarge == 0 && !std::signbit(__imag__ sinhLarge));
    const __complex__ long double expLarge = cexpl_nid_postfix(Make(12000.0L, 0.0L));
    Require(__real__ expLarge == infinity && __imag__ expLarge == 0 && !std::signbit(__imag__ expLarge));
    const __complex__ long double cosLarge = ccosl_nid_postfix(Make(0.0L, 12000.0L));
    Require(__real__ cosLarge == infinity && __imag__ cosLarge == 0 && std::signbit(__imag__ cosLarge));
    const __complex__ long double sinLarge = csinl_nid_postfix(Make(0.0L, 12000.0L));
    Require(__real__ sinLarge == 0 && __imag__ sinLarge == infinity);
    const long double scaledImaginary = std::ldexp(1.0L, -16000);
    const long double hyperbolicScaled = std::ldexp(expl_nid_postfix(6000.0L), -16001) * expl_nid_postfix(6000.0L);
    const __complex__ long double sinhScaled = csinhl_nid_postfix(Make(-12000.0L, scaledImaginary));
    Require(__real__ sinhScaled == -infinity && Close(__imag__ sinhScaled, hyperbolicScaled));
    const __complex__ long double coshScaled = ccoshl_nid_postfix(Make(-12000.0L, scaledImaginary));
    Require(__real__ coshScaled == infinity && Close(__imag__ coshScaled, -hyperbolicScaled));
    const __complex__ long double expScaled = cexpl_nid_postfix(Make(20000.0L, scaledImaginary));
    Require(__real__ expScaled == infinity && Close(__imag__ expScaled, std::ldexp(expl_nid_postfix(10000.0L), -16000) * expl_nid_postfix(10000.0L)));
    const __complex__ long double expHugeImaginary = cexpl_nid_postfix(Make(0.0L, std::ldexp(1.0L, 70)));
    Require(Close(__real__ expHugeImaginary, 0.06031484922481978513219L) && Close(__imag__ expHugeImaginary, -0.9981794021933067599641L));
    const __complex__ long double coshHugeImaginary = ccoshl_nid_postfix(Make(1.0L, std::ldexp(1.0L, 70)));
    Require(Close(__real__ coshHugeImaginary, std::cosh(1.0L) * 0.06031484922481978513219L) && Close(__imag__ coshHugeImaginary, -std::sinh(1.0L) * 0.9981794021933067599641L));
    const __complex__ long double expInfinities = cexpl_nid_postfix(Make(-infinity, infinity));
    Require(__real__ expInfinities == 0 && __imag__ expInfinities == 0);
    const __complex__ long double sinhInfinities = csinhl_nid_postfix(Make(infinity, infinity));
    Require(std::isinf(__real__ sinhInfinities) && std::isnan(__imag__ sinhInfinities));
    return 0;
}
