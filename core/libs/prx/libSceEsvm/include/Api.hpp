#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceEsvmAppAudioSyncCreateContext();
int APS5_VABI sceEsvmAppAudioSyncFlush();
int APS5_VABI sceEsvmSyncVideo();

int APS5_VABI sceEsvmAddDecoder();
int APS5_VABI sceEsvmAddDecoder2();
int APS5_VABI sceEsvmCheckCallback();
int APS5_VABI sceEsvmCreateContext();
int APS5_VABI sceEsvmCreateMediaKeySession();
int APS5_VABI sceEsvmCreateMediaKeys();
int APS5_VABI sceEsvmDestroyContext();
int APS5_VABI sceEsvmDestroyMediaKeySession();
int APS5_VABI sceEsvmDestroyMediaKeySystemAccess();
int APS5_VABI sceEsvmDestroyMediaKeys();
int APS5_VABI sceEsvmFlush();
int APS5_VABI sceEsvmInitialize();
int APS5_VABI sceEsvmMediaKeySessionClose();
int APS5_VABI sceEsvmMediaKeySessionDecryptAudioData();
int APS5_VABI sceEsvmMediaKeySessionGenerateRequest();
int APS5_VABI sceEsvmMediaKeySessionGetExpiration();
int APS5_VABI sceEsvmMediaKeySessionGetKeyStatuses();
int APS5_VABI sceEsvmMediaKeySessionGetSessionId();
int APS5_VABI sceEsvmMediaKeySessionLoad();
int APS5_VABI sceEsvmMediaKeySessionRemove();
int APS5_VABI sceEsvmMediaKeySessionUpdate();
int APS5_VABI sceEsvmMute();
int APS5_VABI sceEsvmPause();
int APS5_VABI sceEsvmPlay();
int APS5_VABI sceEsvmPushAccessUnit();
int APS5_VABI sceEsvmReleaseVideoFrame();
int APS5_VABI sceEsvmRender();
int APS5_VABI sceEsvmRenderById();
int APS5_VABI sceEsvmSetAudiblePlaybackRate();
int APS5_VABI sceEsvmSetCodecConfig();
int APS5_VABI sceEsvmSetPlaybackRate();
int APS5_VABI sceEsvmSetVolume();
int APS5_VABI sceEsvmTerminate();
int APS5_VABI sceEsvmVideoOutAllowOutputResolutionWqhdDetection();
int APS5_VABI sceEsvmVideoOutConfigureOutputEx();
int APS5_VABI sceEsvmVideoOutGetMonitorInfo();
int APS5_VABI sceEsvmVideoOutGetOutputStatus();
int APS5_VABI sceEsvmVideoOutInitializeOutputOptions();
int APS5_VABI sceEsvmVideoOutIsHdcp14Enabled();
int APS5_VABI sceEsvmVideoOutIsHdcp23Enabled();
int APS5_VABI sceEsvmVideoOutIsOutputSupportedByMonitorInfo();
int APS5_VABI sceEsvmVideoOutIsPortDeviceConnected();
int APS5_VABI sceEsvmVideoOutIsPortStatusUsable();
int APS5_VABI sceEsvmVideoOutSetContentDynamicRange();
int APS5_VABI sceEsvmVideoOutSetContentType();


}
