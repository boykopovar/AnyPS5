#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

constexpr int PAD_ERROR_DEVICE_NOT_CONNECTED = static_cast<int>(0x80920007u);

int main() {
    std::int32_t value = 1;
    const auto* parameter = reinterpret_cast<const PadUpdateParameter*>(&value);
    const auto* data = reinterpret_cast<const PadUpdateData*>(&value);
    Require(scePadBondUpdateIsReady(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateGetStatus(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateGetReconnectStatus(&value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateStart(PAD_HANDLE, parameter) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateSendData(0, data) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateResetBond(0) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadBondUpdateInit() == PAD_OK);
    Require(scePadBondUpdateInit() == PAD_OK);
    Require(scePadBondUpdateIsReady(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadBondUpdateGetStatus(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadBondUpdateGetReconnectStatus(nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadInit_nid_postfix() == PAD_OK);
    const int pad = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(pad > 0);
    Require(scePadBondUpdateStart(PAD_HANDLE, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadBondUpdateSendData(0, nullptr) == PAD_ERROR_INVALID_ARG);
    for (int handle : {-1, 0, 2, 12345})
        Require(scePadBondUpdateStart(handle, parameter) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadBondUpdateStart(pad, parameter) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    Require(scePadBondUpdateSendData(0, data) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    Require(scePadBondUpdateResetBond(0) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    value = 1;
    Require(scePadBondUpdateIsReady(0, &value) == PAD_OK && value == 0);
    Require(scePadBondUpdateGetStatus(0, &value) == PAD_OK && value == 0);
    Require(scePadBondUpdateGetReconnectStatus(&value) == PAD_OK && value == 0);
    Require(scePadBondUpdateIsReady(-12345, &value) == PAD_OK && value == 0);

    Require(scePadBondUpdateTerm() == PAD_OK);
    Require(scePadBondUpdateTerm() == PAD_OK);
    Require(scePadBondUpdateIsReady(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    return 0;
}
