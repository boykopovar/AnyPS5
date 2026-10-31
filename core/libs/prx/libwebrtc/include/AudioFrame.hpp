#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcAudioFrameMaxDataSizeSamples = 7680;
constexpr std::int32_t WebRtcAudioFrameSpeechUndefined = 4;
constexpr std::int32_t WebRtcAudioFrameVadUnknown = 2;
constexpr std::int32_t WebRtcChannelLayoutNone = 0;

struct WebRtcAudioFrame {
    std::uint32_t timestamp;
    std::int64_t elapsedTimeMs;
    std::int64_t ntpTimeMs;
    std::size_t samplesPerChannel;
    std::int32_t sampleRateHz;
    std::size_t numChannels;
    std::int32_t speechType;
    std::int32_t vadActivity;
    std::int64_t profileTimestampMs;
    void* packetInfos;
    std::int16_t data[WebRtcAudioFrameMaxDataSizeSamples];
    bool muted;
    std::int32_t channelLayout;
    union {
        char nullState;
        std::int64_t value;
    } absoluteCaptureTimestampMs;
    bool absoluteCaptureTimestampMsEngaged;
};

static_assert(sizeof(WebRtcAudioFrame) == 15456);
static_assert(offsetof(WebRtcAudioFrame, elapsedTimeMs) == 8);
static_assert(offsetof(WebRtcAudioFrame, ntpTimeMs) == 16);
static_assert(offsetof(WebRtcAudioFrame, samplesPerChannel) == 24);
static_assert(offsetof(WebRtcAudioFrame, sampleRateHz) == 32);
static_assert(offsetof(WebRtcAudioFrame, numChannels) == 40);
static_assert(offsetof(WebRtcAudioFrame, speechType) == 48);
static_assert(offsetof(WebRtcAudioFrame, vadActivity) == 52);
static_assert(offsetof(WebRtcAudioFrame, profileTimestampMs) == 56);
static_assert(offsetof(WebRtcAudioFrame, packetInfos) == 64);
static_assert(offsetof(WebRtcAudioFrame, data) == 72);
static_assert(offsetof(WebRtcAudioFrame, muted) == 15432);
static_assert(offsetof(WebRtcAudioFrame, channelLayout) == 15436);
static_assert(offsetof(WebRtcAudioFrame, absoluteCaptureTimestampMs) == 15440);
static_assert(offsetof(WebRtcAudioFrame, absoluteCaptureTimestampMsEngaged) == 15448);
