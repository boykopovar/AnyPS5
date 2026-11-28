#pragma once

#include <cstddef>

struct WebRtcSdpVideoFormat;
struct WebRtcVideoDecoderFactory;

struct WebRtcVideoDecoderFactoryCodecSupport {
    bool isSupported;
    bool isPowerEfficient;
};

static_assert(sizeof(WebRtcVideoDecoderFactoryCodecSupport) == 2);
static_assert(offsetof(WebRtcVideoDecoderFactoryCodecSupport, isPowerEfficient) == 1);
