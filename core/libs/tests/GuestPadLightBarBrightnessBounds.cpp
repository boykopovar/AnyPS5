#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);

    for (int handle : {-1, 0, PAD_REMOTE_HANDLE, 12345, 9999}) {
        Require(scePadSetLightBarBaseBrightness(handle, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 255) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-1, 0, PAD_REMOTE_HANDLE, 12345, 9999}) {
        Require(scePadSetLightBarBaseBrightness(handle, 0) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBaseBrightness(handle, 255) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsLightBarBaseBrightnessControllable(handle) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);
    Require(scePadIsLightBarBaseBrightnessControllable(handle) == 1);
    Require(scePadSetLightBarBaseBrightness(handle, 0) == PAD_OK);
    Require(scePadSetLightBarBaseBrightness(handle, 1) == PAD_OK);
    Require(scePadSetLightBarBaseBrightness(handle, 2) == PAD_OK);
    Require(scePadSetLightBarBaseBrightness(handle, 255) == PAD_OK);

    const int handle2 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle2 > 0);
    Require(scePadIsLightBarBaseBrightnessControllable(handle2) == 1);
    Require(scePadSetLightBarBaseBrightness(handle2, 1) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadIsLightBarBaseBrightnessControllable(special) == 0);
    Require(scePadSetLightBarBaseBrightness(special, 0) == PAD_OK);
    Require(scePadSetLightBarBaseBrightness(special, 1) == PAD_OK);
    Require(scePadSetLightBarBaseBrightness(special, 255) == PAD_OK);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadIsLightBarBaseBrightnessControllable(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBarBaseBrightness(special, 1) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    Require(scePadClose_nid_postfix(handle2) == PAD_OK);

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);
    return 0;
}
