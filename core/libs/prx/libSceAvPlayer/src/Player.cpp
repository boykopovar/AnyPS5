// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include "prx/libSceAvPlayer/include/AvPlayer.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestHeap.hpp"

namespace AvPlayer {

namespace {

bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right, [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
}

bool SameLanguage(const char* preferred, const char* code) {
    return preferred[0] != '\0' && std::strncmp(preferred, code, 4) == 0;
}

void* APS5_VABI GuestAllocate_nid_no_patch(void*, std::uint32_t alignment, std::uint32_t size) {
    return GuestHeap::GuestHeapAlign_nid_postfix(std::max<std::uint32_t>(alignment, 16), size);
}

void APS5_VABI GuestDeallocate_nid_no_patch(void*, void* memory) {
    GuestHeap::GuestHeapFree_nid_postfix(memory);
}

AvPlayerMemAllocator WithGuestHeapFallback(AvPlayerMemAllocator memory) {
    if (!memory.allocate || !memory.deallocate) {
        memory.allocate = GuestAllocate_nid_no_patch;
        memory.deallocate = GuestDeallocate_nid_no_patch;
    }
    if (!memory.allocate_texture || !memory.deallocate_texture) {
        memory.allocate_texture = GuestAllocate_nid_no_patch;
        memory.deallocate_texture = GuestDeallocate_nid_no_patch;
    }
    return memory;
}

}

std::uint32_t DetectSourceType(std::string_view path) {
    std::string_view name = path;
    if (name.find("://") != std::string_view::npos) name = name.substr(0, name.find_first_of("?#"));
    const auto dot = name.rfind('.');
    if (dot == std::string_view::npos) return SourceTypeUnknown;
    auto extension = name.substr(dot);
    extension = extension.substr(0, extension.find('/'));
    for (const auto mp4 : {".mp4", ".m4v", ".m3d", ".m4a", ".mov"}) {
        if (EqualsIgnoreCase(extension, mp4)) return SourceTypeFileMp4;
    }
    if (EqualsIgnoreCase(extension, ".m3u8")) return SourceTypeHls;
    return SourceTypeUnknown;
}

Player::Player(const AvPlayerInitData& init, std::uint32_t bufferCount)
    : memory(WithGuestHeapFallback(init.memory_replacement)), callback(init.event_replacement), videoBuffers(bufferCount) {
    if (init.file_replacement.open && init.file_replacement.close && init.file_replacement.read_offset && init.file_replacement.size) {
        file = init.file_replacement;
    }
    autoStartEnabled = init.auto_start || callback.event_callback == nullptr;
    if (init.default_language) {
        for (std::size_t index = 0; index < 3 && init.default_language[index] != '\0'; ++index) language[index] = init.default_language[index];
    }
    controller = std::thread([this] { controllerLoop(); });
}

Player::~Player() {
    {
        std::lock_guard lock(eventMutex);
        quit = true;
    }
    eventCondition.notify_all();
    controller.join();
    std::lock_guard lock(mutex);
    source.reset();
}

void Player::queue(const Event& event) {
    {
        std::lock_guard lock(eventMutex);
        events.push_back(event);
    }
    eventCondition.notify_all();
}

void Player::OnWarning(std::int32_t code) {
    queue({EventWarningId, code, false, false});
}

void Player::OnError() {
    queue({0, 0, false, true});
}

void Player::controllerLoop() {
    for (;;) {
        Event event{};
        bool pending = false;
        {
            std::unique_lock lock(eventMutex);
            eventCondition.wait_for(lock, std::chrono::milliseconds(5), [this] { return quit || !events.empty(); });
            if (quit) return;
            if (!events.empty()) {
                event = events.front();
                events.pop_front();
                pending = true;
            }
        }
        if (pending) {
            deliver(event);
        } else {
            checkEndOfFile();
        }
    }
}

void Player::deliver(const Event& event) {
    if (event.error) {
        std::lock_guard lock(mutex);
        if (source && state != State::Stop) {
            previous = state;
            state = State::Error;
        }
        return;
    }
    if (event.autoStart) {
        autoStart();
        return;
    }
    if (!callback.event_callback) return;
    std::int32_t warning = event.warning;
    callback.event_callback(callback.object_ptr, event.id, 0, event.id == EventWarningId ? &warning : nullptr);
}

bool Player::readyLocked() const {
    return source && state == State::Ready;
}

void Player::autoStart() {
    {
        std::lock_guard lock(mutex);
        if (!readyLocked()) return;
    }
    const int count = StreamCount();
    if (count <= 0) {
        Stop();
        return;
    }
    int video = -1;
    int audio = -1;
    for (int index = 0; index < count; ++index) {
        AvPlayerStreamInfo info{};
        if (GetStreamInfo(static_cast<std::uint32_t>(index), info) != SCE_OK) {
            Stop();
            return;
        }
        if (info.type == StreamTypeVideo && (video == -1 || SameLanguage(language, info.details.video.language_code))) video = index;
        if (info.type == StreamTypeAudio && (audio == -1 || SameLanguage(language, info.details.audio.language_code))) audio = index;
    }
    if (video != -1) EnableStream(static_cast<std::uint32_t>(video));
    if (audio != -1) EnableStream(static_cast<std::uint32_t>(audio));
    std::lock_guard lock(mutex);
    if (readyLocked()) startLocked();
}

void Player::checkEndOfFile() {
    {
        std::lock_guard lock(mutex);
        if (!source || state != State::Play || !source->Finished()) return;
        previous = state;
        state = State::EndOfFile;
    }
    deliver({EventStateStop, 0, false, false});
}

int Player::PostInit(const AvPlayerPostInitData& data) {
    std::lock_guard lock(mutex);
    demuxVideoBytes = data.demux_video_buffer_size;
    if (source) source->SetDemuxVideoBufferSize(demuxVideoBytes);
    return SCE_OK;
}

int Player::AddSource(std::string_view path, std::uint32_t sourceType) {
    if (path.empty()) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    if (sourceType == SourceTypeUnknown) sourceType = DetectSourceType(path);
    if (sourceType == SourceTypeHls) NotImplemented_nid_no_patch("sceAvPlayerAddSource (HLS)");
    std::lock_guard lock(mutex);
    if (source) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    auto opened = OpenSource({memory, file, videoBuffers}, std::string(path), *this);
    if (!opened) {
        previous = state;
        state = State::Error;
        return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    }
    if (opened->StreamCount() == 0) {
        source = std::move(opened);
        previous = state;
        state = State::Error;
        queue({EventWarningId, SCE_AVPLAYER_ERROR_NOT_SUPPORTED, false, false});
        return SCE_OK;
    }
    opened->SetLooping(looping);
    opened->SetSpeed(speed);
    opened->SetSyncMode(syncMode);
    opened->SetDemuxVideoBufferSize(demuxVideoBytes);
    source = std::move(opened);
    previous = state;
    state = State::Ready;
    queue(autoStartEnabled ? Event{0, 0, true, false} : Event{EventStateReady, 0, false, false});
    return SCE_OK;
}

int Player::StreamCount() {
    std::lock_guard lock(mutex);
    if (!source) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    return static_cast<int>(source->StreamCount());
}

int Player::GetStreamInfo(std::uint32_t index, AvPlayerStreamInfo& info) {
    std::lock_guard lock(mutex);
    if (!source || !source->GetStreamInfo(index, info)) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    return SCE_OK;
}

int Player::GetStreamInfoEx(std::uint32_t index, AvPlayerStreamInfoEx& info) {
    std::lock_guard lock(mutex);
    AvPlayerStreamInfoEx described{};
    if (!source || !source->GetStreamInfoEx(index, described)) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    described.this_size = info.this_size;
    info = described;
    return SCE_OK;
}

int Player::EnableStream(std::uint32_t index) {
    std::lock_guard lock(mutex);
    if (!source || !source->EnableStream(index)) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    return SCE_OK;
}

int Player::DisableStream(std::uint32_t index) {
    std::lock_guard lock(mutex);
    if (!source || !source->DisableStream(index)) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    return SCE_OK;
}

int Player::ChangeStream(std::uint32_t from, std::uint32_t to) {
    std::lock_guard lock(mutex);
    if (!source || state == State::Error || !source->ChangeStream(from, to)) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    return SCE_OK;
}

int Player::Start() {
    std::lock_guard lock(mutex);
    return startLocked();
}

int Player::startLocked() {
    if (!source) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    if (state != State::Ready && state != State::Stop && stopLocked() != SCE_OK) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    const int result = source->Start();
    if (result != SCE_OK) {
        previous = state;
        state = State::Error;
        return result;
    }
    previous = state;
    state = State::Play;
    queue({EventStatePlay, 0, false, false});
    return SCE_OK;
}

int Player::stopLocked() {
    if (!source || state == State::Initial || state == State::Ready || state == State::Stop) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    source->Stop();
    previous = state;
    state = State::Stop;
    queue({EventStateStop, 0, false, false});
    return SCE_OK;
}

int Player::Stop() {
    std::lock_guard lock(mutex);
    return stopLocked();
}

int Player::Pause() {
    std::lock_guard lock(mutex);
    if (state == State::EndOfFile) return SCE_OK;
    if (!source || state != State::Play) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    source->Pause();
    previous = state;
    state = State::Pause;
    queue({EventStatePause, 0, false, false});
    return SCE_OK;
}

int Player::Resume() {
    std::lock_guard lock(mutex);
    if (!source || state != State::Pause) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    source->Resume();
    std::swap(state, previous);
    queue({EventStatePlay, 0, false, false});
    return SCE_OK;
}

int Player::JumpToTime(std::uint64_t milliseconds) {
    std::lock_guard lock(mutex);
    if (!source || (state != State::Ready && state != State::Play && state != State::Pause)) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    source->Jump(milliseconds);
    return SCE_OK;
}

int Player::SetLooping(bool enabled) {
    std::lock_guard lock(mutex);
    looping = enabled;
    if (source) source->SetLooping(enabled);
    return SCE_OK;
}

int Player::SetTrickSpeed(std::int32_t trickSpeed) {
    if (trickSpeed == 0) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    std::lock_guard lock(mutex);
    if (state == State::Stop || state == State::EndOfFile || state == State::Error) return SCE_AVPLAYER_ERROR_OPERATION_FAILED;
    speed = trickSpeed;
    if (source) source->SetSpeed(trickSpeed);
    return SCE_OK;
}

int Player::SetAvSyncMode(std::uint32_t mode) {
    if (mode != SyncModeDefault && mode != SyncModeNone) return SCE_AVPLAYER_ERROR_INVALID_PARAMS;
    std::lock_guard lock(mutex);
    syncMode = mode;
    if (source) source->SetSyncMode(mode);
    return SCE_OK;
}

bool Player::GetVideoData(AvPlayerFrameInfoEx& info) {
    std::lock_guard lock(mutex);
    if (!source || (state != State::Play && state != State::Pause)) return false;
    return source->GetVideoData(info);
}

bool Player::GetVideoData(AvPlayerFrameInfo& info) {
    AvPlayerFrameInfoEx extended{};
    if (!GetVideoData(extended)) return false;
    info = {};
    info.p_data = static_cast<std::uint8_t*>(extended.p_data);
    info.timestamp = extended.timestamp;
    info.details.video.width = extended.details.video.width;
    info.details.video.height = extended.details.video.height;
    info.details.video.aspect_ratio = extended.details.video.aspect_ratio;
    std::memcpy(info.details.video.language_code, extended.details.video.language_code, sizeof(info.details.video.language_code));
    return true;
}

bool Player::GetAudioData(AvPlayerFrameInfo& info) {
    std::lock_guard lock(mutex);
    if (!source || state != State::Play) return false;
    return source->GetAudioData(info);
}

bool Player::IsActive() {
    std::lock_guard lock(mutex);
    if (!source || state == State::Initial || state == State::Stop || state == State::EndOfFile || state == State::Error) return false;
    return !source->Finished();
}

std::uint64_t Player::CurrentTime() {
    std::lock_guard lock(mutex);
    return source ? source->CurrentTime() : 0;
}

}
