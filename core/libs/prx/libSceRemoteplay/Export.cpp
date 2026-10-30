#include "prx/libSceRemoteplay/include/InventoryExports.hpp"
#include "prx/libSceRemoteplay/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

constexpr int REMOTEPLAY_CONNECTION_STATUS_DISCONNECT = 0;
constexpr int REMOTEPLAY_ERROR_INVALID_ARG = static_cast<int>(0x80FC0001);
constexpr int REMOTEPLAY_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80FC0003);
constexpr int REMOTEPLAY_ERROR_NOT_INITIALIZED = static_cast<int>(0x80FC0004);
constexpr std::size_t REMOTEPLAY_MINIMUM_HEAP_SIZE = 0x1800;

namespace {

enum class RemoteplayState : std::uint32_t {
 None,
 Terminated,
 Initialized,
};

std::mutex g_stateMutex;
RemoteplayState g_state = RemoteplayState::None;

}

extern "C" {

int APS5_VABI sceRemoteplayIsRemoteViewering() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceRemoteplayNotifyCanWakeup() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceRemoteplayRegistCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceRemoteplayUnregistCallback() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceRemoteplayGetConnectionStatus(int user_id, int* status) {
 (void)user_id;
 std::lock_guard lock(g_stateMutex);
 if (g_state != RemoteplayState::Initialized) return REMOTEPLAY_ERROR_NOT_INITIALIZED;
 if (!status) return REMOTEPLAY_ERROR_INVALID_ARG;
 *status = REMOTEPLAY_CONNECTION_STATUS_DISCONNECT;
 return 0;
}

int APS5_VABI sceRemoteplayInitialize(void* heap, size_t heap_size) {
 std::lock_guard lock(g_stateMutex);
 if (g_state == RemoteplayState::Initialized) return REMOTEPLAY_ERROR_ALREADY_INITIALIZED;
 if (heap && heap_size < REMOTEPLAY_MINIMUM_HEAP_SIZE) return REMOTEPLAY_ERROR_INVALID_ARG;
 g_state = RemoteplayState::Initialized;
 return 0;
}

int APS5_VABI sceRemoteplayTerminate(void) {
 std::lock_guard lock(g_stateMutex);
 if (g_state != RemoteplayState::Initialized) return REMOTEPLAY_ERROR_NOT_INITIALIZED;
 g_state = RemoteplayState::Terminated;
 return 0;
}

int APS5_VABI sceRemoteplayApprove(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayClearAllRegistData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayClearConnectHistory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayConfirmDeviceRegist(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayDisconnect(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGeneratePinCode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetApMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetConnectHistory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetConnectUserId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetMbusDeviceInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetOperationStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetPrimaryUserId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetRemoteplayStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayGetRpMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeFilterResult(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeGetEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeNotify(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeNotifyEventResult(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeSetCaret(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayImeSetText(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayIsRemoteOskReady(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayIsRemotePlaying(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifyMbusDeviceRegistComplete(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifyNpPushWakeup(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifyPinCodeError(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifySubmitCode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayNotifyUserDelete(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayPrintAllRegistData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayProhibit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayProhibitStreaming(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayServerLock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplayServerUnLock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetApMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetLogLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetProhibition(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetProhibitionForVsh(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRemoteplaySetRpMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
