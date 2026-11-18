#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    PadControllerInformation info{};
    for (int handle : {-9999, -1, 0, 1, PAD_REMOTE_HANDLE, 12345}) {
        Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(handle, &info) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-9999, -1, 0, 1, PAD_REMOTE_HANDLE, 12345}) {
        Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetControllerInformation(handle, &info) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_ARG);

    std::memset(&info, 0xaa, sizeof(info));
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);
    Require(info.touchPadInfo.pixelDensity == 44.86f);
    Require(info.touchPadInfo.resolution.x == kTouchPadWidth);
    Require(info.touchPadInfo.resolution.y == kTouchPadHeight);
    Require(info.stickInfo.deadZoneLeft == 13);
    Require(info.stickInfo.deadZoneRight == 13);
    Require(info.connectionType == PAD_CONNECTION_TYPE_LOCAL);
    Require(info.deviceClass == PAD_DEVICE_CLASS_STANDARD);
    Require(info.connected && info.connectedCount == 1);

    PadSetPortConnected_nid_postfix(0, false);
    std::memset(&info, 0xbb, sizeof(info));
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);
    Require(!info.connected && info.connectedCount == 0);
    Require(info.touchPadInfo.pixelDensity == 44.86f);
    Require(info.touchPadInfo.resolution.x == kTouchPadWidth);
    Require(info.touchPadInfo.resolution.y == kTouchPadHeight);
    Require(info.stickInfo.deadZoneLeft == 13);
    Require(info.stickInfo.deadZoneRight == 13);
    Require(info.connectionType == PAD_CONNECTION_TYPE_LOCAL);
    Require(info.deviceClass == PAD_DEVICE_CLASS_STANDARD);

    PadSetPortConnected_nid_postfix(0, true);
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);
    Require(info.connected && info.connectedCount == 1);

    const int handle2 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle2 > 0);
    Require(scePadGetControllerInformation(handle2, nullptr) == PAD_ERROR_INVALID_ARG);
    PadSetPortConnected_nid_postfix(1, false);
    Require(scePadGetControllerInformation(handle2, &info) == PAD_OK);
    Require(!info.connected && info.connectedCount == 0);
    PadSetPortConnected_nid_postfix(1, true);
    Require(scePadGetControllerInformation(handle2, &info) == PAD_OK);
    Require(info.connected && info.connectedCount == 1);
    Require(scePadClose_nid_postfix(handle2) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadGetControllerInformation(special, nullptr) == PAD_ERROR_INVALID_ARG);

    std::memset(&info, 0xcc, sizeof(info));
    Require(scePadGetControllerInformation(special, &info) == PAD_OK);
    Require(!info.connected && info.connectedCount == 0);
    Require(info.touchPadInfo.pixelDensity == 0.0f);
    Require(info.stickInfo.deadZoneLeft == 0);
    Require(info.stickInfo.deadZoneRight == 0);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadGetControllerInformation(special, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadGetControllerInformation(special, &info) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    return 0;
}
