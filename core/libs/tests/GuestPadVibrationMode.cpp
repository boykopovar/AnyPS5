#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    for (int handle : {-1, 0, 1, 12345})
        for (int mode : {-1, 0, 1, 4})
            Require(scePadSetVibrationMode(handle, mode) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);
    for (int handle : {-1, 0, 12345}) {
        Require(scePadSetVibrationMode(handle, -1) == PAD_ERROR_INVALID_ARG);
        Require(scePadSetVibrationMode(handle, 0) == PAD_ERROR_INVALID_ARG);
        for (int mode : {1, 2, 3, 4, 5})
            Require(scePadSetVibrationMode(handle, mode) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);
    Require(scePadSetVibrationMode(handle, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(handle, 1) == PAD_OK);
    Require(scePadSetVibrationMode(handle, 2) == PAD_OK);
    Require(scePadSetVibrationMode(handle, 3) == PAD_OK);
    Require(scePadSetVibrationMode(handle, 4) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(handle, 5) == PAD_OK);
    Require(scePadSetVibrationMode(handle, 6) == PAD_ERROR_INVALID_ARG);
    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetVibrationMode(special, 1) == PAD_OK);
    Require(scePadSetVibrationMode(special, 4) == PAD_ERROR_INVALID_ARG);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
}
