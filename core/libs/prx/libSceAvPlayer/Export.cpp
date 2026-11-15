#include "prx/libSceAvPlayer/include/Api.hpp"
#include "prx/libc/include/General.hpp"
// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <string>
#include <string_view>
#include "SceTypes.hpp"
#include "prx/libSceAvPlayer/include/AvPlayer.hpp"
#include "prx/libSceAvPlayer/include/LogCallback.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"

using namespace AvPlayer;

namespace {

void* LogCallback = nullptr;
void* LogUserData = nullptr;

Player* ToPlayer(AvPlayerInternal* h) {
    return static_cast<Player*>(h);
}

std::uint32_t VideoBufferCount(std::int32_t requested) {
    return static_cast<std::uint32_t>(std::clamp(requested, 2, 16));
}

constexpr std::size_t Ps5InitDataExSize = 0x230;
constexpr std::size_t Ps5AutoStartOffset = 0x74;
constexpr std::size_t Ps5VideoFrameBuffersOffset = 0x228;

bool CopyGuestIn(void* dst, const void* src, std::size_t size) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = savedErrno;
    return ok;
}

struct Language {
    char text[4]{};
};

bool CopyLanguage(const char*& language, Language& local) {
    if (language == nullptr) return true;
    for (std::size_t index = 0; index < 3; ++index) {
        if (!CopyGuestIn(&local.text[index], language + index, sizeof(char))) return false;
        if (local.text[index] == '\0') break;
    }
    language = local.text;
    return true;
}

}

extern "C" {

void SceAvPlayerSetLogCallbackState(void* callback, void* user_data) {
    LogCallback = callback;
    LogUserData = user_data;
}

int SceAvPlayerWriteLog(const char* format, VaList* arguments) {
    if (LogCallback) {
        return reinterpret_cast<SceAvPlayerLogCallback>(LogCallback)(LogUserData, format, arguments);
    }
#ifdef _WIN32
    return LibcDetail::PrintWindows(format, arguments);
#else
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(arguments));
    const int result = std::vprintf(format, copy);
    va_end(copy);
    return result;
#endif
}

}

#pragma GCC visibility push(default)
extern "C" {

int APS5_VABI sceAvPlayerAddSource(AvPlayerInternal* h, const char* filename) {
    if (!h || !filename) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->AddSource(filename, SourceTypeUnknown);
}

int APS5_VABI sceAvPlayerAddSourceEx(AvPlayerInternal* h, uint32_t uri_type, const AvPlayerSourceDetails* source_details) {
    if (!h || uri_type != 0 || !source_details) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    AvPlayerSourceDetails details{};
    if (!CopyGuestIn(&details, source_details, sizeof(details)) || !details.uri.name) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    constexpr uint32_t MaxSourceUriLength = 65536;
    if (details.uri.length > MaxSourceUriLength) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    std::string uri(details.uri.length, '\0');
    if (details.uri.length > 0 && !CopyGuestIn(uri.data(), details.uri.name, details.uri.length)) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->AddSource(uri, details.source_type);
}

int APS5_VABI sceAvPlayerChangeStream(AvPlayerInternal* h, uint32_t old_stream_id, uint32_t new_stream_id) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->ChangeStream(old_stream_id, new_stream_id);
}

int APS5_VABI sceAvPlayerClose(AvPlayerInternal* h) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    delete ToPlayer(h);
    return SCE_OK;
}

uint64_t APS5_VABI sceAvPlayerCurrentTime(AvPlayerInternal* h) {
    if (!h) return static_cast<uint64_t>(static_cast<int64_t>(SCE_AVPLAYER_ERROR_INVALID_PARAMS));
    return ToPlayer(h)->CurrentTime();
}

int APS5_VABI sceAvPlayerDisableStream(AvPlayerInternal* h, uint32_t stream_id) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->DisableStream(stream_id);
}

int APS5_VABI sceAvPlayerEnableStream(AvPlayerInternal* h, uint32_t stream_id) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->EnableStream(stream_id);
}

