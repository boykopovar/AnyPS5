#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    const PadVibrationParam vibZero{0, 0};
    const PadVibrationParam vibFull{255, 255};
    const PadVibrationParam vibMixed{128, 64};

    std::array<std::uint8_t, 256> validTrigger{};
    validTrigger[0] = 0x3;

    std::array<std::uint8_t, 256> invalidMask{};
    invalidMask[0] = 0x4;

    std::array<std::uint8_t, 256> invalidMaskHigh{};
    invalidMaskHigh[0] = 0xFF;

    std::array<std::uint8_t, 256> invalidModeL2{};
    invalidModeL2[0] = 0x1;
    const std::uint32_t badMode = 7;
    std::memcpy(&invalidModeL2[8], &badMode, sizeof(badMode));

    std::array<std::uint8_t, 256> invalidModeR2{};
    invalidModeR2[0] = 0x2;
    std::memcpy(&invalidModeR2[8 + 56], &badMode, sizeof(badMode));

    for (int handle : {-9999, -100, -2, -1, 0, 5, 99, 1000}) {
        Require(scePadSetVibration(handle, &vibZero) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, validTrigger.data()) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-9999, -100, -2, -1, 0, 5, 99, 1000}) {
        Require(scePadSetVibration(handle, &vibZero) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, validTrigger.data()) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibration(handle, &vibZero) == PAD_OK);
    Require(scePadSetVibration(handle, &vibFull) == PAD_OK);
    Require(scePadSetVibration(handle, &vibMixed) == PAD_OK);

    Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetTriggerEffect(handle, validTrigger.data()) == PAD_OK);
    Require(scePadSetTriggerEffect(handle, invalidMask.data()) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetTriggerEffect(handle, invalidMaskHigh.data()) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetTriggerEffect(handle, invalidModeL2.data()) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetTriggerEffect(handle, invalidModeR2.data()) == PAD_ERROR_INVALID_ARG);

    for (int invalidHandle : {-9999, -100, -2, -1, 0, handle + 99, 1000}) {
        Require(scePadSetVibration(invalidHandle, &vibMixed) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(invalidHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(invalidHandle, validTrigger.data()) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTriggerEffect(invalidHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    Require(scePadSetVibration(handle, &vibZero) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTriggerEffect(handle, validTrigger.data()) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTriggerEffect(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    Require(scePadSetVibration(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibration(special, &vibZero) == PAD_OK);
    Require(scePadSetTriggerEffect(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetTriggerEffect(special, validTrigger.data()) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);

    Require(scePadSetVibration(special, &vibZero) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVibration(special, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTriggerEffect(special, validTrigger.data()) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetTriggerEffect(special, nullptr) == PAD_ERROR_INVALID_HANDLE);

    return 0;
}
