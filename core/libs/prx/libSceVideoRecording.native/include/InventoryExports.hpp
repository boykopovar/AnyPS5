#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoRecordingCopyBGRAtoP010();
int APS5_VABI sceVideoRecordingDisableScanInEncode();
int APS5_VABI sceVideoRecordingEnableScanInEncode();
int APS5_VABI sceVideoRecordingFinalizeByAppId();
int APS5_VABI sceVideoRecordingFinalizeByHandle();
int APS5_VABI sceVideoRecordingGetCountScanInAction();
int APS5_VABI sceVideoRecordingGetFinalizeActiveInfo();
int APS5_VABI sceVideoRecordingGetFormatByAppId();
int APS5_VABI sceVideoRecordingGetFormatExtensionByAppId();
int APS5_VABI sceVideoRecordingGetRecordedDurationByAppId();
int APS5_VABI sceVideoRecordingGetRecordedDurationByHandle();
int APS5_VABI sceVideoRecordingGetStatusByAppId();
int APS5_VABI sceVideoRecordingGetStatusByHandle();
int APS5_VABI sceVideoRecordingIsProfileChangeSupported();
int APS5_VABI sceVideoRecordingOpenByRAFeatureId();
int APS5_VABI sceVideoRecordingResetRecordedData();
int APS5_VABI sceVideoRecordingRestModeProcessStop();
int APS5_VABI sceVideoRecordingSetCopyrightImage();
int APS5_VABI sceVideoRecordingSetInfoByHandle();
int APS5_VABI sceVideoRecordingSetPrivacyImage();
int APS5_VABI sceVideoRecordingSetProfileLimit();
int APS5_VABI sceVideoRecordingSetResourceStateByHandle();
int APS5_VABI sceVideoRecordingStartByHandle();
int APS5_VABI sceVideoRecordingStopByHandle();
int APS5_VABI libSceVideoRecording_nativeUnknown_26XG0o9fx8Q_nid_no_patch();
int APS5_VABI libSceVideoRecording_nativeUnknown_WT4aTfe5OYk_nid_no_patch();
int APS5_VABI libSceVideoRecording_nativeUnknown_iYtbtBx4jvE_nid_no_patch();

}
