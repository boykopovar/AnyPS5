#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

double APS5_VABI _Pow_nid_postfix(double x, double y, short*) {
    return pow_nid_postfix(x, y);
}

float APS5_VABI _FPow_nid_postfix(float x, float y, short*) {
    return powf_nid_postfix(x, y);
}

long double APS5_VABI _LPow_nid_postfix(long double x, long double y, short* exponent) {
    if (exponent != nullptr) NotImplemented_nid_no_patch("_LPow: a non-null exponent pointer");
    return powl_nid_postfix(x, y);
}

}
