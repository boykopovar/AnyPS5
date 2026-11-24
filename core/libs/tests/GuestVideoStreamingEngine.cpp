#include "prx/libSceVideoStreamingEngine/libSceVideoStreamingEngine.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool value) {
    if (!value) std::abort();
}

void CheckQueryVideoMemoryCase(std::uint32_t mode, std::uint8_t flagA, std::uint8_t flagB,
                               const std::array<std::uint64_t, 6>& expected) {
    constexpr int ok = 0;
    std::array<std::uint8_t, 16> param{};
    std::memcpy(param.data() + 0x8, &mode, sizeof(mode));
    param[0xc] = flagA;
    param[0xd] = flagB;
    std::array<std::uint64_t, 6> out{};
    out.fill(0xA5A5A5A5A5A5A5A5ull);
    Require(sceVideoStreamingEngineQueryVideoMemory(param.data(), out.data()) == ok);
    Require(out == expected);
}

} // namespace

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);

    constexpr int invalidArgument = static_cast<int>(0x81780001u);
    constexpr int mediaKeyNotInitialized = static_cast<int>(0x81780002u);
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
        Require(sceVideoStreamingEngineIsTransparentVideoMode() == notInitialized);
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

        Require(sceVideoStreamingEngineDestroyMediaKeySystemAccess() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySessionClose() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySessionGenerateRequest() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySessionGetExpiration() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySessionGetKeyStatuses() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySessionGetSessionId() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySessionLoad() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySessionRemove() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySessionSetEventHandler() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySessionUpdate() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySystemAccessCreateMediaKeys() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySystemAccessDestroyMediaKeys() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySystemAccessGetConfiguration() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySystemAccessGetKeySystem() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeySystemAccessSetCdmStorageConfig() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeysCreateSession() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeysDestroySession() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineMediaKeysSetServerCertificate() == mediaKeyNotInitialized);
        Require(sceVideoStreamingEngineRequestMediaKeySystemAccess() == mediaKeyNotInitialized);
    }

    Require(sceVideoStreamingEngineQueryInitEsvmMemory(1, nullptr) == invalidArgument);
    errno = 71;
    Require(sceVideoStreamingEngineQueryInitEsvmMemory(1, reinterpret_cast<void*>(0x10)) == invalidArgument);
    Require(errno == 71);

    for (const std::uint32_t mode : {0u, 2u}) {
        std::array<std::uint64_t, 2> esvm{0xA5A5A5A5A5A5A5A5ull, 0xA5A5A5A5A5A5A5A5ull};
        Require(sceVideoStreamingEngineQueryInitEsvmMemory(mode, esvm.data()) == 0);
        Require(esvm[0] == 0 && esvm[1] == 0);
    }
    {
        std::array<std::uint64_t, 2> esvm{};
        Require(sceVideoStreamingEngineQueryInitEsvmMemory(1, esvm.data()) == 0);
        Require(esvm[0] == 0x800000ull && esvm[1] == 0x200000ull);
    }
    {
        std::array<std::uint64_t, 2> esvm{};
        Require(sceVideoStreamingEngineQueryInitEsvmMemory(4, esvm.data()) == 0);
        Require(esvm[0] == 0x1000000ull && esvm[1] == 0x200000ull);
    }

    std::array<std::uint8_t, 16> validParam{};
    std::array<std::uint64_t, 6> videoOut{};
    videoOut.fill(0xA5A5A5A5A5A5A5A5ull);
    Require(sceVideoStreamingEngineQueryVideoMemory(nullptr, videoOut.data()) == invalidArgument);
    Require(sceVideoStreamingEngineQueryVideoMemory(validParam.data(), nullptr) == invalidArgument);
    for (const std::uint64_t word : videoOut) Require(word == 0xA5A5A5A5A5A5A5A5ull);

    errno = 71;
    Require(sceVideoStreamingEngineQueryVideoMemory(reinterpret_cast<const void*>(0x10), videoOut.data()) == invalidArgument);
    Require(errno == 71);
    errno = 71;
    Require(sceVideoStreamingEngineQueryVideoMemory(validParam.data(), reinterpret_cast<void*>(0x10)) == invalidArgument);
    Require(errno == 71);

    const std::uint32_t badMode = 2u;
    std::memcpy(validParam.data() + 0x8, &badMode, sizeof(badMode));
    Require(sceVideoStreamingEngineQueryVideoMemory(validParam.data(), videoOut.data()) == invalidArgument);
    for (const std::uint64_t word : videoOut) Require(word == 0xA5A5A5A5A5A5A5A5ull);

    CheckQueryVideoMemoryCase(1, 1, 0, {0x1b00000ull, 0x4000ull, 0, 0, 0x17400000ull, 0x200000ull});
    CheckQueryVideoMemoryCase(0, 1, 0, {0x1600000ull, 0x4000ull, 0, 0, 0x8400000ull, 0x200000ull});
    CheckQueryVideoMemoryCase(1, 0, 1, {0x15000000ull, 0x4000ull, 0x4c84000ull, 0x4000ull, 0, 0});
    CheckQueryVideoMemoryCase(1, 0, 0, {0x9c00000ull, 0x4000ull, 0x10100000ull, 0x4000ull, 0, 0});
    CheckQueryVideoMemoryCase(0, 0, 1, {0x7a00000ull, 0x4000ull, 0x1324000ull, 0x4000ull, 0, 0});
    CheckQueryVideoMemoryCase(0, 0, 0, {0x3f00000ull, 0x4000ull, 0x4f00000ull, 0x4000ull, 0, 0});

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
