#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);
    Require(scePadEnableExtensionPort(1) == PAD_OK);
    Require(scePadEnableExtensionPort(0) == PAD_OK);
    Require(scePadEnableExtensionPort(static_cast<bool>(2)) == PAD_OK);
    Require(scePadEnableExtensionPort(static_cast<bool>(-1)) == PAD_OK);

    Require(scePadInit_nid_postfix() == PAD_OK);

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);

    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(-1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(0) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(handle + 999) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadEnableExtensionPort(true) == PAD_OK);
    Require(scePadEnableExtensionPort(false) == PAD_OK);

    return 0;
}
