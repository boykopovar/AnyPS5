#include "prx/libSceEsvmEngine/libSceEsvmEngine.hpp"
#include "prx/libSceEsvmEngine/include/InventoryExports.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x8178000Au);
    constexpr int nullArgument = static_cast<int>(0x81780001u);
    for (int call = 0; call < 2; ++call) {
        Require(libSceEsvmEngineUnknown_TLCfQqZ_minus_6AU_nid_no_patch() == notInitialized);
        Require(sceEsvmEngineAddDecoder() == notInitialized);
        Require(sceEsvmEngineAddDecoder2() == notInitialized);
        Require(sceEsvmEngineAppAudioSyncFlush() == notInitialized);
        Require(sceEsvmEngineCreateContext() == notInitialized);
        Require(sceEsvmEngineDbgOverrideMediaKeySession() == notInitialized);
        Require(sceEsvmEngineDeleteContext() == notInitialized);
        Require(sceEsvmEngineFlush() == notInitialized);
        Require(sceEsvmEngineGetAudioFrame() == notInitialized);
        Require(sceEsvmEngineGetPlaybackRate() == notInitialized);
        Require(sceEsvmEngineGetVideoFrame() == notInitialized);
        Require(sceEsvmEngineGetVideoPlaybackQuality() == notInitialized);
        Require(sceEsvmEnginePause() == notInitialized);
        Require(sceEsvmEnginePlay() == notInitialized);
        Require(sceEsvmEnginePushAccessUnit() == notInitialized);
        Require(sceEsvmEngineReleaseVideoFrame() == notInitialized);
        Require(sceEsvmEngineRemoveDecoder() == notInitialized);
        Require(sceEsvmEngineSetAudiblePlaybackRate() == notInitialized);
        Require(sceEsvmEngineSetMediaKeys() == notInitialized);
        Require(sceEsvmEngineSetPlaybackRate() == notInitialized);
        Require(sceEsvmEngineSyncVideo() == notInitialized);
        Require(sceEsvmEngineInitializeCdm(nullptr) == nullArgument);
        Require(sceEsvmEngineQueryVideoMemory(nullptr) == nullArgument);
        Require(libSceEsvmEngineUnknown_Y6Rtwfuj0Bw_nid_no_patch(nullptr) == nullArgument);
    }
    return 0;
}
