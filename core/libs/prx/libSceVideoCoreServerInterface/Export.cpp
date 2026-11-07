// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceVideoCoreServerInterface/include/Api.hpp"
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
    auto& state = Resources();
    std::lock_guard lock(state.mutex);
    const std::uint32_t id = state.nextId++;
    state.resources[id] = DecoderResource{beWrapper, *param, callback};
    *resourceId = id;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreAcquireDecoderResource(const VideoCoreAcquireDecoderResourceParam* param, VideoCoreDecoderResourceCallback callback, std::uint32_t* resourceId) {
    return AcquireDecoderResource(param, callback, resourceId, false);
}

int APS5_VABI sceVideoCoreAcquireDecoderResourceForBeWrapper(const VideoCoreAcquireDecoderResourceParam* param, VideoCoreDecoderResourceCallback callback, std::uint32_t* resourceId) {
    return AcquireDecoderResource(param, callback, resourceId, true);
}

int APS5_VABI sceVideoCoreAddStream() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreAsyncAddStream() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreAsyncSendRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreCancelThumbnail() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreChangeActiveStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreChangeAudioStream() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreChangeDisplaySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreChangeHdcpSetting() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreChangeSubtitle() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreDeleteStream() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreGenerateThumbnail() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreGenerateThumbnail2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreGetCanvasHandle() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreGetPlayStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreGetPlayStatus2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreGetStreamUid() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreGetThumbnail() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreGetThumbnail2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreLaunchVideoCore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceAbortSourceBuffer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceAddSourceBuffer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceAppendData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceEndOfStream() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceGetBufferedTimeRanges() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceGetInputBufferEmptySize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceGetPlaybackQuality() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceGetSourceBufferEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceRemoveSourceBuffer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceSetDuration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreMediaSourceSetTimestampOffset() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCorePlayControl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreRegistEventCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreReleaseDecoderResource(std::uint32_t resourceId) {
    return ReleaseDecoderResource(resourceId, false);
}

int APS5_VABI sceVideoCoreReleaseDecoderResourceForBeWrapper(std::uint32_t resourceId) {
    return ReleaseDecoderResource(resourceId, true);
}

int APS5_VABI sceVideoCoreSetAudioMute() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreSetAudioVolume() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreSetFullScreen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreSetInitialInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreSetInitializeInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreSetPlaybackRange() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreSetThumbnailInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreSyncAddStream() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreSyncSendRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoCoreUnregistEventCallback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
