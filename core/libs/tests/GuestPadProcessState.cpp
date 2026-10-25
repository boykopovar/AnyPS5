#include "SceTypes.hpp"
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
    Require(scePadSetProcessFocus(nullptr) == PAD_ERROR_INVALID_ARG);
    for (int handle : {-1, 0, 2, 12345}) {
        Require(scePadSetPowerSavingState(handle, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetUserNumber(handle, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibrationStrength(handle, 50) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffectStrength(handle, 50) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetIsCoPilot(handle, true) == PAD_ERROR_INVALID_HANDLE);
    }
    Require(scePadSetAutoPowerOffCount(300) == PAD_OK);
    Require(scePadSetPowerSavingState(PAD_HANDLE, 1) == PAD_OK);
    Require(scePadSetProcessFocus(focus) == PAD_OK);
    Require(scePadSetLoginUserNumber(1) == PAD_OK);
    Require(scePadSetUserNumber(PAD_HANDLE, 1) == PAD_OK);
    Require(scePadSetVibrationStrength(PAD_HANDLE, 50) == PAD_OK);
    Require(scePadSetTriggerEffectStrength(PAD_HANDLE, 50) == PAD_OK);
    Require(scePadSetForceIntercepted(true) == PAD_OK);
    Require(scePadSetIsCoPilot(PAD_HANDLE, true) == PAD_OK);
    return 0;
}
