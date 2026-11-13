#include "prx/libSceVisionManager/libSceVisionManager.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notOpen = static_cast<int>(0x80E70025u);
    for (int call = 0; call < 2; ++call) {
        Require(sceVisionManagerRequestCalibrateHmd() == notOpen);
        Require(sceVisionManagerRequestCalibrateStereoCamera() == notOpen);
        Require(sceVisionManagerRequestCalibrateTrackingLed() == notOpen);
        Require(sceVisionManagerRequestClose() == notOpen);
        Require(sceVisionManagerRequestRecogUtility() == notOpen);
    }
    return 0;
}
