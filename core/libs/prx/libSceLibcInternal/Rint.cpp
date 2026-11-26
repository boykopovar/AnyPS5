#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"

extern "C" {

double APS5_VABI _Rint_nid_postfix(double x) {
    double result = 0.0;
    result = rint_nid_postfix(x);
    return result;
}

float APS5_VABI _FRint_nid_postfix(float x) {
    float result = 0.0f;
    result = rintf_nid_postfix(x);
    return result;
}

long double APS5_VABI _LRint_nid_postfix(long double x) {
    long double result = 0.0L;
    result = rintl_nid_postfix(x);
    return result;
}

}
