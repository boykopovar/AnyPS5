#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcAudioCodecInfo {
    int sampleRateHz;
    std::uint64_t numChannels;
    int defaultBitrateBps;
    int minBitrateBps;
    int maxBitrateBps;
    bool allowComfortNoise;
    bool supportsNetworkAdaption;
};

static_assert(sizeof(WebRtcAudioCodecInfo) == 32);
static_assert(offsetof(WebRtcAudioCodecInfo, sampleRateHz) == 0);
static_assert(offsetof(WebRtcAudioCodecInfo, numChannels) == 8);
static_assert(offsetof(WebRtcAudioCodecInfo, defaultBitrateBps) == 16);
static_assert(offsetof(WebRtcAudioCodecInfo, minBitrateBps) == 20);
static_assert(offsetof(WebRtcAudioCodecInfo, maxBitrateBps) == 24);
static_assert(offsetof(WebRtcAudioCodecInfo, allowComfortNoise) == 28);
static_assert(offsetof(WebRtcAudioCodecInfo, supportsNetworkAdaption) == 29);
