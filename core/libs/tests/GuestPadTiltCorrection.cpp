#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    for (int handle : {-1, 0, 1, 12345}) {
        Require(scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadSetTiltCorrectionState(handle, false) == PAD_ERROR_NOT_INITIALIZED);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);
    for (int handle : {-1, 0, 12345}) {
        Require(scePadSetTiltCorrectionState(handle, true) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetTiltCorrectionState(handle, false) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);
    Require(scePadSetTiltCorrectionState(handle, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(handle, false) == PAD_OK);
    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetTiltCorrectionState(special, true) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, false) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetTiltCorrectionState(special, true) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(-1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(0) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(12345) == PAD_ERROR_INVALID_HANDLE);
}
