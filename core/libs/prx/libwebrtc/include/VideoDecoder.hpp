#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>

struct WebRtcVideoDecoder;

struct WebRtcVideoDecoderInfo {
    WebRtcGuestString implementationName;
    bool isHardwareAccelerated;
};

static_assert(sizeof(WebRtcVideoDecoderInfo) == 48);
static_assert(offsetof(WebRtcVideoDecoderInfo, isHardwareAccelerated) == 40);
