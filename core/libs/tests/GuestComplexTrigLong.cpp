#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cfenv>
#include <cmath>
#include <cstdlib>
#include <limits>

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
    const auto nearAxis = clogl_nid_postfix(Make(1.0L, -0x1p-80L));
    Require(__real__ nearAxis == 0x1p-161L && __imag__ nearAxis == -0x1p-80L);
    const auto nearImaginaryAxis = clogl_nid_postfix(Make(-0x1p-65L, -1.0L));
    Require(__real__ nearImaginaryAxis == 0x1p-131L && __imag__ nearImaginaryAxis < 0.0L);
    using Limits = std::numeric_limits<long double>;
    const long double step = std::ldexp(1.0L, -(Limits::digits / 2));
    const long double square = step * step;
    const auto insideCircle = clogl_nid_postfix(Make(1.0L - square, step));
    Require(std::fabs(__real__ insideCircle + square / 2) <= square * Limits::epsilon());
    const auto outsideCircle = clogl_nid_postfix(Make(1.0L - 2 * square, 2 * step));
    Require(__real__ outsideCircle == 2 * square * square);
    const int originalRounding = std::fegetround();
    const int modes[] = {FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};
    for (int mode : modes) {
        Require(std::fesetround(mode) == 0);
        const auto directed = clogl_nid_postfix(Make(1.0L - 2 * square, 2 * step));
        Require(std::fegetround() == mode);
        Require(std::fabs(__real__ directed - 2 * square * square) <= 4 * square * square * Limits::epsilon());
    }
    Require(std::fesetround(originalRounding) == 0);
    const auto belowCut = clogl_nid_postfix(Make(-1.0L, -0.0L));
    Require(__real__ belowCut == 0.0L && !std::signbit(__real__ belowCut) && __imag__ belowCut == -pi);
    constexpr int exponent = (Limits::min_exponent - Limits::digits) / 2 + 1;
    const auto subnormal = clogl_nid_postfix(Make(1.0L, std::ldexp(1.0L, exponent)));
    Require(__real__ subnormal == std::ldexp(1.0L, 2 * exponent - 1));
    const long double scales[] = {Limits::max(), Limits::denorm_min()};
    for (long double scale : scales) {
        const auto diagonal = clogl_nid_postfix(Make(scale, scale));
        const long double expected = std::log(scale) + std::log(2.0L) / 2;
        Require(std::isfinite(__real__ diagonal));
        Require(std::fabs(__real__ diagonal - expected) <= 2 * std::fabs(expected) * Limits::epsilon());
        Require(__imag__ diagonal == pi / 4);
    }
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
    return 0;
}
