#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceEsvmEngine/include/InventoryExports.hpp"
#include "prx/libSceEsvmEngine/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED = static_cast<int>(0x8178000A);

}

extern "C" {

int APS5_VABI sceEsvmEngineAddDecoder2() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineAppAudioSyncFlush() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineGetMetrics() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineGetVideoPlaybackQuality() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineInitializeCdm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineMediaKeySessionDecryptAudioData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineMediaKeysDecryptAudioData() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineMediaKeysSetAppParameter() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineProcessAckResponse() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineReleaseVideoFrame() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineSetAudiblePlaybackRate() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineSetMediaKeys() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineSyncVideo() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineDestroyMediaKeySystemAccess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySessionClose() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySessionGenerateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySessionGetExpiration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySessionGetKeyStatuses() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySessionGetSessionId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySessionLoad() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySessionRemove() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySessionSetEventHandler() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySessionUpdate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySystemAccessCreateMediaKeys() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySystemAccessDestroyMediaKeys() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySystemAccessGetConfiguration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeySystemAccessSetCdmStorageConfig() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeysCreateSession() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeysDestroySession() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeysSetEventHandler() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineMediaKeysSetServerCertificate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceEsvmEngineRequestMediaKeySystemAccess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceEsvmEngineAddDecoder() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineCreateContext() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineDbgOverrideMediaKeySession() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineDeleteContext() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineFlush() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineGetAudioFrame() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineGetPlaybackRate() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineGetProvisioningRequest() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineGetServiceCertificateRequest() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineGetVideoFrame() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineHandleProvisioningResponse() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineIsProvisioned() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineMediaKeySystemAccessCreateMediaKeys2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineMediaKeySystemAccessGetKeySystem() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineParseAndLoadServerCertificateResponse() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEnginePause() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEnginePlay() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEnginePushAccessUnit() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineQueryVideoMemory() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineRemoveDecoder() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineSetPlaybackRate() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceEsvmEngineSetServerCertificate2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceEsvmEngineTerminate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceEsvmEngineUnknown_TLCfQqZ_minus_6AU_nid_no_patch() {
    return SCE_ESVM_ENGINE_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("TLCfQqZ-6AU", libSceEsvmEngineUnknown_TLCfQqZ_minus_6AU_nid_no_patch);

int APS5_VABI libSceEsvmEngineUnknown_Y6Rtwfuj0Bw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Y6Rtwfuj0Bw", libSceEsvmEngineUnknown_Y6Rtwfuj0Bw_nid_no_patch);

}
