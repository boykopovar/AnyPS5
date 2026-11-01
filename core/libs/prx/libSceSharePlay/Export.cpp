#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceSharePlay/include/InventoryExports.hpp"
#include "prx/libSceSharePlay/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_SHARE_PLAY_ERROR_INVALID_ARGS = static_cast<int>(0x810E0001);
static constexpr int SCE_SHARE_PLAY_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x810E0003);
static constexpr int SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED = static_cast<int>(0x810E0004);
static std::mutex g_mutex;
static bool g_initialized = false;
static constexpr std::int32_t SCE_USER_SERVICE_USER_ID_INVALID = -1;

extern "C" {

int APS5_VABI sceSharePlayInitialize(void* heap, size_t heapSize) {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return SCE_SHARE_PLAY_ERROR_ALREADY_INITIALIZED;
    if (heap && heapSize < 0x1800) return SCE_SHARE_PLAY_ERROR_INVALID_ARGS;
    g_initialized = true;
    return 0;
}

int APS5_VABI sceSharePlayTerminate(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    g_initialized = false;
    return 0;
}

int APS5_VABI sceSharePlayGetCurrentConnectionInfoA(SharePlayConnectionInfoA* info) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    if (info == nullptr) return SCE_SHARE_PLAY_ERROR_INVALID_ARGS;
    *info = {};
    info->hostUserId = SCE_USER_SERVICE_USER_ID_INVALID;
    info->visitorUserId = SCE_USER_SERVICE_USER_ID_INVALID;
    return 0;
}

int APS5_VABI sceSharePlayProhibitController(std::uint8_t prohibited) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    if (prohibited != 0 && prohibited != 1) return SCE_SHARE_PLAY_ERROR_INVALID_ARGS;
    return 0;
}

int APS5_VABI sceSharePlayCrashDaemon(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayGetCurrentConnectionInfo(SharePlayConnectionInfo* info) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    if (info == nullptr) return SCE_SHARE_PLAY_ERROR_INVALID_ARGS;
    *info = {};
    info->hostUserId = SCE_USER_SERVICE_USER_ID_INVALID;
    info->visitorUserId = SCE_USER_SERVICE_USER_ID_INVALID;
    return 0;
}

int APS5_VABI sceSharePlayGetCurrentInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayGetEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayNotifyDialogOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayNotifyForceCloseForCdlg(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayResumeScreenForCdlg(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayServerLock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayServerUnLock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlaySetMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlaySetProhibition(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlaySetProhibitionModeWithAppId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayStartStandby(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayStartStreaming(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayStopStandby(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayStopStreaming(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlaySetShareScreenStatus() {
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceSharePlayUnknown_6egMR0eB8RU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6egMR0eB8RU", libSceSharePlayUnknown_6egMR0eB8RU_nid_no_patch);

int APS5_VABI libSceSharePlayUnknown_891hmdoV7UQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("891hmdoV7UQ", libSceSharePlayUnknown_891hmdoV7UQ_nid_no_patch);

int APS5_VABI libSceSharePlayUnknown_LpPA6mprZ8Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LpPA6mprZ8Q", libSceSharePlayUnknown_LpPA6mprZ8Q_nid_no_patch);

int APS5_VABI libSceSharePlayUnknown_wcI2co2I4Xc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wcI2co2I4Xc", libSceSharePlayUnknown_wcI2co2I4Xc_nid_no_patch);

}
