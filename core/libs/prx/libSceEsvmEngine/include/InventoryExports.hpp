#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceEsvmEngineAddDecoder();
int APS5_VABI sceEsvmEngineCreateContext();
int APS5_VABI sceEsvmEngineDbgOverrideMediaKeySession();
int APS5_VABI sceEsvmEngineDeleteContext();
int APS5_VABI sceEsvmEngineFlush();
int APS5_VABI sceEsvmEngineGetAudioFrame();
int APS5_VABI sceEsvmEngineGetPlaybackRate();
int APS5_VABI sceEsvmEngineGetProvisioningRequest();
int APS5_VABI sceEsvmEngineGetServiceCertificateRequest();
int APS5_VABI sceEsvmEngineGetVideoFrame();
int APS5_VABI sceEsvmEngineHandleProvisioningResponse();
int APS5_VABI sceEsvmEngineInitialize();
int APS5_VABI sceEsvmEngineIsProvisioned();
int APS5_VABI sceEsvmEngineMediaKeySystemAccessCreateMediaKeys2();
int APS5_VABI sceEsvmEngineMediaKeySystemAccessGetKeySystem();
int APS5_VABI sceEsvmEngineParseAndLoadServerCertificateResponse();
int APS5_VABI sceEsvmEnginePause();
int APS5_VABI sceEsvmEnginePlay();
int APS5_VABI sceEsvmEnginePushAccessUnit();
int APS5_VABI sceEsvmEngineQueryVideoMemory();
int APS5_VABI sceEsvmEngineRemoveDecoder();
int APS5_VABI sceEsvmEngineSetPlaybackRate();
int APS5_VABI sceEsvmEngineSetServerCertificate2();
int APS5_VABI sceEsvmEngineTerminate();
int APS5_VABI libSceEsvmEngineUnknown_TLCfQqZ_minus_6AU_nid_no_patch();
int APS5_VABI libSceEsvmEngineUnknown_Y6Rtwfuj0Bw_nid_no_patch();

}
