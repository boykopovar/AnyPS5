#include "prx/libSceVideoCoreServerInterface/libSceVideoCoreServerInterface.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "VIDEO CORE UNAVAILABLE: %s\n", message);
    std::abort();
}

} // namespace

int main() {
    const std::array<decltype(&sceVideoCoreAbortRequest), 43> calls{
        sceVideoCoreAbortRequest, sceVideoCoreAddStream, sceVideoCoreAsyncAddStream,
        sceVideoCoreAsyncSendRequest, sceVideoCoreCancelThumbnail, sceVideoCoreChangeActiveStatus,
        sceVideoCoreChangeAudioStream, sceVideoCoreChangeDisplaySize, sceVideoCoreChangeHdcpSetting,
        sceVideoCoreChangeSubtitle, sceVideoCoreDeleteStream, sceVideoCoreGenerateThumbnail,
        sceVideoCoreGenerateThumbnail2, sceVideoCoreGetCanvasHandle, sceVideoCoreGetPlayStatus,
        sceVideoCoreGetPlayStatus2, sceVideoCoreGetStreamUid, sceVideoCoreGetThumbnail,
        sceVideoCoreGetThumbnail2, sceVideoCoreLaunchVideoCore, sceVideoCoreMediaSourceAbortSourceBuffer,
        sceVideoCoreMediaSourceAddSourceBuffer, sceVideoCoreMediaSourceAppendData, sceVideoCoreMediaSourceEndOfStream,
        sceVideoCoreMediaSourceGetBufferedTimeRanges, sceVideoCoreMediaSourceGetInputBufferEmptySize, sceVideoCoreMediaSourceGetPlaybackQuality,
        sceVideoCoreMediaSourceGetSourceBufferEvent, sceVideoCoreMediaSourceRemoveSourceBuffer, sceVideoCoreMediaSourceSetDuration,
        sceVideoCoreMediaSourceSetTimestampOffset, sceVideoCorePlayControl, sceVideoCoreRegistEventCallback,
        sceVideoCoreSetAudioMute, sceVideoCoreSetAudioVolume, sceVideoCoreSetFullScreen,
        sceVideoCoreSetInitialInfo, sceVideoCoreSetInitializeInfo, sceVideoCoreSetPlaybackRange,
        sceVideoCoreSetThumbnailInfo, sceVideoCoreSyncAddStream, sceVideoCoreSyncSendRequest,
        sceVideoCoreUnregistEventCallback};
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto call : calls) {
            Require(call() == 0, "an unavailable VideoCore call must answer 0");
        }
    }
}
