#include "prx/libScePad/include/Pad.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }


int main() {
    std::int32_t value = 1;
    const std::uint32_t version = 7;
    const auto* parameter = reinterpret_cast<const PadUpdateParameter*>(&value);
    Require(scePadControllerUpdateInit() == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateTerm() == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateIsReady(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateGetStatus(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateGetReconnectStatus(&value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateStart(-1, parameter) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateSendData(0, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadControllerUpdateResetController(0) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadControllerUpdateIsUpdateNeeded2(0, &version, nullptr) == PAD_ERROR_INVALID_ARG);
    std::uint16_t status = 7;
    Require(scePadControllerUpdateIsUpdateNeeded2(0, nullptr, &status) == PAD_ERROR_INVALID_ARG);
    Require(status == 0);
    Require(scePadControllerUpdateIsUpdateNeeded2(-12345, nullptr, &status) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateIsReady(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateGetStatus(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateGetReconnectStatus(nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadControllerUpdateStart(-1, nullptr) == PAD_ERROR_INVALID_ARG);
    PadUpdateData wrong{};
    Require(scePadControllerUpdateSendData(0, &wrong) == PAD_ERROR_INVALID_ARG);
    return 0;
}
