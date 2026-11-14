#include "prx/libSceVideoStreamingEngine/libSceVideoStreamingEngine.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x8178000Au);
    for (int call = 0; call < 2; ++call) {
        Require(sceVideoStreamingEngineAbortAuxMessage() == notInitialized);
        Require(sceVideoStreamingEngineAbortParsing() == notInitialized);
        Require(sceVideoStreamingEngineAddSourceBuffer() == notInitialized);
        Require(sceVideoStreamingEngineAppendBuffer() == notInitialized);
        Require(sceVideoStreamingEngineCancelThumbnail() == notInitialized);
        Require(sceVideoStreamingEngineClearLiveSeekableRange() == notInitialized);
        Require(sceVideoStreamingEngineCreatePlayer() == notInitialized);
        Require(sceVideoStreamingEngineDeleteMediaSample() == notInitialized);
        Require(sceVideoStreamingEngineDeletePlayer() == notInitialized);
        Require(sceVideoStreamingEngineDivideMediaSample() == notInitialized);
        Require(sceVideoStreamingEngineEnableAudioTrack() == notInitialized);
        Require(sceVideoStreamingEngineEndOfStream() == notInitialized);
        Require(sceVideoStreamingEngineEnqueueMediaSample() == notInitialized);
        Require(sceVideoStreamingEngineGenerateThumbnail() == notInitialized);
        Require(sceVideoStreamingEngineGetAudioFrame() == notInitialized);
        Require(sceVideoStreamingEngineGetAudioTrackInfo() == notInitialized);
        Require(sceVideoStreamingEngineGetBufferedInfo() == notInitialized);
        Require(sceVideoStreamingEngineGetDuration() == notInitialized);
        Require(sceVideoStreamingEngineGetLoadingProgress() == notInitialized);
        Require(sceVideoStreamingEngineGetLoop() == notInitialized);
        Require(sceVideoStreamingEngineGetMediaKeys() == notInitialized);
        Require(sceVideoStreamingEngineGetPlaybackRange() == notInitialized);
        Require(sceVideoStreamingEngineGetPlaybackRate() == notInitialized);
        Require(sceVideoStreamingEngineGetResolution() == notInitialized);
        Require(sceVideoStreamingEngineGetTimeProperty() == notInitialized);
        Require(sceVideoStreamingEngineGetVideoTrackInfo() == notInitialized);
        Require(sceVideoStreamingEngineIsLiveStreaming() == notInitialized);
        Require(sceVideoStreamingEngineLoad() == notInitialized);
        Require(sceVideoStreamingEnginePause() == notInitialized);
        Require(sceVideoStreamingEnginePlay() == notInitialized);
        Require(sceVideoStreamingEngineReleaseThumbnailData() == notInitialized);
        Require(sceVideoStreamingEngineRemoveSourceBuffer() == notInitialized);
        Require(sceVideoStreamingEngineSeek() == notInitialized);
        Require(sceVideoStreamingEngineSelectVideoTrack() == notInitialized);
        Require(sceVideoStreamingEngineSendAuxMessageForRepeatedResults() == notInitialized);
        Require(sceVideoStreamingEngineSendAuxMessageSync() == notInitialized);
        Require(sceVideoStreamingEngineSetDuration() == notInitialized);
        Require(sceVideoStreamingEngineSetLiveSeekableRange() == notInitialized);
        Require(sceVideoStreamingEngineSetLoop() == notInitialized);
        Require(sceVideoStreamingEngineSetMediaKeys() == notInitialized);
        Require(sceVideoStreamingEngineSetPlaybackRange() == notInitialized);
        Require(sceVideoStreamingEngineSetPlaybackRate() == notInitialized);
        Require(sceVideoStreamingEngineSetPtsDtsToMediaSample() == notInitialized);
        Require(sceVideoStreamingEngineStop() == notInitialized);
        Require(sceVideoStreamingEngineStopDecode() == notInitialized);
        Require(sceVideoStreamingEngineUpdateByteStreamFormatSpecification() == notInitialized);
    }
    return 0;
}
