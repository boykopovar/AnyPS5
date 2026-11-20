// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceVideoCoreServerInterface/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int VideoCoreErrorInvalidArgument = static_cast<int>(0x80DC0001);
constexpr int VideoCoreErrorBeWrapperResourceNotFound = static_cast<int>(0x80DC000E);
constexpr int VideoCoreErrorResourceNotFound = static_cast<int>(0x80DC0015);

struct DecoderResource {
    bool beWrapper;
    VideoCoreAcquireDecoderResourceParam param;
    VideoCoreDecoderResourceCallback callback;
};

struct DecoderResources {
    std::mutex mutex;
    std::uint32_t nextId = 1;
    std::unordered_map<std::uint32_t, DecoderResource> resources;
};

DecoderResources& Resources() {
    static DecoderResources resources;
    return resources;
}

int AcquireDecoderResource(const VideoCoreAcquireDecoderResourceParam* param, VideoCoreDecoderResourceCallback callback, std::uint32_t* resourceId, bool beWrapper) {
    if (param == nullptr || callback == nullptr || resourceId == nullptr) return VideoCoreErrorInvalidArgument;
    VideoCoreAcquireDecoderResourceParam localParam{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam))) {
        errno = savedErrno;
        return VideoCoreErrorInvalidArgument;
    }
    auto& state = Resources();
    std::lock_guard lock(state.mutex);
    const std::uint32_t id = state.nextId;
    if (!GuestCopyOut_nid_no_patch(resourceId, &id, sizeof(id))) {
        errno = savedErrno;
        return VideoCoreErrorInvalidArgument;
    }
    ++state.nextId;
    errno = savedErrno;
    state.resources[id] = DecoderResource{beWrapper, localParam, callback};
    return 0;
}

int ReleaseDecoderResource(std::uint32_t resourceId, bool beWrapper) {
    const int notFound = beWrapper ? VideoCoreErrorBeWrapperResourceNotFound : VideoCoreErrorResourceNotFound;
    if (resourceId == 0) return notFound;
    auto& state = Resources();
    std::lock_guard lock(state.mutex);
    const auto found = state.resources.find(resourceId);
    if (found == state.resources.end() || found->second.beWrapper != beWrapper) return notFound;
    state.resources.erase(found);
    return 0;
}

}

extern "C" {

int APS5_VABI sceVideoCoreAbortRequest() {
    return 0;
}

int APS5_VABI sceVideoCoreAcquireDecoderResource(const VideoCoreAcquireDecoderResourceParam* param, VideoCoreDecoderResourceCallback callback, std::uint32_t* resourceId) {
    return AcquireDecoderResource(param, callback, resourceId, false);
}

int APS5_VABI sceVideoCoreAcquireDecoderResourceForBeWrapper(const VideoCoreAcquireDecoderResourceParam* param, VideoCoreDecoderResourceCallback callback, std::uint32_t* resourceId) {
    return AcquireDecoderResource(param, callback, resourceId, true);
}

int APS5_VABI sceVideoCoreAddStream() {
    return 0;
}

int APS5_VABI sceVideoCoreAsyncAddStream() {
    return 0;
}

int APS5_VABI sceVideoCoreAsyncSendRequest() {
    return 0;
}

int APS5_VABI sceVideoCoreCancelThumbnail() {
    return 0;
}

int APS5_VABI sceVideoCoreChangeActiveStatus() {
    return 0;
}

int APS5_VABI sceVideoCoreChangeAudioStream() {
    return 0;
}

int APS5_VABI sceVideoCoreChangeDisplaySize() {
    return 0;
}

int APS5_VABI sceVideoCoreChangeHdcpSetting() {
    return 0;
}

int APS5_VABI sceVideoCoreChangeSubtitle() {
    return 0;
}

int APS5_VABI sceVideoCoreDeleteStream() {
    return 0;
}

int APS5_VABI sceVideoCoreGenerateThumbnail() {
    return 0;
}

int APS5_VABI sceVideoCoreGenerateThumbnail2() {
    return 0;
}

int APS5_VABI sceVideoCoreGetCanvasHandle() {
    return 0;
}

int APS5_VABI sceVideoCoreGetPlayStatus() {
    return 0;
}

int APS5_VABI sceVideoCoreGetPlayStatus2() {
    return 0;
}

int APS5_VABI sceVideoCoreGetStreamUid() {
    return 0;
}

int APS5_VABI sceVideoCoreGetThumbnail() {
    return 0;
}

int APS5_VABI sceVideoCoreGetThumbnail2() {
    return 0;
}

int APS5_VABI sceVideoCoreLaunchVideoCore() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceAbortSourceBuffer() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceAddSourceBuffer() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceAppendData() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceEndOfStream() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceGetBufferedTimeRanges() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceGetInputBufferEmptySize() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceGetPlaybackQuality() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceGetSourceBufferEvent() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceRemoveSourceBuffer() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceSetDuration() {
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceSetTimestampOffset() {
    return 0;
}

int APS5_VABI sceVideoCorePlayControl() {
    return 0;
}

int APS5_VABI sceVideoCoreRegistEventCallback() {
    return 0;
}

int APS5_VABI sceVideoCoreReleaseDecoderResource(std::uint32_t resourceId) {
    return ReleaseDecoderResource(resourceId, false);
}

int APS5_VABI sceVideoCoreReleaseDecoderResourceForBeWrapper(std::uint32_t resourceId) {
    return ReleaseDecoderResource(resourceId, true);
}

int APS5_VABI sceVideoCoreSetAudioMute() {
    return 0;
}

int APS5_VABI sceVideoCoreSetAudioVolume() {
    return 0;
}

int APS5_VABI sceVideoCoreSetFullScreen() {
    return 0;
}

int APS5_VABI sceVideoCoreSetInitialInfo() {
    return 0;
}

int APS5_VABI sceVideoCoreSetInitializeInfo() {
    return 0;
}

int APS5_VABI sceVideoCoreSetPlaybackRange() {
    return 0;
}

int APS5_VABI sceVideoCoreSetThumbnailInfo() {
    return 0;
}

int APS5_VABI sceVideoCoreSyncAddStream() {
    return 0;
}

int APS5_VABI sceVideoCoreSyncSendRequest() {
    return 0;
}

int APS5_VABI sceVideoCoreUnregistEventCallback() {
    return 0;
}

}
