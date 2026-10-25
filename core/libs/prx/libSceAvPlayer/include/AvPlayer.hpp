// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_LIBS_PRX_LIBSCEAVPLAYER_INCLUDE_AVPLAYER_HPP
#define CORE_LIBS_PRX_LIBSCEAVPLAYER_INCLUDE_AVPLAYER_HPP

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include "SceTypes.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"

namespace AvPlayer {

constexpr int SCE_OK = 0;
constexpr int SCE_AVPLAYER_ERROR_INVALID_PARAMS = static_cast<int>(0x806A0001u);
constexpr int SCE_AVPLAYER_ERROR_OPERATION_FAILED = static_cast<int>(0x806A0002u);
constexpr int SCE_AVPLAYER_ERROR_NO_MEMORY = static_cast<int>(0x806A0003u);
constexpr int SCE_AVPLAYER_ERROR_NOT_SUPPORTED = static_cast<int>(0x806A0004u);
constexpr int SCE_AVPLAYER_ERROR_WAR_LOOPING_BACK = static_cast<int>(0x806A00A1u);
constexpr int SCE_AVPLAYER_ERROR_WAR_JUMP_COMPLETE = static_cast<int>(0x806A00A3u);

constexpr std::int32_t EventStateStop = 0x01;
constexpr std::int32_t EventStateReady = 0x02;
constexpr std::int32_t EventStatePlay = 0x03;
constexpr std::int32_t EventStatePause = 0x04;
constexpr std::int32_t EventWarningId = 0x20;

constexpr std::uint32_t StreamTypeVideo = 1;
constexpr std::uint32_t StreamTypeAudio = 0;
constexpr std::uint32_t Ps5StreamTypeAudio = 2;
constexpr std::uint32_t StreamTypeTimedText = 2;

constexpr std::uint32_t SourceTypeUnknown = 0;
constexpr std::uint32_t SourceTypeFileMp4 = 1;
constexpr std::uint32_t SourceTypeHls = 8;

constexpr std::uint32_t SyncModeDefault = 0;
constexpr std::uint32_t SyncModeNone = 1;

constexpr std::int32_t NormalSpeed = 100;

class ISourceEvents {
public:
    virtual ~ISourceEvents() = default;
    virtual void OnWarning(std::int32_t code) = 0;
    virtual void OnError() = 0;
};

struct StartInfo {
    std::uint64_t size;
    std::uint64_t startTime;
};

static_assert(sizeof(StartInfo) == 16);

struct SourceSettings {
    AvPlayerMemAllocator memory;
    AvPlayerFileReplacement file;
    std::uint32_t videoBuffers;
};

class ISource {
public:
    virtual ~ISource() = default;
    virtual std::uint32_t StreamCount() const = 0;
    virtual bool GetStreamInfo(std::uint32_t index, AvPlayerStreamInfo& info) const = 0;
    virtual bool GetStreamInfoEx(std::uint32_t index, AvPlayerStreamInfoEx& info) const = 0;
    virtual bool EnableStream(std::uint32_t index) = 0;
    virtual bool DisableStream(std::uint32_t index) = 0;
    virtual bool ChangeStream(std::uint32_t from, std::uint32_t to) = 0;
    virtual int Start(std::uint64_t startTime) = 0;
    virtual void Stop() = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual void Jump(std::uint64_t milliseconds) = 0;
    virtual void SetLooping(bool looping) = 0;
    virtual void SetSpeed(std::int32_t speed) = 0;
    virtual void SetSyncMode(std::uint32_t mode) = 0;
    virtual void SetDemuxVideoBufferSize(std::uint32_t bytes) = 0;
    virtual bool GetVideoData(AvPlayerFrameInfoEx& info) = 0;
    virtual bool GetAudioData(AvPlayerFrameInfo& info) = 0;
    virtual std::uint64_t CurrentTime() = 0;
    virtual bool Finished() = 0;
    virtual void SetClockTimeForTest(double milliseconds) = 0;
    virtual void SetHoldAudioEofForTest(bool hold) = 0;
    virtual bool AudioEofHeldForTest() = 0;
    virtual bool AudioEndedForTest() = 0;
    virtual int QueuedAudioForTest() = 0;
    virtual bool NextQueuedAudioTimestampForTest(std::uint64_t& timestamp) = 0;
};

std::unique_ptr<ISource> OpenSource(const SourceSettings& settings, const std::string& path, ISourceEvents& events);

std::uint32_t DetectSourceType(std::string_view path);

void SetAvPlayerClockForTest_nid_no_patch(AvPlayerInternal* h, double milliseconds);
void HoldAvPlayerAudioEofForTest_nid_no_patch(AvPlayerInternal* h, bool hold);
bool AvPlayerAudioEofHeldForTest_nid_no_patch(AvPlayerInternal* h);
bool AvPlayerAudioEndedForTest_nid_no_patch(AvPlayerInternal* h);
int QueuedAvPlayerAudioForTest_nid_no_patch(AvPlayerInternal* h);
bool NextQueuedAvPlayerAudioTimestampForTest_nid_no_patch(AvPlayerInternal* h, std::uint64_t* timestamp);

class Player final : public AvPlayerInternal, private ISourceEvents {
public:
    Player(const AvPlayerInitData& init, std::uint32_t bufferCount, bool ps5Layout = false);
    ~Player() override;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    int PostInit(const AvPlayerPostInitData& data);
    int AddSource(std::string_view path, std::uint32_t sourceType);
    int StreamCount();
    int GetStreamInfo(std::uint32_t index, AvPlayerStreamInfo& info);
    int GetStreamInfoEx(std::uint32_t index, AvPlayerStreamInfoEx& info);
    int EnableStream(std::uint32_t index);
    int DisableStream(std::uint32_t index);
    int ChangeStream(std::uint32_t from, std::uint32_t to);
    int Start(std::uint64_t startTime = 0);
    int Stop();
    int Pause();
    int Resume();
    int JumpToTime(std::uint64_t milliseconds);
    int SetLooping(bool enabled);
    int SetTrickSpeed(std::int32_t trickSpeed);
    int SetAvSyncMode(std::uint32_t mode);
    bool GetVideoData(AvPlayerFrameInfoEx& info);
    bool GetVideoData(AvPlayerFrameInfo& info);
    bool GetAudioData(AvPlayerFrameInfo& info);
    bool IsActive();
    std::uint64_t CurrentTime();
    void SetClockTimeForTest(double milliseconds);
    void SetHoldAudioEofForTest(bool hold);
    bool AudioEofHeldForTest();
    bool AudioEndedForTest();
    int QueuedAudioForTest();
    bool NextQueuedAudioTimestampForTest(std::uint64_t& timestamp);

private:
    enum class State { Initial, Ready, Play, Pause, Stop, EndOfFile, Error };

