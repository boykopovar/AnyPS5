#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <limits>
#include <stdexcept>

extern "C" {
short APS5_VABI _FDsign_nid_postfix(const float* value);
short APS5_VABI _Dsign_nid_postfix(const double* value);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    try {
        _FDsign_nid_postfix(nullptr);
        std::abort();
    } catch (const std::invalid_argument&) {
    }
    try {
        _Dsign_nid_postfix(nullptr);
        std::abort();
    } catch (const std::invalid_argument&) {
    }

    const float positiveFloat = 1.0f;
    const float negativeFloat = -1.0f;
    const float negativeZeroFloat = -0.0f;
    Require(_FDsign_nid_postfix(&positiveFloat) == 0);
    Require(_FDsign_nid_postfix(&negativeFloat) == static_cast<short>(0x8000));
    Require(_FDsign_nid_postfix(&negativeZeroFloat) == static_cast<short>(0x8000));

    const double positiveDouble = 1.0;
    const double negativeDouble = -1.0;
    const double negativeZeroDouble = -0.0;
    const double negativeInf = -std::numeric_limits<double>::infinity();
    Require(_Dsign_nid_postfix(&positiveDouble) == 0);
    Require(_Dsign_nid_postfix(&negativeDouble) == static_cast<short>(0x8000));
    Require(_Dsign_nid_postfix(&negativeZeroDouble) == static_cast<short>(0x8000));
    Require(_Dsign_nid_postfix(&negativeInf) == static_cast<short>(0x8000));
    return 0;
}