Bool APS5_VABI sceAvPlayerGetAudioData(AvPlayerInternal* h, AvPlayerFrameInfo* audio_info) {
    if (!h || !audio_info) return false;
    AvPlayerFrameInfo localInfo{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&localInfo, audio_info, sizeof(localInfo))) {
        errno = savedErrno;
        return false;
    }
    errno = savedErrno;
    if (!ToPlayer(h)->GetAudioData(localInfo)) return false;
    const bool copied = GuestCopyOut_nid_no_patch(audio_info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    return copied;
}

int APS5_VABI sceAvPlayerGetStreamInfo(AvPlayerInternal* h, uint32_t stream_id, AvPlayerStreamInfo* info) {
    if (!h || !info) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    AvPlayerStreamInfo localInfo{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&localInfo, info, sizeof(localInfo))) {
        errno = savedErrno;
        return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    errno = savedErrno;
    const int res = ToPlayer(h)->GetStreamInfo(stream_id, localInfo);
    if (res != 0) return res;
    const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    if (!copied) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return 0;
}

int APS5_VABI sceAvPlayerGetStreamInfoEx(AvPlayerInternal* h, uint32_t stream_id, AvPlayerStreamInfoEx* info) {
    if (!h || !info) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    AvPlayerStreamInfoEx localInfo{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&localInfo, info, sizeof(localInfo))) {
        errno = savedErrno;
        return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    errno = savedErrno;
    const int res = ToPlayer(h)->GetStreamInfoEx(stream_id, localInfo);
    if (res != 0) return res;
    const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    if (!copied) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return 0;
}

Bool APS5_VABI sceAvPlayerGetVideoData(AvPlayerInternal* h, AvPlayerFrameInfo* video_info) {
    if (!h || !video_info) return false;
    AvPlayerFrameInfo localInfo{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&localInfo, video_info, sizeof(localInfo))) {
        errno = savedErrno;
        return false;
    }
    errno = savedErrno;
    if (!ToPlayer(h)->GetVideoData(localInfo)) return false;
    const bool copied = GuestCopyOut_nid_no_patch(video_info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    return copied;
}

Bool APS5_VABI sceAvPlayerGetVideoDataEx(AvPlayerInternal* h, AvPlayerFrameInfoEx* video_info) {
    if (!h || !video_info) return false;
    AvPlayerFrameInfoEx localInfo{};
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&localInfo, video_info, sizeof(localInfo))) {
        errno = savedErrno;
        return false;
    }
    errno = savedErrno;
    if (!ToPlayer(h)->GetVideoData(localInfo)) return false;
    const bool copied = GuestCopyOut_nid_no_patch(video_info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    return copied;
}

AvPlayerInternal* APS5_VABI sceAvPlayerInit(AvPlayerInitData* init) {
    if (!init) return nullptr;
    AvPlayerInitData localInit{};
    Language language;
    if (!CopyGuestIn(&localInit, init, sizeof(localInit)) || !CopyLanguage(localInit.default_language, language)) return nullptr;
    return new Player(localInit, VideoBufferCount(localInit.num_output_video_framebuffers));
}

int APS5_VABI sceAvPlayerInitEx(const AvPlayerInitDataEx* init_ex, AvPlayerInternal** handle) {
    if (!init_ex || !handle) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    AvPlayerInitDataEx localEx{};
    if (!CopyGuestIn(&localEx, init_ex, sizeof(localEx))) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    AvPlayerInitData init{};
    init.memory_replacement = localEx.memory_replacement;
    init.file_replacement = localEx.file_replacement;
    init.event_replacement = localEx.event_replacement;
    init.debug_level = localEx.debug_level;
    if (localEx.this_size >= Ps5InitDataExSize) {
        std::array<std::uint8_t, Ps5InitDataExSize> ps5Bytes{};
        if (!CopyGuestIn(ps5Bytes.data(), init_ex, ps5Bytes.size())) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
        std::memcpy(&init.num_output_video_framebuffers, ps5Bytes.data() + Ps5VideoFrameBuffersOffset, sizeof(init.num_output_video_framebuffers));
        init.auto_start = ps5Bytes[Ps5AutoStartOffset] != 0;
    } else {
        init.num_output_video_framebuffers = localEx.num_output_video_framebuffers;
        init.auto_start = localEx.auto_start;
    }
    init.default_language = localEx.default_language;
    Language language;
    if (!CopyLanguage(init.default_language, language)) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    auto* player = new Player(init, VideoBufferCount(init.num_output_video_framebuffers));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(handle, &player, sizeof(player));
    errno = savedErrno;
    if (!copied) {
        delete player;
        return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    }
    return SCE_OK;
}