    struct Event {
        std::int32_t id;
        std::int32_t warning;
        bool autoStart;
        bool error;
    };

    void OnWarning(std::int32_t code) override;
    void OnError() override;
    void queue(const Event& event);
    void controllerLoop(const bool& closed);
    void deliver(const Event& event);
    void autoStart();
    void checkEndOfFile();
    bool readyLocked() const;
    int startLocked(std::uint64_t startTime = 0);
    int stopLocked();

    AvPlayerFileReplacement file{};
    AvPlayerMemAllocator memory{};
    AvPlayerEventReplacement callback{};
    bool autoStartEnabled = false;
    bool ps5Layout = false;
    char language[4]{};
    std::uint32_t videoBuffers = 2;

    std::mutex mutex;
    std::unique_ptr<ISource> source;
    State state = State::Initial;
    State previous = State::Initial;
    bool looping = false;
    std::int32_t speed = NormalSpeed;
    std::uint32_t syncMode = SyncModeDefault;
    std::uint32_t demuxVideoBytes = 0;

    std::mutex eventMutex;
    TimedWait::Condition eventCondition;
    std::deque<Event> events;
    bool quit = false;
    std::shared_ptr<bool> controllerClosed = std::make_shared<bool>(false);
    std::thread controller;
};

}

#endif
