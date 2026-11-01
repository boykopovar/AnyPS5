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
    const PadLightBarParam color{255, 128, 64};

    for (int handle : {-1, -2, -100, -9999}) {
        Require(scePadSetLightBar(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-1, -2, -100, -9999}) {
        Require(scePadSetLightBar(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);
    Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetLightBar(handle, &color) == PAD_OK);

    for (int invalidHandle : {-1, -2, -100, -9999}) {
        Require(scePadSetLightBar(invalidHandle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(invalidHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetLightBar(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetLightBar(special, &color) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);

    return 0;
}
