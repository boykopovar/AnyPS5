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

__complex__ float Make(float real, float imaginary) {
    __complex__ float result;
    __real__ result = real;
    __imag__ result = imaginary;
    return result;
}

bool Near(__complex__ float value, float real, float imaginary) {
    constexpr float tolerance = 1e-5f;
    return std::fabs(__real__ value - real) < tolerance && std::fabs(__imag__ value - imaginary) < tolerance;
}

}

int main() {
    const float pi = std::acos(-1.0f);
    const __complex__ float zero = Make(0.0f, 0.0f);
    const __complex__ float one = Make(1.0f, 0.0f);
    const __complex__ float i = Make(0.0f, 1.0f);

    Require(Near(cacosf_nid_postfix(zero), pi / 2, 0.0f));
    Require(Near(casinf_nid_postfix(zero), 0.0f, 0.0f));
    Require(Near(catanf_nid_postfix(zero), 0.0f, 0.0f));
    const auto largeAtan = catanf_nid_postfix(Make(1e30f, 1e30f));
    Require(__real__ largeAtan == 0x1.921fb6p+0f && __imag__ largeAtan == 0x1.4484cp-101f);
    const auto largeAtanh = catanhf_nid_postfix(Make(1e5f, 1.0f));
    Require(__real__ largeAtanh == 0x1.4f8b58p-17f && __imag__ largeAtanh == 0x1.921fb6p+0f);
    const auto nearPole = catanhf_nid_postfix(Make(1.0f, std::numeric_limits<float>::denorm_min()));
    Require(__real__ nearPole == 0x1.9fe368p+5f && __imag__ nearPole == 0x1.921fb6p-1f);
    const auto belowCut = catanhf_nid_postfix(Make(-2.0f, -0.0f));
    Require(__real__ belowCut == -0x1.193ea8p-1f && __imag__ belowCut == -0x1.921fb6p+0f);
    std::feclearexcept(FE_DIVBYZERO);
    const auto pole = catanf_nid_postfix(Make(-0.0f, -1.0f));
    Require(__real__ pole == 0.0f && std::signbit(__real__ pole) && std::isinf(__imag__ pole) && std::signbit(__imag__ pole));
    Require(std::fetestexcept(FE_DIVBYZERO) != 0);
    Require(Near(ccosf_nid_postfix(zero), 1.0f, 0.0f));
    Require(Near(csinf_nid_postfix(zero), 0.0f, 0.0f));
    Require(Near(ctanf_nid_postfix(zero), 0.0f, 0.0f));
    Require(Near(ccoshf_nid_postfix(zero), 1.0f, 0.0f));
    Require(Near(csinhf_nid_postfix(zero), 0.0f, 0.0f));
    Require(Near(ctanhf_nid_postfix(zero), 0.0f, 0.0f));
    const auto saturated = ctanhf_nid_postfix(Make(-100.0f, -1.0f));
    Require(__real__ saturated == -1.0f && __imag__ saturated == 0.0f && std::signbit(__imag__ saturated));
    const auto tail = ctanf_nid_postfix(Make(1.0f, 48.0f));
    Require(__real__ tail == 0x1.498p-138f && __imag__ tail == 1.0f);
    const auto hugeAngle = ctanhf_nid_postfix(Make(1.0f, 1e30f));
    Require(__real__ hugeAngle == 0x1.087fd2p+0f && __imag__ hugeAngle == 0x1.1a4e72p-2f);
    const auto nearTangentPole = ctanhf_nid_postfix(Make(0x1p-30f, 0x1.921fb6p+0f));
    Require(__real__ nearTangentPole == 0x1.dbc9d4p+18f && __imag__ nearTangentPole == -0x1.5cec08p+24f);
    const int tangentRounding = std::fegetround();
    Require(std::fesetround(FE_UPWARD) == 0);
    const auto directedPole = ctanhf_nid_postfix(Make(-0x1.356342p-105f, 0x1.921fbep+0f));
    Require(__real__ directedPole == -0x1.039c10p-63f && __imag__ directedPole == -0x1.d501a6p+20f);
    Require(std::fegetround() == FE_UPWARD);
    Require(std::fesetround(tangentRounding) == 0);
    std::feclearexcept(FE_INVALID);
    const auto infiniteAngle = ctanhf_nid_postfix(Make(-0.0f, std::numeric_limits<float>::infinity()));
    Require(__real__ infiniteAngle == 0.0f && std::signbit(__real__ infiniteAngle) && std::isnan(__imag__ infiniteAngle));
    Require(std::fetestexcept(FE_INVALID) != 0);
    Require(Near(cacoshf_nid_postfix(one), 0.0f, 0.0f));
    Require(Near(cexpf_nid_postfix(zero), 1.0f, 0.0f));
    Require(Near(clogf_nid_postfix(one), 0.0f, 0.0f));
    const auto closeToOne = clogf_nid_postfix(Make(1.0f, 0x1p-60f));
    Require(__real__ closeToOne == 0x1p-121f && __imag__ closeToOne == 0x1p-60f);
    const auto insideCircle = clogf_nid_postfix(Make(0x1.fffffep-1f, 0x1p-12f));
    Require(__real__ insideCircle == -0x1.fffffep-26f && __imag__ insideCircle == 0x1p-12f);
    const float maximum = std::numeric_limits<float>::max();
    const auto large = clogf_nid_postfix(Make(maximum, maximum));
    Require(__real__ large == 0x1.644714p+6f && __imag__ large == 0x1.921fb6p-1f);
    const auto lowerCut = clogf_nid_postfix(Make(-1.0f, -0.0f));
    Require(__real__ lowerCut == 0.0f && !std::signbit(__real__ lowerCut) && __imag__ lowerCut == -pi);
    const int rounding = std::fegetround();
    Require(std::fesetround(FE_UPWARD) == 0);
    const float minimum = std::numeric_limits<float>::denorm_min();
    const auto roundedTiny = clogf_nid_postfix(Make(1.0f, minimum));
    Require(__real__ roundedTiny == minimum);
    Require(std::fesetround(FE_DOWNWARD) == 0);
    const auto onCircle = clogf_nid_postfix(i);
    Require(__real__ onCircle == 0.0f && !std::signbit(__real__ onCircle));
    Require(std::fesetround(rounding) == 0);
    Require(Near(csqrtf_nid_postfix(Make(-4.0f, 0.0f)), 0.0f, 2.0f));
    Require(Near(cexpf_nid_postfix(Make(0.0f, pi)), -1.0f, 0.0f));
    Require(Near(csinf_nid_postfix(i), 0.0f, std::sinh(1.0f)));
    Require(Near(clogf_nid_postfix(Make(-1.0f, 0.0f)), 0.0f, pi));
    const float tiny = std::ldexp(1.0f, -14);
    const __complex__ float nearUnit = clogf_nid_postfix(Make(1.0f, tiny));
    Require(std::fabs(__real__ nearUnit - std::ldexp(1.0f, -29)) <= std::ldexp(1.0f, -29) * 1e-3f);
    Require(std::fabs(__imag__ nearUnit - tiny) <= tiny * 1e-3f);
    Require(__real__ clogf_nid_postfix(Make(tiny, 1.0f)) == __real__ nearUnit);
    Require(Near(cpowf_nid_postfix(Make(2.0f, 0.0f), Make(2.0f, 0.0f)), 4.0f, 0.0f));
    Require(Near(cpowf_nid_postfix(i, Make(2.0f, 0.0f)), -1.0f, 0.0f));
    Require(Near(csqrtf_nid_postfix(Make(-4.0f, -0.0f)), 0.0f, -2.0f));
    const __complex__ float point = Make(0.5f, 0.25f);
    Require(Near(casinhf_nid_postfix(point), 0.49267568f, 0.22432845f));
    Require(Near(catanhf_nid_postfix(point), 0.50037000f, 0.31439814f));
    const __complex__ float negative = Make(-0.5f, -0.75f);
    Require(Near(casinhf_nid_postfix(negative), -0.60633500f, -0.68220397f));
    Require(Near(catanhf_nid_postfix(negative), -0.31042828f, -0.72322067f));
    return 0;
}
