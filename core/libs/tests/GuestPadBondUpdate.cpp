#include "prx/libScePad/include/Pad.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }


int main() {
    std::int32_t value = 1;
    const auto* parameter = reinterpret_cast<const PadUpdateParameter*>(&value);
    Require(scePadBondUpdateInit() == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateTerm() == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateIsReady(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateGetStatus(0, &value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateGetReconnectStatus(&value) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateStart(-1, parameter) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateSendData(0, nullptr) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadBondUpdateResetBond(0) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadBondUpdateIsReady(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadBondUpdateGetStatus(0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadBondUpdateGetReconnectStatus(nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadBondUpdateStart(-1, nullptr) == PAD_ERROR_INVALID_ARG);
    PadUpdateData wrong{};
    Require(scePadBondUpdateSendData(0, &wrong) == PAD_ERROR_INVALID_ARG);
    return 0;
}
