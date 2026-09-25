#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Update checks need the network, which is not available; requests fail so the game skips the check.
static constexpr int SCE_GAME_UPDATE_ERROR_OFFLINE = static_cast<int>(0x80D50001);

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
 (void)entitlement_label;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceGameUpdateInitialize(void) {
    return 0;
}

int APS5_VABI sceGameUpdateTerminate(void) {
    return 0;
}

}
