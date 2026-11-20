#pragma once

#include "prx/libwebrtc/include/SdpAudioFormat.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcScalabilityModeCount = 34;

struct WebRtcScalabilityModeAllocation {
    std::uint8_t* data;
    std::uint64_t capacity;
};

struct WebRtcScalabilityModeVector {
    std::uint64_t sizeAndIsAllocated;
    union {
        std::uint8_t inlined[WebRtcScalabilityModeCount];
        WebRtcScalabilityModeAllocation allocated;
    };
};

struct WebRtcSdpVideoFormat {
    WebRtcGuestString name;
    WebRtcStringMap parameters;
    WebRtcScalabilityModeVector scalabilityModes;
};

static_assert(sizeof(WebRtcScalabilityModeVector) == 48);
static_assert(offsetof(WebRtcScalabilityModeVector, inlined) == 8);
static_assert(offsetof(WebRtcScalabilityModeVector, allocated) == 8);
static_assert(sizeof(WebRtcSdpVideoFormat) == 112);
static_assert(offsetof(WebRtcSdpVideoFormat, parameters) == 40);
static_assert(offsetof(WebRtcSdpVideoFormat, scalabilityModes) == 64);
