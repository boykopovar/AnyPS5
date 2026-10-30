#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceEsvmEngineAddDecoder2();
int APS5_VABI sceEsvmEngineAppAudioSyncFlush();
int APS5_VABI sceEsvmEngineGetMetrics();
int APS5_VABI sceEsvmEngineGetVideoPlaybackQuality();
int APS5_VABI sceEsvmEngineInitializeCdm();
int APS5_VABI sceEsvmEngineMediaKeySessionDecryptAudioData();
int APS5_VABI sceEsvmEngineMediaKeysDecryptAudioData();
int APS5_VABI sceEsvmEngineMediaKeysSetAppParameter();
int APS5_VABI sceEsvmEngineProcessAckResponse();
int APS5_VABI sceEsvmEngineReleaseVideoFrame();
int APS5_VABI sceEsvmEngineSetAudiblePlaybackRate();
int APS5_VABI sceEsvmEngineSetMediaKeys();
int APS5_VABI sceEsvmEngineSyncVideo();

int APS5_VABI sceEsvmEngineDestroyMediaKeySystemAccess();
int APS5_VABI sceEsvmEngineMediaKeySessionClose();
int APS5_VABI sceEsvmEngineMediaKeySessionGenerateRequest();
int APS5_VABI sceEsvmEngineMediaKeySessionGetExpiration();
int APS5_VABI sceEsvmEngineMediaKeySessionGetKeyStatuses();
int APS5_VABI sceEsvmEngineMediaKeySessionGetSessionId();
int APS5_VABI sceEsvmEngineMediaKeySessionLoad();
int APS5_VABI sceEsvmEngineMediaKeySessionRemove();
int APS5_VABI sceEsvmEngineMediaKeySessionSetEventHandler();
int APS5_VABI sceEsvmEngineMediaKeySessionUpdate();
int APS5_VABI sceEsvmEngineMediaKeySystemAccessCreateMediaKeys();
int APS5_VABI sceEsvmEngineMediaKeySystemAccessDestroyMediaKeys();
int APS5_VABI sceEsvmEngineMediaKeySystemAccessGetConfiguration();
int APS5_VABI sceEsvmEngineMediaKeySystemAccessSetCdmStorageConfig();
int APS5_VABI sceEsvmEngineMediaKeysCreateSession();
int APS5_VABI sceEsvmEngineMediaKeysDestroySession();
int APS5_VABI sceEsvmEngineMediaKeysSetEventHandler();
int APS5_VABI sceEsvmEngineMediaKeysSetServerCertificate();
int APS5_VABI sceEsvmEngineRequestMediaKeySystemAccess();

}
