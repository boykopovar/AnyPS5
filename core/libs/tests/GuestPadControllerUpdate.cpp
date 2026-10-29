#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

constexpr int PAD_ERROR_DEVICE_NOT_CONNECTED = static_cast<int>(0x80920007u);

int main() {
    std::int32_t value = 1;
    std::uint16_t status = 7;
    const auto* parameter = reinterpret_cast<const PadUpdateParameter*>(&value);
    const auto* data = reinterpret_cast<const PadUpdateData*>(&value);
    Require(scePadControllerUpdateIsUpdateNeeded2(0, nullptr, &status) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateIsReady(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateGetStatus(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateGetReconnectStatus(&value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateStart(PAD_HANDLE, parameter) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateSendData(0, data) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateResetController(0) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadControllerUpdateInit() == PAD_OK);
    Require(scePadControllerUpdateInit() == PAD_OK);
    Require(scePadControllerUpdateIsUpdateNeeded2(0, nullptr, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateIsReady(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateGetStatus(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateGetReconnectStatus(nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadInit_nid_postfix() == PAD_OK);
    const int pad = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(pad > 0);
    Require(scePadControllerUpdateStart(PAD_HANDLE, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateSendData(0, nullptr) == PAD_ERROR_INVALID_ARG);
    for (int handle : {-1, 0, 2, 12345})
        Require(scePadControllerUpdateStart(handle, parameter) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadControllerUpdateStart(pad, parameter) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    Require(scePadControllerUpdateSendData(0, data) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    Require(scePadControllerUpdateResetController(0) == PAD_ERROR_DEVICE_NOT_CONNECTED);
    value = 1;
    status = 7;
    Require(scePadControllerUpdateIsUpdateNeeded2(0, nullptr, &status) == PAD_OK && status == 0);
    Require(scePadControllerUpdateIsReady(0, &value) == PAD_OK && value == 0);
    Require(scePadControllerUpdateGetStatus(0, &value) == PAD_OK && value == 0);
    Require(scePadControllerUpdateGetReconnectStatus(&value) == PAD_OK && value == 0);
    Require(scePadControllerUpdateIsUpdateNeeded2(-12345, nullptr, &status) == PAD_OK && status == 0);
    const std::uint32_t version = 0x01020304;
    Require(scePadControllerUpdateIsUpdateNeeded2(0, &version, &status) == PAD_OK && status == 0);

    Require(scePadControllerUpdateTerm() == PAD_OK);
    Require(scePadControllerUpdateTerm() == PAD_OK);
    Require(scePadControllerUpdateIsReady(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    return 0;
}
