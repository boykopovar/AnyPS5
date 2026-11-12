#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoStreamingEngineAbortAuxMessage();
int APS5_VABI sceVideoStreamingEngineAbortParsing();
int APS5_VABI sceVideoStreamingEngineAddSourceBuffer();
int APS5_VABI sceVideoStreamingEngineAppendBuffer();
int APS5_VABI sceVideoStreamingEngineCancelThumbnail();
int APS5_VABI sceVideoStreamingEngineClearLiveSeekableRange();
int APS5_VABI sceVideoStreamingEngineCreatePlayer();
int APS5_VABI sceVideoStreamingEngineDeleteMediaSample();
int APS5_VABI sceVideoStreamingEngineDeletePlayer();
int APS5_VABI sceVideoStreamingEngineDivideMediaSample();
int APS5_VABI sceVideoStreamingEngineEnableAudioTrack();
int APS5_VABI sceVideoStreamingEngineEndOfStream();
int APS5_VABI sceVideoStreamingEngineEnqueueMediaSample();
int APS5_VABI sceVideoStreamingEngineGenerateThumbnail();
int APS5_VABI sceVideoStreamingEngineGetAudioFrame();
int APS5_VABI sceVideoStreamingEngineGetAudioTrackInfo();
int APS5_VABI sceVideoStreamingEngineGetBufferedInfo();
int APS5_VABI sceVideoStreamingEngineGetDuration();
int APS5_VABI sceVideoStreamingEngineGetLoadingProgress();
int APS5_VABI sceVideoStreamingEngineGetLoop();
int APS5_VABI sceVideoStreamingEngineGetMediaKeys();
int APS5_VABI sceVideoStreamingEngineGetPlaybackRange();
int APS5_VABI sceVideoStreamingEngineGetPlaybackRate();
int APS5_VABI sceVideoStreamingEngineGetResolution();
int APS5_VABI sceVideoStreamingEngineGetTimeProperty();
int APS5_VABI sceVideoStreamingEngineGetVideoTrackInfo();
int APS5_VABI sceVideoStreamingEngineInitialize();
int APS5_VABI sceVideoStreamingEngineIsLiveStreaming();
int APS5_VABI sceVideoStreamingEngineIsTransparentVideoMode();
int APS5_VABI sceVideoStreamingEngineLoad();
int APS5_VABI sceVideoStreamingEngineMediaKeySessionClose();
int APS5_VABI sceVideoStreamingEngineMediaKeySessionRemove();
int APS5_VABI sceVideoStreamingEngineMediaKeySessionUpdate();
int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessGetConfiguration();
int APS5_VABI sceVideoStreamingEngineMediaKeysDestroySession();
int APS5_VABI sceVideoStreamingEngineMediaKeysSetServerCertificate();
int APS5_VABI sceVideoStreamingEnginePause();
int APS5_VABI sceVideoStreamingEnginePlay();
int APS5_VABI sceVideoStreamingEngineQueryInitEsvmMemory();
int APS5_VABI sceVideoStreamingEngineQueryVideoMemory();
int APS5_VABI sceVideoStreamingEngineReleaseThumbnailData();
int APS5_VABI sceVideoStreamingEngineRemoveSourceBuffer();
int APS5_VABI sceVideoStreamingEngineRequestMediaKeySystemAccess();
int APS5_VABI sceVideoStreamingEngineSeek();
int APS5_VABI sceVideoStreamingEngineSelectVideoTrack();
int APS5_VABI sceVideoStreamingEngineSendAuxMessageForRepeatedResults();
int APS5_VABI sceVideoStreamingEngineSendAuxMessageSync();
int APS5_VABI sceVideoStreamingEngineSetDuration();
int APS5_VABI sceVideoStreamingEngineSetLiveSeekableRange();
int APS5_VABI sceVideoStreamingEngineSetLoop();
int APS5_VABI sceVideoStreamingEngineSetMediaKeys();
int APS5_VABI sceVideoStreamingEngineSetPlaybackRange();
int APS5_VABI sceVideoStreamingEngineSetPlaybackRate();
int APS5_VABI sceVideoStreamingEngineSetPtsDtsToMediaSample();
int APS5_VABI sceVideoStreamingEngineStop();
int APS5_VABI sceVideoStreamingEngineStopDecode();
int APS5_VABI sceVideoStreamingEngineTerminate();
int APS5_VABI sceVideoStreamingEngineUpdateByteStreamFormatSpecification();

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
