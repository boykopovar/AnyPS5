#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Update checks need the network, which is not available; requests fail so the game skips the check.
static constexpr int SCE_GAME_UPDATE_ERROR_OFFLINE = static_cast<int>(0x80D50001);
static constexpr int SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED = static_cast<int>(0x80412801);
static constexpr int SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80412803);
static constexpr int SCE_GAME_UPDATE_ERROR_INVALID_SIZE = static_cast<int>(0x80412804);

static std::atomic<bool> g_initialized{false};

extern "C" {

int APS5_VABI sceGameUpdateAbortRequest(int request_id) {
    (void)request_id;
    return 0;
}

int APS5_VABI sceGameUpdateCheck(int request_id, const GameUpdateCheckParam* param, GameUpdateCheckResult* result) {
    (void)request_id;
    (void)param;
    (void)result;
    return SCE_GAME_UPDATE_ERROR_OFFLINE;
}

int APS5_VABI sceGameUpdateCreateRequest(void) {
    return SCE_GAME_UPDATE_ERROR_OFFLINE;
}

int APS5_VABI sceGameUpdateDeleteRequest(int request_id) {
    (void)request_id;
    return 0;
}

int APS5_VABI sceGameUpdateGetAddcontLatestVersion(uint32_t service_label, const void* entitlement_label, GameUpdateAddcontVersionInfo* info) {
    (void)service_label;
    if (!g_initialized.load()) {
        return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    }
    if (info == nullptr) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    if (entitlement_label == nullptr) {
        return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    }
    if (info->size < sizeof(GameUpdateAddcontVersionInfo)) {
        return SCE_GAME_UPDATE_ERROR_INVALID_SIZE;
    }
    const std::size_t size = info->size;
    *info = {};
    info->size = size;
    info->found = false;
    return 0;
}

int APS5_VABI sceGameUpdateInitialize(void) {
    g_initialized.store(true);
    return 0;
}

int APS5_VABI sceGameUpdateTerminate(void) {
    g_initialized.store(false);
    return 0;
}

}
