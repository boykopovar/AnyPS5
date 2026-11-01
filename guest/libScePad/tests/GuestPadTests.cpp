#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static inline int scePadRead(int handle, PadData* data, int num) {
    return scePadRead_nid_postfix(handle, data, num);
}

static inline int scePadClose(int handle) {
    return scePadClose_nid_postfix(handle);
}

int main() {
    PadData data{};
    const PadLightBarParam color{255, 128, 64};

    for (int handle : {-9999, -100, -2, -1, 0, 5, 99, 1000}) {
        Require(scePadRead(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead(handle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadClose(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-9999, -100, -2, -1, 0, 5, 99, 1000}) {
        Require(scePadRead(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead(handle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(handle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadClose(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    Require(scePadRead(handle, nullptr, 1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(handle, nullptr, 1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead(handle, &data, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(handle, &data, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead(handle, &data, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(handle, &data, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead(handle, nullptr, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(handle, nullptr, 0) == PAD_ERROR_INVALID_ARG);

    Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetLightBar(handle, &color) == PAD_OK);

    for (int invalidHandle : {-9999, -100, -2, -1, 0, handle + 99, 1000}) {
        Require(scePadRead(invalidHandle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(invalidHandle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead(invalidHandle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadRead_nid_postfix(invalidHandle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadClose(invalidHandle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadClose_nid_postfix(invalidHandle) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(invalidHandle, &color) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(invalidHandle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose(handle) == PAD_OK);

    Require(scePadClose(handle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(handle) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead_nid_postfix(handle, &data, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead(handle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead_nid_postfix(handle, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(handle, &color) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);

    Require(scePadRead(special, nullptr, 1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(special, nullptr, 1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead(special, &data, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(special, &data, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead(special, &data, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadRead_nid_postfix(special, &data, -1) == PAD_ERROR_INVALID_ARG);

    Require(scePadSetLightBar(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetLightBar(special, &color) == PAD_OK);

    Require(scePadClose(special) == PAD_OK);

    Require(scePadClose(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(special) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead(special, &data, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead_nid_postfix(special, &data, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead(special, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadRead_nid_postfix(special, nullptr, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(special, &color) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(special, nullptr) == PAD_ERROR_INVALID_HANDLE);

    return 0;
}
