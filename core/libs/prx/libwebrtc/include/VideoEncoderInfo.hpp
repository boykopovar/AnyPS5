#pragma once

#include "prx/libwebrtc/include/CodecSpecificInfo.hpp"
#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcMaxSpatialLayers = 5;
constexpr std::size_t WebRtcFpsAllocationInlineCapacity = 16;
constexpr std::size_t WebRtcPreferredPixelFormatsInlineCapacity = 5;

struct WebRtcResolutionBitrateLimits {
    std::int32_t frameSizePixels;
    std::int32_t minStartBitrateBps;
    std::int32_t minBitrateBps;
    std::int32_t maxBitrateBps;
};

struct WebRtcVideoEncoderInfo {
    std::int32_t qpThresholdLow;
    std::int32_t qpThresholdHigh;
    bool hasQpThresholds;
    std::int32_t minPixelsPerFrame;
    std::uint32_t requestedResolutionAlignment;
    bool applyAlignmentToAllSimulcastLayers;
    bool supportsNativeHandle;
    WebRtcGuestString implementationName;
    bool hasTrustedRateController;
    bool isHardwareAccelerated;
    WebRtcInlinedVector<WebRtcFpsAllocationInlineCapacity> fpsAllocation[WebRtcMaxSpatialLayers];
    WebRtcResolutionBitrateLimits* resolutionBitrateLimitsBegin;
    WebRtcResolutionBitrateLimits* resolutionBitrateLimitsEnd;
    WebRtcResolutionBitrateLimits* resolutionBitrateLimitsCapacity;
    bool supportsSimulcast;
    WebRtcInlinedVector<WebRtcPreferredPixelFormatsInlineCapacity * sizeof(std::int32_t)> preferredPixelFormats;
    bool isQpTrusted;
    bool hasIsQpTrusted;
    std::int32_t minQp;
    bool hasMinQp;
};

static_assert(sizeof(WebRtcResolutionBitrateLimits) == 16);
static_assert(sizeof(WebRtcInlinedVector<WebRtcFpsAllocationInlineCapacity>) == 24);
static_assert(sizeof(WebRtcInlinedVector<WebRtcPreferredPixelFormatsInlineCapacity * sizeof(std::int32_t)>) == 32);
static_assert(sizeof(WebRtcVideoEncoderInfo) == 272);
static_assert(offsetof(WebRtcVideoEncoderInfo, hasQpThresholds) == 8);
static_assert(offsetof(WebRtcVideoEncoderInfo, minPixelsPerFrame) == 12);
static_assert(offsetof(WebRtcVideoEncoderInfo, requestedResolutionAlignment) == 16);
static_assert(offsetof(WebRtcVideoEncoderInfo, applyAlignmentToAllSimulcastLayers) == 20);
static_assert(offsetof(WebRtcVideoEncoderInfo, supportsNativeHandle) == 21);
static_assert(offsetof(WebRtcVideoEncoderInfo, implementationName) == 24);
static_assert(offsetof(WebRtcVideoEncoderInfo, hasTrustedRateController) == 64);
static_assert(offsetof(WebRtcVideoEncoderInfo, isHardwareAccelerated) == 65);
static_assert(offsetof(WebRtcVideoEncoderInfo, fpsAllocation) == 72);
static_assert(offsetof(WebRtcVideoEncoderInfo, resolutionBitrateLimitsBegin) == 192);
static_assert(offsetof(WebRtcVideoEncoderInfo, supportsSimulcast) == 216);
static_assert(offsetof(WebRtcVideoEncoderInfo, preferredPixelFormats) == 224);
static_assert(offsetof(WebRtcVideoEncoderInfo, isQpTrusted) == 256);
static_assert(offsetof(WebRtcVideoEncoderInfo, hasIsQpTrusted) == 257);
static_assert(offsetof(WebRtcVideoEncoderInfo, minQp) == 260);
static_assert(offsetof(WebRtcVideoEncoderInfo, hasMinQp) == 264);
