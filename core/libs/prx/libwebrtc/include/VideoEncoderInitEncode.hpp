#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcVideoEncoder;
struct WebRtcVideoCodec;

struct WebRtcVideoEncoderSettings {
    bool lossNotification;
    std::int32_t numberOfCores;
    std::uint64_t maxPayloadSize;
    union {
        char nullState;
        std::int32_t value;
    } encoderThreadLimit;
    bool encoderThreadLimitEngaged;
};

static_assert(sizeof(WebRtcVideoEncoderSettings) == 24);
static_assert(alignof(WebRtcVideoEncoderSettings) == 8);
static_assert(offsetof(WebRtcVideoEncoderSettings, lossNotification) == 0);
static_assert(offsetof(WebRtcVideoEncoderSettings, numberOfCores) == 4);
static_assert(offsetof(WebRtcVideoEncoderSettings, maxPayloadSize) == 8);
static_assert(offsetof(WebRtcVideoEncoderSettings, encoderThreadLimit) == 16);
static_assert(offsetof(WebRtcVideoEncoderSettings, encoderThreadLimitEngaged) == 20);
