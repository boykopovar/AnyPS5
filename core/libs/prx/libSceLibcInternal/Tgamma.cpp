#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"

extern "C" {

double APS5_VABI _Tgamma_nid_postfix(double x) {
    double result = 0.0;
    result = tgamma_nid_postfix(x);
    return result;
}

float APS5_VABI _FTgamma_nid_postfix(float x) {
    float result = 0.0f;
    result = tgammaf_nid_postfix(x);
    return result;
}

long double APS5_VABI _LTgamma_nid_postfix(long double x) {
    long double result = 0.0L;
    result = tgammal_nid_postfix(x);
    return result;
}

}
