#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI FinalizeMediaModule();
int APS5_VABI ForbidCopyrightProtectedContents();
int APS5_VABI GetCapabilities();
int APS5_VABI GetCurrentSrcId();
int APS5_VABI GetCurrentTime();
int APS5_VABI GetMediaPlaybackInfo();
int APS5_VABI GetMute();
int APS5_VABI GetServiceType();
int APS5_VABI GetServiceTypeFromTitleId();
int APS5_VABI GetSrcIdList();
int APS5_VABI GetUsbDeviceListArray();
int APS5_VABI GetVolume();
int APS5_VABI InitializeMediaModule();
int APS5_VABI IsSrcIdPlaying();
int APS5_VABI MASSMADeleteMessage();
int APS5_VABI MASSMAInitialize();
int APS5_VABI MASSMASendMessage();
int APS5_VABI MASSMATerminate();
int APS5_VABI MASSMCDeleteMessage();
int APS5_VABI MASSMCReadMessage();
int APS5_VABI MASSMCTerminate();
int APS5_VABI MASSValueSizeMax();
int APS5_VABI PlayUri();
int APS5_VABI PlaybackCommand();
int APS5_VABI RemoveMediaSource();
int APS5_VABI Seek_nid_postfix();
int APS5_VABI ServiceSupported();
int APS5_VABI SetCurrentSrcId();
int APS5_VABI SetCurrentTime();
int APS5_VABI SetMute();
int APS5_VABI SetServiceType();
int APS5_VABI SetVolume();
int APS5_VABI addProhibitionFlagListener();
int APS5_VABI addTelemetryErrorListener();
int APS5_VABI getDecryptionKeys();
int APS5_VABI getProhibitionFlags();
int APS5_VABI removeProhibitionFlagListener();
int APS5_VABI removeTelemetryErrorListener();
int APS5_VABI setVideoMode();


int APS5_VABI libReactNativeMediaInventory_BIdvn6c_minus_zAw_nid_no_patch();
int APS5_VABI libReactNativeMediaInventory_BwF_minus_lBIdW5g_nid_no_patch();
int APS5_VABI libReactNativeMediaInventory_JwW_plus_jnYDAcI_nid_no_patch();
int APS5_VABI libReactNativeMediaInventory_QWwXJ_minus_KSWg4_nid_no_patch();
int APS5_VABI libReactNativeMediaInventory_S3lY_minus_8Af_plus_rw_nid_no_patch();
int APS5_VABI libReactNativeMediaInventory_Xn1h2KUpUN4_nid_no_patch();
int APS5_VABI libReactNativeMediaInventory_kYVOipktdO0_nid_no_patch();
int APS5_VABI libReactNativeMediaInventory_m4NzcIr_minus_JJk_nid_no_patch();
int APS5_VABI libReactNativeMediaInventory_zj7_minus_XSRULNw_nid_no_patch();

}
