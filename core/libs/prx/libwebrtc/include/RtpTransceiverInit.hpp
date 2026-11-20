#pragma once

#include "prx/libwebrtc/include/IceServer.hpp"
#include "prx/libwebrtc/include/SdpAudioFormat.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::int32_t WebRtcRtpTransceiverDirectionSendRecv = 0;

struct WebRtcRtpEncodingParametersVector {
    struct WebRtcRtpEncodingParameters* begin;
    struct WebRtcRtpEncodingParameters* end;
    struct WebRtcRtpEncodingParameters* capacityEnd;
};

struct WebRtcOptionalString {
    WebRtcGuestString value;
    std::uint8_t engaged;
};

struct WebRtcRtcpFeedback {
    std::int32_t type;
    std::int32_t messageType;
    std::uint8_t hasMessageType;
};

struct WebRtcRtcpFeedbackVector {
    WebRtcRtcpFeedback* begin;
    WebRtcRtcpFeedback* end;
    WebRtcRtcpFeedback* capacityEnd;
};

struct WebRtcRtpCodec {
    void** vtable;
    WebRtcGuestString name;
    std::int32_t kind;
    std::int32_t clockRate;
    std::uint8_t hasClockRate;
    std::int32_t numChannels;
    std::uint8_t hasNumChannels;
    WebRtcRtcpFeedbackVector rtcpFeedback;
    WebRtcStringMap parameters;
};

struct WebRtcOptionalRtpCodec {
    WebRtcRtpCodec value;
    std::uint8_t engaged;
};

struct WebRtcRtpEncodingParameters {
    std::uint32_t ssrc;
    std::uint8_t hasSsrc;
    double bitratePriority;
    std::int32_t networkPriority;
    std::int32_t maxBitrateBps;
    std::uint8_t hasMaxBitrateBps;
    std::int32_t minBitrateBps;
    std::uint8_t hasMinBitrateBps;
    double maxFramerate;
    std::uint8_t hasMaxFramerate;
    std::int32_t numTemporalLayers;
    std::uint8_t hasNumTemporalLayers;
    double scaleResolutionDownBy;
    std::uint8_t hasScaleResolutionDownBy;
    WebRtcOptionalString scalabilityMode;
    std::int32_t resolutionWidth;
    std::int32_t resolutionHeight;
    std::uint8_t hasResolution;
    std::uint8_t active;
    WebRtcGuestString rid;
    std::uint8_t requestKeyFrame;
    std::uint8_t adaptivePtime;
    WebRtcOptionalRtpCodec codec;
};

struct WebRtcRtpTransceiverInit {
    std::int32_t direction;
    WebRtcGuestStringVector streamIds;
    WebRtcRtpEncodingParametersVector sendEncodings;
};

static_assert(sizeof(WebRtcRtpEncodingParametersVector) == 24);
static_assert(sizeof(WebRtcOptionalString) == 48);
static_assert(sizeof(WebRtcRtcpFeedback) == 12);
static_assert(sizeof(WebRtcRtcpFeedbackVector) == 24);
static_assert(sizeof(WebRtcRtpCodec) == 120);
static_assert(offsetof(WebRtcRtpCodec, name) == 8);
static_assert(offsetof(WebRtcRtpCodec, rtcpFeedback) == 72);
static_assert(offsetof(WebRtcRtpCodec, parameters) == 96);
static_assert(sizeof(WebRtcOptionalRtpCodec) == 128);
static_assert(offsetof(WebRtcOptionalRtpCodec, engaged) == 120);
static_assert(sizeof(WebRtcRtpEncodingParameters) == 320);
static_assert(offsetof(WebRtcRtpEncodingParameters, scalabilityMode) == 80);
static_assert(offsetof(WebRtcRtpEncodingParameters, rid) == 144);
static_assert(offsetof(WebRtcRtpEncodingParameters, codec) == 192);
static_assert(sizeof(WebRtcRtpTransceiverInit) == 56);
static_assert(offsetof(WebRtcRtpTransceiverInit, streamIds) == 8);
static_assert(offsetof(WebRtcRtpTransceiverInit, sendEncodings) == 32);
