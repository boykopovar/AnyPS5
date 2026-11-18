#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

int APS5_VABI sceVideoRecordingCopyBGRAtoP010();
int APS5_VABI sceVideoRecordingDisableScanInEncode();
int APS5_VABI sceVideoRecordingEnableScanInEncode();
int APS5_VABI sceVideoRecordingFinalizeByAppId(std::int32_t appId, std::uint32_t argument2);
int APS5_VABI sceVideoRecordingFinalizeByHandle(const void* handle, std::uint32_t argument2);
int APS5_VABI sceVideoRecordingGetCountScanInAction(void* count);
int APS5_VABI sceVideoRecordingGetFinalizeActiveInfo(void* info);
int APS5_VABI sceVideoRecordingGetFormatByAppId(std::int32_t appId);
int APS5_VABI sceVideoRecordingGetFormatExtensionByAppId(std::int32_t appId, char* extension, std::size_t size);
int APS5_VABI sceVideoRecordingGetRecordedDurationByAppId(std::int32_t appId);
int APS5_VABI sceVideoRecordingGetRecordedDurationByHandle(const void* handle);
int APS5_VABI sceVideoRecordingGetStatusByAppId(std::int32_t appId);
int APS5_VABI sceVideoRecordingGetStatusByHandle(const void* handle);
int APS5_VABI sceVideoRecordingIsProfileChangeSupported(void* supported);
int APS5_VABI sceVideoRecordingOpenByRAFeatureId();
int APS5_VABI sceVideoRecordingResetRecordedData();
int APS5_VABI sceVideoRecordingRestModeProcessStop();
int APS5_VABI sceVideoRecordingSetCopyrightImage();
int APS5_VABI sceVideoRecordingSetInfoByHandle(const void* handle);
int APS5_VABI sceVideoRecordingSetPrivacyImage(const void* argument1, const void* argument2, std::uint32_t mode);
int APS5_VABI sceVideoRecordingSetProfileLimit(std::uint32_t limit);
int APS5_VABI sceVideoRecordingSetResourceStateByHandle(const void* handle, std::uint32_t state);
int APS5_VABI sceVideoRecordingStartByHandle(const void* handle);
int APS5_VABI sceVideoRecordingStopByHandle(const void* handle);
int APS5_VABI libSceVideoRecording_nativeUnknown_26XG0o9fx8Q_nid_no_patch();
int APS5_VABI libSceVideoRecording_nativeUnknown_WT4aTfe5OYk_nid_no_patch();
int APS5_VABI libSceVideoRecording_nativeUnknown_iYtbtBx4jvE_nid_no_patch();

}
