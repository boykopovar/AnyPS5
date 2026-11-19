#pragma once

#include "prx/libwebrtc/include/CodecSpecificInfo.hpp"
#include "prx/libwebrtc/include/SdpAudioFormat.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcScalabilityModeCount = 34;

struct WebRtcSdpVideoFormat {
    WebRtcGuestString name;
    WebRtcStringMap parameters;
    WebRtcInlinedVector<WebRtcScalabilityModeCount> scalabilityModes;
};

static_assert(sizeof(WebRtcInlinedVector<WebRtcScalabilityModeCount>) == 48);
static_assert(sizeof(WebRtcSdpVideoFormat) == 112);
static_assert(offsetof(WebRtcSdpVideoFormat, name) == 0);
static_assert(offsetof(WebRtcSdpVideoFormat, parameters) == 40);
static_assert(offsetof(WebRtcSdpVideoFormat, scalabilityModes) == 64);
