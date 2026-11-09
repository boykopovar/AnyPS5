#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);

    PadVibrationParam param{};
    Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibration(-1, &param) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVibration(handle + 999, &param) == PAD_ERROR_INVALID_HANDLE);

    for (int large : {-1, 256, 300, 1000}) {
        param.largeMotor = static_cast<std::uint8_t>(large);
        param.smallMotor = 0;
        Require(scePadSetVibration(handle, &param) == PAD_OK);
    }

    for (int small : {-1, 256, 300, 1000}) {
        param.largeMotor = 0;
        param.smallMotor = static_cast<std::uint8_t>(small);
        Require(scePadSetVibration(handle, &param) == PAD_OK);
    }

    for (int i = 0; i <= 255; ++i) {
        param.largeMotor = static_cast<std::uint8_t>(i);
        param.smallMotor = static_cast<std::uint8_t>(255 - i);
        Require(scePadSetVibration(handle, &param) == PAD_OK);
    }

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadSetVibration(handle, &param) == PAD_ERROR_INVALID_HANDLE);
}
