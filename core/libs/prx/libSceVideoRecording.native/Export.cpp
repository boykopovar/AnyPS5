#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVideoRecording.native/include/InventoryExports.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x80a80003u);

} // namespace

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI sceVideoRecordingCopyBGRAtoP010() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingDisableScanInEncode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingEnableScanInEncode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingFinalizeByAppId(std::int32_t appId, std::uint32_t argument2) {
    if (appId == -1 || argument2 >= 3) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingFinalizeByHandle(const void* handle, std::uint32_t argument2) {
    if (handle == nullptr || argument2 >= 3) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingGetCountScanInAction(void* count) {
    if (count == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingGetFinalizeActiveInfo(void* info) {
    if (info == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingGetFormatByAppId(std::int32_t appId) {
    if (appId == -1) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingGetFormatExtensionByAppId(std::int32_t appId, char* extension, std::size_t size) {
    if (appId == -1 || extension == nullptr || size < 5) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingGetRecordedDurationByAppId(std::int32_t appId) {
    if (appId == -1) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingGetRecordedDurationByHandle(const void* handle) {
    if (handle == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingGetStatusByAppId(std::int32_t appId) {
    if (appId == -1) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingGetStatusByHandle(const void* handle) {
    if (handle == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingIsProfileChangeSupported(void* supported) {
    if (supported == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingOpenByRAFeatureId() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingResetRecordedData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingRestModeProcessStop() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingSetCopyrightImage() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingSetInfoByHandle(const void* handle) {
    if (handle == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingSetPrivacyImage(const void*, const void*, std::uint32_t mode) {
    if (mode > 2) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingSetProfileLimit(std::uint32_t limit) {
    if (limit > 1) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingSetResourceStateByHandle(const void* handle, std::uint32_t state) {
    if (handle == nullptr || state >= 2) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingStartByHandle(const void* handle) {
    if (handle == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoRecordingStopByHandle(const void* handle) {
    if (handle == nullptr) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceVideoRecording_nativeUnknown_26XG0o9fx8Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("26XG0o9fx8Q", libSceVideoRecording_nativeUnknown_26XG0o9fx8Q_nid_no_patch);

int APS5_VABI libSceVideoRecording_nativeUnknown_WT4aTfe5OYk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WT4aTfe5OYk", libSceVideoRecording_nativeUnknown_WT4aTfe5OYk_nid_no_patch);

int APS5_VABI libSceVideoRecording_nativeUnknown_iYtbtBx4jvE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iYtbtBx4jvE", libSceVideoRecording_nativeUnknown_iYtbtBx4jvE_nid_no_patch);

}
