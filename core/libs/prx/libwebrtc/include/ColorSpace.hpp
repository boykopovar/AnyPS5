#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcHdrMetadata {
    float primaryR[2];
    float primaryG[2];
    float primaryB[2];
    float whitePoint[2];
    float luminanceMax;
    float luminanceMin;
    std::int32_t maxContentLightLevel;
    std::int32_t maxFrameAverageLightLevel;
};

struct WebRtcColorSpace {
    std::uint8_t primaries;
    std::uint8_t transfer;
    std::uint8_t matrix;
    std::int32_t range;
    std::int32_t chromaSitingHorizontal;
    std::int32_t chromaSitingVertical;
    WebRtcHdrMetadata hdrMetadata;
    bool hasHdrMetadata;
};

static_assert(sizeof(WebRtcHdrMetadata) == 48);
static_assert(sizeof(WebRtcColorSpace) == 68);
static_assert(offsetof(WebRtcColorSpace, range) == 4);
static_assert(offsetof(WebRtcColorSpace, chromaSitingVertical) == 12);
static_assert(offsetof(WebRtcColorSpace, hdrMetadata) == 16);
static_assert(offsetof(WebRtcColorSpace, hasHdrMetadata) == 64);
