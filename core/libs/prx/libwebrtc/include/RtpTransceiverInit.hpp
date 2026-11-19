#pragma once

#include "prx/libwebrtc/include/IceServer.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::int32_t WebRtcRtpTransceiverDirectionSendRecv = 0;

struct WebRtcRtpEncodingParametersVector {
    void* begin;
    void* end;
    void* capacityEnd;
};

struct WebRtcRtpTransceiverInit {
    std::int32_t direction;
    WebRtcGuestStringVector streamIds;
    WebRtcRtpEncodingParametersVector sendEncodings;
};

static_assert(sizeof(WebRtcRtpEncodingParametersVector) == 24);
static_assert(sizeof(WebRtcRtpTransceiverInit) == 56);
static_assert(offsetof(WebRtcRtpTransceiverInit, streamIds) == 8);
static_assert(offsetof(WebRtcRtpTransceiverInit, sendEncodings) == 32);
