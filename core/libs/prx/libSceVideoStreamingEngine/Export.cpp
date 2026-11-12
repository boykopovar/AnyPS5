#include "prx/libSceVideoStreamingEngine/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED = static_cast<int>(0x8178000A);

}

extern "C" {

int APS5_VABI sceVideoStreamingEngineAbortAuxMessage() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineAbortParsing() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineAddSourceBuffer() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineAppendBuffer() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineCancelThumbnail() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineClearLiveSeekableRange() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineCreatePlayer() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineDeleteMediaSample() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineDeletePlayer() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineDivideMediaSample() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineEnableAudioTrack() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineEndOfStream() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineEnqueueMediaSample() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGenerateThumbnail() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetAudioFrame() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetAudioTrackInfo() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetBufferedInfo() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetDuration() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetLoadingProgress() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetLoop() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetMediaKeys() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetPlaybackRange() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetPlaybackRate() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetResolution() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetTimeProperty() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineGetVideoTrackInfo() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineIsLiveStreaming() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineIsTransparentVideoMode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineLoad() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionClose() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionRemove() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionUpdate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessGetConfiguration() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineMediaKeysDestroySession() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineMediaKeysSetServerCertificate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEnginePause() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEnginePlay() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineQueryInitEsvmMemory() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineQueryVideoMemory() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineReleaseThumbnailData() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineRemoveSourceBuffer() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineRequestMediaKeySystemAccess() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineSeek() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSelectVideoTrack() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSendAuxMessageForRepeatedResults() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSendAuxMessageSync() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSetDuration() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSetLiveSeekableRange() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSetLoop() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSetMediaKeys() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSetPlaybackRange() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSetPlaybackRate() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineSetPtsDtsToMediaSample() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineStop() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineStopDecode() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineTerminate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVideoStreamingEngineUpdateByteStreamFormatSpecification() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineDestroyMediaKeySystemAccess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionGenerateRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionGetExpiration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionGetKeyStatuses() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionGetSessionId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionLoad() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionSetEventHandler() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessCreateMediaKeys() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessDestroyMediaKeys() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessGetKeySystem() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessSetCdmStorageConfig() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoStreamingEngineMediaKeysCreateSession() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
