#include "prx/libScePad/include/Pad.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    std::uint32_t parameter = 0;
    const auto* focus = reinterpret_cast<const PadProcessFocusParameter*>(&parameter);
    Require(scePadSetAutoPowerOffCount(300) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetPowerSavingState(PAD_HANDLE, 1) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetProcessFocus(focus) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetLoginUserNumber(1) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetUserNumber(PAD_HANDLE, 1) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetVibrationStrength(PAD_HANDLE, 50) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetTriggerEffectStrength(PAD_HANDLE, 50) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetForceIntercepted(true) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetIsCoPilot(PAD_HANDLE, true) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);
    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);
    Require(scePadSetProcessFocus(nullptr) == PAD_ERROR_INVALID_ARG);
    for (int unopened : {-1, 0, 2, 12345}) {
        Require(scePadSetPowerSavingState(unopened, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetUserNumber(unopened, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibrationStrength(unopened, 50) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffectStrength(unopened, 50) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetIsCoPilot(unopened, true) == PAD_ERROR_INVALID_HANDLE);
    }
    return 0;
}
