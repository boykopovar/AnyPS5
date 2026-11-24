#include "prx/libSceVideoStreamingEngine/include/Api.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_VIDEO_STREAMING_ENGINE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81780001u);
constexpr int SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED = static_cast<int>(0x81780002u);
constexpr int SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED = static_cast<int>(0x8178000Au);

template <typename T>
bool TryRead(const void* base, std::size_t offset, T& out) {
    if (base == nullptr) {
        return false;
    }
    const auto addr = reinterpret_cast<std::uintptr_t>(base);
    if (offset > UINTPTR_MAX - addr) {
        return false;
    }
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&out, reinterpret_cast<const void*>(addr + offset), sizeof(out));
    errno = savedErrno;
    return ok;
}

bool TryWrite(void* dst, const void* src, std::size_t size) {
    if (dst == nullptr) {
        return false;
    }
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = savedErrno;
    return ok;
}

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
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineLoad() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionClose() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionRemove() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionUpdate() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessGetConfiguration() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeysDestroySession() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeysSetServerCertificate() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEnginePause() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEnginePlay() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineQueryInitEsvmMemory(std::uint32_t mode, void* memoryInfo) {
    if (memoryInfo == nullptr) {
        return SCE_VIDEO_STREAMING_ENGINE_ERROR_INVALID_ARGUMENT;
    }
    std::array<std::uint64_t, 2> result{};
    if (mode == 1u) {
        result = {0x800000ull, 0x200000ull};
    } else if (mode == 4u) {
        result = {0x1000000ull, 0x200000ull};
    }
    return TryWrite(memoryInfo, result.data(), sizeof(result))
        ? 0
        : SCE_VIDEO_STREAMING_ENGINE_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceVideoStreamingEngineQueryVideoMemory(const void* param, void* memoryInfo) {
    if (param == nullptr || memoryInfo == nullptr) {
        return SCE_VIDEO_STREAMING_ENGINE_ERROR_INVALID_ARGUMENT;
    }
    std::uint32_t mode = 0;
    std::uint8_t flagA = 0;
    std::uint8_t flagB = 0;
    if (!TryRead(param, 0x8, mode) || !TryRead(param, 0xc, flagA) || !TryRead(param, 0xd, flagB)) {
        return SCE_VIDEO_STREAMING_ENGINE_ERROR_INVALID_ARGUMENT;
    }
    if (mode > 1u) {
        return SCE_VIDEO_STREAMING_ENGINE_ERROR_INVALID_ARGUMENT;
    }
    std::array<std::uint64_t, 6> result{0, 0x4000ull, 0, 0, 0, 0};
    if (flagA != 0) {
        if (mode == 1u) {
            result = {0x1b00000ull, 0x4000ull, 0, 0, 0x17400000ull, 0x200000ull};
        } else {
            result = {0x1600000ull, 0x4000ull, 0, 0, 0x8400000ull, 0x200000ull};
        }
    } else if (mode == 1u) {
        if (flagB != 0) {
            result = {0x15000000ull, 0x4000ull, 0x4c84000ull, 0x4000ull, 0, 0};
        } else {
            result = {0x9c00000ull, 0x4000ull, 0x10100000ull, 0x4000ull, 0, 0};
        }
    } else if (flagB != 0) {
        result = {0x7a00000ull, 0x4000ull, 0x1324000ull, 0x4000ull, 0, 0};
    } else {
        result = {0x3f00000ull, 0x4000ull, 0x4f00000ull, 0x4000ull, 0, 0};
    }
    return TryWrite(memoryInfo, result.data(), sizeof(result))
        ? 0
        : SCE_VIDEO_STREAMING_ENGINE_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceVideoStreamingEngineReleaseThumbnailData() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineRemoveSourceBuffer() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineRequestMediaKeySystemAccess() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
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
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionGenerateRequest() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionGetExpiration() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionGetKeyStatuses() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionGetSessionId() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionLoad() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySessionSetEventHandler() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessCreateMediaKeys() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessDestroyMediaKeys() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessGetKeySystem() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeySystemAccessSetCdmStorageConfig() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

int APS5_VABI sceVideoStreamingEngineMediaKeysCreateSession() {
    return SCE_VIDEO_STREAMING_ENGINE_ERROR_MEDIA_KEY_NOT_INITIALIZED;
}

}
