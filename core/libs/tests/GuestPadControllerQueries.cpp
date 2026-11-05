#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    int busType = -1;
    int jack = -1;
    PadContainerIdInformation container{};
    Require(scePadGetParticularMode() == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetControllerBusType(1, &busType) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetControllerBusType(1, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetJackState(1, &jack) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadIsControllerUpdateRequired(1) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadGetContainerIdInformation(1, &container) == PAD_ERROR_NOT_INITIALIZED);
    Require(busType == -1 && jack == -1);

    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadGetParticularMode() == 0);
    Require(scePadSetParticularMode(true) == PAD_OK);
    Require(scePadGetParticularMode() == 1);
    Require(scePadSetParticularMode(false) == PAD_OK);
    Require(scePadGetParticularMode() == 0);

    for (int handle : {-1, 0, 1, 12345}) {
        Require(scePadGetControllerBusType(handle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetJackState(handle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetContainerIdInformation(handle, nullptr) == PAD_ERROR_INVALID_ARG);
        Require(scePadGetControllerBusType(handle, &busType) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetJackState(handle, &jack) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadIsControllerUpdateRequired(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadGetContainerIdInformation(handle, &container) == PAD_ERROR_INVALID_HANDLE);
    }
    Require(busType == -1 && jack == -1 && container.size == 0);

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);
    Require(scePadGetControllerBusType(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadGetControllerBusType(handle, &busType) == PAD_OK);
    Require(busType == PAD_CONNECT_TYPE_BLUETOOTH);
    Require(scePadGetJackState(handle, &jack) == PAD_OK);
    Require(jack == 0);
    Require(scePadIsControllerUpdateRequired(handle) == 0);
    std::memset(&container, 0x5a, sizeof(container));
    Require(scePadGetContainerIdInformation(handle, &container) == PAD_OK);
    Require(container.size == 0 && container.id[0] == '\0');

    PadSetPortConnected_nid_postfix(0, false);
    Require(scePadGetControllerBusType(handle, &busType) == static_cast<int>(0x80920007));
    Require(scePadGetJackState(handle, &jack) == static_cast<int>(0x80920007));
    Require(scePadIsControllerUpdateRequired(handle) == static_cast<int>(0x80920007));
    Require(scePadGetContainerIdInformation(handle, &container) == static_cast<int>(0x80920007));
    PadSetPortConnected_nid_postfix(0, true);
    Require(scePadGetJackState(handle, &jack) == PAD_OK);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadGetJackState(handle, &jack) == PAD_OK);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadGetControllerBusType(special, &busType) == static_cast<int>(0x80920007));
    Require(scePadGetJackState(special, &jack) == static_cast<int>(0x80920007));
    Require(scePadIsControllerUpdateRequired(special) == static_cast<int>(0x80920007));
    Require(scePadGetContainerIdInformation(special, &container) == static_cast<int>(0x80920007));
    Require(scePadClose_nid_postfix(special) == PAD_OK);
}