Bool APS5_VABI sceAvPlayerIsActive(AvPlayerInternal* h) {
    if (!h) return false;
    return ToPlayer(h)->IsActive();
}

int APS5_VABI sceAvPlayerJumpToTime(AvPlayerInternal* h, uint64_t time_ms) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->JumpToTime(time_ms);
}

int APS5_VABI sceAvPlayerPause(AvPlayerInternal* h) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->Pause();
}

int APS5_VABI sceAvPlayerPostInit(AvPlayerInternal* h, const AvPlayerPostInitData* post_init) {
    if (!h || !post_init) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    AvPlayerPostInitData localPost{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localPost, post_init, sizeof(localPost));
    errno = savedErrno;
    if (!copied) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->PostInit(localPost);
}

int APS5_VABI sceAvPlayerResume(AvPlayerInternal* h) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->Resume();
}

int APS5_VABI sceAvPlayerSetAvSyncMode(AvPlayerInternal* h, uint32_t sync_mode) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->SetAvSyncMode(sync_mode);
}

int APS5_VABI sceAvPlayerSetAvailableBandwidth(AvPlayerInternal* h, uint32_t start_bandwidth, uint32_t minimum_bandwidth, uint32_t maximum_bandwidth) {
    (void)start_bandwidth;
    (void)minimum_bandwidth;
    (void)maximum_bandwidth;
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->SetAvailableBandwidth();
}

int APS5_VABI sceAvPlayerSetLogCallback(void* callback, void* user_data) {
    SceAvPlayerSetLogCallbackState(callback, user_data);
    return SCE_OK;
}

int APS5_VABI sceAvPlayerSetLooping(AvPlayerInternal* h, Bool loop) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->SetLooping(loop != 0);
}

int APS5_VABI sceAvPlayerSetTrickSpeed(AvPlayerInternal* h, int32_t trick_speed) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->SetTrickSpeed(trick_speed);
}

int APS5_VABI sceAvPlayerStart(AvPlayerInternal* h) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->Start();
}

int APS5_VABI sceAvPlayerStartEx(AvPlayerInternal* handle, const StartInfo* startInfo) {
    if (handle == nullptr || startInfo == nullptr) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    StartInfo localStart{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localStart, startInfo, sizeof(localStart));
    errno = savedErrno;
    if (!copied || localStart.size != sizeof(localStart)) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(handle)->Start(localStart.startTime);
}

int APS5_VABI sceAvPlayerStop(AvPlayerInternal* h) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->Stop();
}

int APS5_VABI sceAvPlayerStreamCount(AvPlayerInternal* h) {
    if (!h) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    return ToPlayer(h)->StreamCount();
}

}

namespace AvPlayer {

void SetAvPlayerClockForTest_nid_no_patch(AvPlayerInternal* h, double milliseconds) {
    if (h) ToPlayer(h)->SetClockTimeForTest(milliseconds);
}

void HoldAvPlayerAudioEofForTest_nid_no_patch(AvPlayerInternal* h, bool hold) {
    if (h) ToPlayer(h)->SetHoldAudioEofForTest(hold);
}

bool AvPlayerAudioEofHeldForTest_nid_no_patch(AvPlayerInternal* h) {
    return h && ToPlayer(h)->AudioEofHeldForTest();
}

bool AvPlayerAudioEndedForTest_nid_no_patch(AvPlayerInternal* h) {
    return h && ToPlayer(h)->AudioEndedForTest();
}

int QueuedAvPlayerAudioForTest_nid_no_patch(AvPlayerInternal* h) {
    return h ? ToPlayer(h)->QueuedAudioForTest() : 0;
}

bool NextQueuedAvPlayerAudioTimestampForTest_nid_no_patch(AvPlayerInternal* h, std::uint64_t* timestamp) {
    std::uint64_t value = 0;
    if (!h || !timestamp || !ToPlayer(h)->NextQueuedAudioTimestampForTest(value)) return false;
    *timestamp = value;
    return true;
}

}
#pragma GCC visibility pop
