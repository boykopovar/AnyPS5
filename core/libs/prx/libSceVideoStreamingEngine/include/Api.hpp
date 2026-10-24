#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoStreamingEngineDestroyMediaKeySystemAccess();
int APS5_VABI sceVideoStreamingEngineMediaKeySessionGenerateRequest();
int APS5_VABI sceVideoStreamingEngineMediaKeySessionGetExpiration();
int APS5_VABI sceVideoStreamingEngineMediaKeySessionGetKeyStatuses();
int APS5_VABI sceVideoStreamingEngineMediaKeySessionGetSessionId();
int APS5_VABI sceVideoStreamingEngineMediaKeySessionLoad();
int APS5_VABI sceVideoStreamingEngineMediaKeySessionSetEventHandler();
int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessCreateMediaKeys();
int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessDestroyMediaKeys();
int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessGetKeySystem();
int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessSetCdmStorageConfig();
int APS5_VABI sceVideoStreamingEngineMediaKeysCreateSession();

}
