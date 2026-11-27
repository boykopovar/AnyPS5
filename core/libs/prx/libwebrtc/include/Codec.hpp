#pragma once

#include "prx/libwebrtc/include/RtpTransceiverInit.hpp"
#include "prx/libwebrtc/include/SdpVideoFormat.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::int32_t WebRtcCodecTypeVideo = 1;
constexpr std::int32_t WebRtcCodecIdNotSet = -1;
constexpr std::int32_t WebRtcVideoCodecClockrate = 90000;

struct WebRtcFeedbackParam {
    WebRtcGuestString id;
    WebRtcGuestString param;
};

struct WebRtcFeedbackParamVector {
    WebRtcFeedbackParam* begin;
    WebRtcFeedbackParam* end;
    WebRtcFeedbackParam* capacityEnd;
};

struct WebRtcRtpCodecParameters {
    WebRtcRtpCodec codec;
    std::int32_t payloadType;
};

static_assert(offsetof(WebRtcRtpCodecParameters, payloadType) == 120);
static_assert(sizeof(WebRtcRtpCodecParameters) == 128);

struct WebRtcCodec {
    const void* vtable;
    std::int32_t type;
    std::int32_t id;
    WebRtcGuestString name;
    std::int32_t clockrate;
    std::int32_t bitrate;
    std::uint64_t channels;
    WebRtcOptionalString packetization;
    WebRtcScalabilityModeVector scalabilityModes;
    WebRtcOptionalString txMode;
    WebRtcStringMap params;
    WebRtcFeedbackParamVector feedbackParams;
};

static_assert(sizeof(WebRtcFeedbackParam) == 80);
static_assert(sizeof(WebRtcFeedbackParamVector) == 24);
static_assert(sizeof(WebRtcCodec) == 264);
static_assert(offsetof(WebRtcCodec, type) == 8);
static_assert(offsetof(WebRtcCodec, id) == 12);
static_assert(offsetof(WebRtcCodec, name) == 16);
static_assert(offsetof(WebRtcCodec, clockrate) == 56);
static_assert(offsetof(WebRtcCodec, bitrate) == 60);
static_assert(offsetof(WebRtcCodec, channels) == 64);
static_assert(offsetof(WebRtcCodec, packetization) == 72);
static_assert(offsetof(WebRtcCodec, scalabilityModes) == 120);
static_assert(offsetof(WebRtcCodec, txMode) == 168);
static_assert(offsetof(WebRtcCodec, params) == 216);
static_assert(offsetof(WebRtcCodec, feedbackParams) == 240);

void APS5_VABI DestroyRtpCodecParameters_nid_no_patch(WebRtcRtpCodecParameters* parameters);
