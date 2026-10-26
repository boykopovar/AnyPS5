#pragma once

#include "SceTypes.hpp"

extern "C" {

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
