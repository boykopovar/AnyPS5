#include "prx/libSceGameUpdate/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED = static_cast<int>(0x80412801);
static constexpr int SCE_GAME_UPDATE_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80412802);
static constexpr int SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80412803);
static constexpr int SCE_GAME_UPDATE_ERROR_INVALID_SIZE = static_cast<int>(0x80412804);
static constexpr int SCE_GAME_UPDATE_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x80412805);

static_assert(sizeof(GameUpdateCheckParam) == 0x30);
static_assert(sizeof(GameUpdateCheckResult) == 0x30);

static std::atomic<bool> initialized{false};

static int checkRequestId(int requestId) {
    if (requestId <= 0) {
        return SCE_GAME_UPDATE_ERROR_REQUEST_NOT_FOUND;
    }
    if (!initialized.load()) {
        return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    }
    return SCE_GAME_UPDATE_ERROR_REQUEST_NOT_FOUND;
}

static bool hasNonZeroReserved(const GameUpdateCheckParam* param) {
    for (std::uint32_t word : param->reserved) {
        if (word != 0) {
            return true;
        }
    }
    return false;
}

extern "C" {

int APS5_VABI sceGameUpdateAbortRequest(int requestId) {
    return checkRequestId(requestId);
}

int APS5_VABI sceGameUpdateCheck(int requestId, const GameUpdateCheckParam* param, GameUpdateCheckResult* result) {
    if (requestId <= 0) {
        return SCE_GAME_UPDATE_ERROR_REQUEST_NOT_FOUND;
    }
    if (param == nullptr || result == nullptr) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    if (param->size != sizeof(GameUpdateCheckParam) || result->size != sizeof(GameUpdateCheckResult)) {
        return SCE_GAME_UPDATE_ERROR_INVALID_SIZE;
    }
    if (param->option != 0 || hasNonZeroReserved(param)) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    return checkRequestId(requestId);
}

int APS5_VABI sceGameUpdateCreateRequest(void) {
    if (!initialized.load()) {
        return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    }
    throw std::runtime_error("sceGameUpdateCreateRequest: patch service request creation is not implemented");
}

int APS5_VABI sceGameUpdateDeleteRequest(int requestId) {
    return checkRequestId(requestId);
}

int APS5_VABI sceGameUpdateGetAddcontLatestVersion(uint32_t serviceLabel, const NpUnifiedEntitlementLabel* entitlementLabel, GameUpdateAddcontVersionInfo* info) {
    (void)serviceLabel;
    if (info == nullptr) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    if (entitlementLabel == nullptr) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    if (!initialized.load()) {
        return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    }
    throw std::runtime_error("sceGameUpdateGetAddcontLatestVersion: entitlement version lookup is not implemented");
}

int APS5_VABI sceGameUpdateInitialize(void) {
    if (initialized.exchange(true)) {
        return SCE_GAME_UPDATE_ERROR_ALREADY_INITIALIZED;
    }
    return 0;
}

int APS5_VABI sceGameUpdateTerminate(void) {
    if (!initialized.exchange(false)) {
        return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    }
    return 0;
}

}
