#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <limits>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr short Negative = static_cast<short>(0x8000);
    const float floatInf = std::numeric_limits<float>::infinity();
    const float floatNaN = std::numeric_limits<float>::quiet_NaN();
    Require(_FDsign_nid_postfix(1.0f) == 0);
    Require(_FDsign_nid_postfix(0.0f) == 0);
    Require(_FDsign_nid_postfix(floatInf) == 0);
    Require(_FDsign_nid_postfix(-1.0f) == Negative);
    Require(_FDsign_nid_postfix(-0.0f) == Negative);
    Require(_FDsign_nid_postfix(-floatInf) == Negative);
    Require(_FDsign_nid_postfix(-floatNaN) == Negative);

    const double doubleInf = std::numeric_limits<double>::infinity();
    const double doubleNaN = std::numeric_limits<double>::quiet_NaN();
    Require(_Dsign_nid_postfix(1.0) == 0);
    Require(_Dsign_nid_postfix(0.0) == 0);
    Require(_Dsign_nid_postfix(doubleInf) == 0);
    Require(_Dsign_nid_postfix(-1.0) == Negative);
    Require(_Dsign_nid_postfix(-0.0) == Negative);
    Require(_Dsign_nid_postfix(-doubleInf) == Negative);
    Require(_Dsign_nid_postfix(-doubleNaN) == Negative);
    return 0;
}
