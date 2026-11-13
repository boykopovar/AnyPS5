#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceMusicFrameworkInterface/include/InventoryExports.hpp"
#include "prx/libSceMusicFrameworkInterface/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81797001u);

} // namespace

extern "C" {

int APS5_VABI sceMusicFwGetCurrentTime(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwGetLoop(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwGetMuted(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwGetNetworkState(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwGetVolume(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwIsEnded(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwIsPlaying(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwIsStopped(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwRemoveSrc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMusicFwSendMessageAsynchronously() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMusicFwSendMessageSynchronously() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMusicFwSetCurrentTime() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMusicFwSetLoop() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMusicFwSetShuffle() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMusicFwSetVolume() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMusicFwSkipNext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceMusicFwSkipPrevious() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceMusicFwAddSrc() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwFinalize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwGetCapability(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwGetDuration(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwGetShuffle(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwGetVolume2(void* output) {
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwInitialize2(const void* param) {
    if (param == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwIsPaused(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwIsSeeking(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwLoad() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwPause() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwPlay() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwSetMuted() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwSetVolume2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwStop() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceMusicFrameworkInterfaceUnknown_X9GN6gWabWM_nid_no_patch(std::int32_t handle, void* output) {
    (void)handle;
    if (output == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("X9GN6gWabWM", libSceMusicFrameworkInterfaceUnknown_X9GN6gWabWM_nid_no_patch);

int APS5_VABI sceMusicFwRegisterNotifyMessage() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwUnregisterNotifyMessage() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceMusicFwAbortMessage() {
    NotImplemented_nid_no_patch(__func__);
}

}
