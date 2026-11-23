#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include <limits>

namespace {

int NegateExponent(int exponent) {
    return exponent == std::numeric_limits<int>::min() ? exponent : -exponent;
}

}

extern "C" {

double APS5_VABI sceLibcInternalSignificand_nid_no_patch(double x) {
    return scalbn_nid_postfix(x, NegateExponent(ilogb_nid_postfix(x)));
}
APS5_EXPORT("Az3tTyAy380", sceLibcInternalSignificand_nid_no_patch);

float APS5_VABI sceLibcInternalSignificandF_nid_no_patch(float x) {
    return scalbnf_nid_postfix(x, NegateExponent(ilogbf_nid_postfix(x)));
}
APS5_EXPORT("L2YaHYQdmHw", sceLibcInternalSignificandF_nid_no_patch);

}
