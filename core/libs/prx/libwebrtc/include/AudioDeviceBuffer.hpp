#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcAudioTransport;

struct WebRtcAudioDeviceBuffer {
    std::uint8_t mainThreadChecker;
    std::uint8_t reserved0[7];
    std::uint64_t lock;
    std::uint64_t taskQueue;
    WebRtcAudioTransport* audioTransportCallback;
    std::uint32_t recordingSampleRate;
    std::uint32_t playoutSampleRate;
    std::uint64_t recordingChannels;
    std::uint64_t playoutChannels;
    bool playing;
    bool recording;
};

static_assert(offsetof(WebRtcAudioDeviceBuffer, lock) == 8);
static_assert(offsetof(WebRtcAudioDeviceBuffer, taskQueue) == 16);
static_assert(offsetof(WebRtcAudioDeviceBuffer, audioTransportCallback) == 24);
static_assert(offsetof(WebRtcAudioDeviceBuffer, recordingSampleRate) == 32);
static_assert(offsetof(WebRtcAudioDeviceBuffer, playoutSampleRate) == 36);
static_assert(offsetof(WebRtcAudioDeviceBuffer, recordingChannels) == 40);
static_assert(offsetof(WebRtcAudioDeviceBuffer, playoutChannels) == 48);
static_assert(offsetof(WebRtcAudioDeviceBuffer, playing) == 56);
static_assert(offsetof(WebRtcAudioDeviceBuffer, recording) == 57);
