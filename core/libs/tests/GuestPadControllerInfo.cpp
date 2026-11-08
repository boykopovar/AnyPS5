#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(scePadInit_nid_postfix() == PAD_OK);
    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    PadControllerInformation info;
    std::memset(&info, 0x5a, sizeof(info));
    Require(scePadGetControllerInformation(handle, &info) == PAD_OK);
    Require(info.touchPadInfo.resolution.x == kTouchPadWidth);
    Require(info.touchPadInfo.resolution.y == kTouchPadHeight);
    Require(info.touchPadInfo.pixelDensity == 44.86f);
    Require(info.stickInfo.deadZoneLeft == 13 && info.stickInfo.deadZoneRight == 13);
    Require(info.connected && info.connectedCount == 1);

    Require(scePadGetControllerInformation(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    for (const int invalid : {-9999, -100, -2, -1, 0, handle + 1, 9999}) {
        Require(scePadGetControllerInformation(invalid, &info) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
}
