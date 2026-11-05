#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcAudioEncoder;

struct WebRtcOptionalUint32 {
    union {
        char nullState;
        std::uint32_t value;
    } storage;
    bool engaged;
};

struct WebRtcOptionalFloat {
    union {
        char nullState;
        float value;
    } storage;
    bool engaged;
};

struct WebRtcAnaStats {
    WebRtcOptionalUint32 bitrateActionCounter;
    WebRtcOptionalUint32 channelActionCounter;
    WebRtcOptionalUint32 dtxActionCounter;
    WebRtcOptionalUint32 fecActionCounter;
    WebRtcOptionalUint32 frameLengthIncreaseCounter;
    WebRtcOptionalUint32 frameLengthDecreaseCounter;
    WebRtcOptionalFloat uplinkPacketLossFraction;
};

static_assert(sizeof(WebRtcOptionalUint32) == 8);
static_assert(offsetof(WebRtcOptionalUint32, engaged) == 4);
static_assert(sizeof(WebRtcOptionalFloat) == 8);
static_assert(sizeof(WebRtcAnaStats) == 56);
static_assert(offsetof(WebRtcAnaStats, uplinkPacketLossFraction) == 48);
