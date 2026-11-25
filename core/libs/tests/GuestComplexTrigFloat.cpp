#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cmath>
#include <cstdlib>

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
    Require(Near(ccosf_nid_postfix(zero), 1.0f, 0.0f));
    Require(Near(csinf_nid_postfix(zero), 0.0f, 0.0f));
    Require(Near(ctanf_nid_postfix(zero), 0.0f, 0.0f));
    Require(Near(ccoshf_nid_postfix(zero), 1.0f, 0.0f));
    Require(Near(csinhf_nid_postfix(zero), 0.0f, 0.0f));
    Require(Near(ctanhf_nid_postfix(zero), 0.0f, 0.0f));
    Require(Near(cacoshf_nid_postfix(one), 0.0f, 0.0f));
    Require(Near(cexpf_nid_postfix(zero), 1.0f, 0.0f));
    Require(Near(clogf_nid_postfix(one), 0.0f, 0.0f));
    Require(Near(csqrtf_nid_postfix(Make(-4.0f, 0.0f)), 0.0f, 2.0f));
    Require(Near(cexpf_nid_postfix(Make(0.0f, pi)), -1.0f, 0.0f));
    Require(Near(csinf_nid_postfix(i), 0.0f, std::sinh(1.0f)));
    Require(Near(clogf_nid_postfix(Make(-1.0f, 0.0f)), 0.0f, pi));
    const float tiny = std::ldexp(1.0f, -14);
    const __complex__ float nearUnit = clogf_nid_postfix(Make(1.0f, tiny));
    Require(std::fabs(__real__ nearUnit - std::ldexp(1.0f, -29)) <= std::ldexp(1.0f, -29) * 1e-3f);
    Require(std::fabs(__imag__ nearUnit - tiny) <= tiny * 1e-3f);
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
