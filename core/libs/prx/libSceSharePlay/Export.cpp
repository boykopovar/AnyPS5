#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceSharePlay/include/InventoryExports.hpp"
#include "prx/libSceSharePlay/include/Api.hpp"
#include <cerrno>
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
    SharePlayConnectionInfoA out{};
    out.hostUserId = SCE_USER_SERVICE_USER_ID_INVALID;
    out.visitorUserId = SCE_USER_SERVICE_USER_ID_INVALID;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &out, sizeof(out));
    errno = savedErrno;
    if (!copied) return SCE_SHARE_PLAY_ERROR_INVALID_ARGS;
    return 0;
}

int APS5_VABI sceSharePlayProhibitController(std::uint8_t prohibited) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    if (prohibited != 0 && prohibited != 1) return SCE_SHARE_PLAY_ERROR_INVALID_ARGS;
    return 0;
}

int APS5_VABI sceSharePlayCrashDaemon(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayGetCurrentConnectionInfo(SharePlayConnectionInfo* info) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    if (info == nullptr) return SCE_SHARE_PLAY_ERROR_INVALID_ARGS;
    SharePlayConnectionInfo out{};
    out.hostUserId = SCE_USER_SERVICE_USER_ID_INVALID;
    out.visitorUserId = SCE_USER_SERVICE_USER_ID_INVALID;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &out, sizeof(out));
    errno = savedErrno;
    if (!copied) return SCE_SHARE_PLAY_ERROR_INVALID_ARGS;
    return 0;
}

int APS5_VABI sceSharePlayGetCurrentInfo(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayGetEvent(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayNotifyDialogOpen(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayNotifyForceCloseForCdlg(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayResumeScreenForCdlg(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayServerLock(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayServerUnLock(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlaySetMode(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlaySetProhibition(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlaySetProhibitionModeWithAppId(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayStartStandby(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayStartStreaming(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayStopStandby(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlayStopStreaming(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSharePlaySetShareScreenStatus() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceSharePlayUnknown_6egMR0eB8RU_nid_no_patch() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return 1;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6egMR0eB8RU", libSceSharePlayUnknown_6egMR0eB8RU_nid_no_patch);

int APS5_VABI libSceSharePlayUnknown_891hmdoV7UQ_nid_no_patch() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("891hmdoV7UQ", libSceSharePlayUnknown_891hmdoV7UQ_nid_no_patch);

int APS5_VABI libSceSharePlayUnknown_LpPA6mprZ8Q_nid_no_patch() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LpPA6mprZ8Q", libSceSharePlayUnknown_LpPA6mprZ8Q_nid_no_patch);

int APS5_VABI libSceSharePlayUnknown_wcI2co2I4Xc_nid_no_patch() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return SCE_SHARE_PLAY_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wcI2co2I4Xc", libSceSharePlayUnknown_wcI2co2I4Xc_nid_no_patch);

}
