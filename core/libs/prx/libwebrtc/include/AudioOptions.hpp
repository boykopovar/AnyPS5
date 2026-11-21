#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcOptionalBool {
    bool value;
    bool engaged;
};

struct WebRtcOptionalInt {
    std::int32_t value;
    bool engaged;
};

struct WebRtcOptionalGuestString {
    WebRtcGuestString value;
    bool engaged;
};

struct WebRtcAudioOptions {
    WebRtcOptionalBool echoCancellation;
    WebRtcOptionalBool autoGainControl;
    WebRtcOptionalBool noiseSuppression;
    WebRtcOptionalBool highpassFilter;
    WebRtcOptionalBool stereoSwapping;
    WebRtcOptionalInt audioJitterBufferMaxPackets;
    WebRtcOptionalBool audioJitterBufferFastAccelerate;
    WebRtcOptionalInt audioJitterBufferMinDelayMs;
    WebRtcOptionalBool audioNetworkAdaptor;
    WebRtcOptionalGuestString audioNetworkAdaptorConfig;
    WebRtcOptionalBool initRecordingOnSend;
};

static_assert(sizeof(WebRtcOptionalInt) == 8);
static_assert(sizeof(WebRtcOptionalGuestString) == 48);
static_assert(sizeof(WebRtcAudioOptions) == 96);
static_assert(offsetof(WebRtcAudioOptions, echoCancellation) == 0);
static_assert(offsetof(WebRtcAudioOptions, autoGainControl) == 2);
static_assert(offsetof(WebRtcAudioOptions, noiseSuppression) == 4);
static_assert(offsetof(WebRtcAudioOptions, highpassFilter) == 6);
static_assert(offsetof(WebRtcAudioOptions, stereoSwapping) == 8);
static_assert(offsetof(WebRtcAudioOptions, audioJitterBufferMaxPackets) == 12);
static_assert(offsetof(WebRtcAudioOptions, audioJitterBufferFastAccelerate) == 20);
static_assert(offsetof(WebRtcAudioOptions, audioJitterBufferMinDelayMs) == 24);
static_assert(offsetof(WebRtcAudioOptions, audioNetworkAdaptor) == 32);
static_assert(offsetof(WebRtcAudioOptions, audioNetworkAdaptorConfig) == 40);
static_assert(offsetof(WebRtcAudioOptions, initRecordingOnSend) == 88);
