#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcVideoSinkFrameSize {
    std::int32_t width;
    std::int32_t height;
};

struct WebRtcVideoSinkWants {
    bool rotationApplied;
    bool blackFrames;
    std::int32_t maxPixelCount;
    std::int32_t targetPixelCount;
    bool hasTargetPixelCount;
    std::int32_t maxFramerateFps;
    std::int32_t resolutionAlignment;
    WebRtcVideoSinkFrameSize* resolutionsBegin;
    WebRtcVideoSinkFrameSize* resolutionsEnd;
    WebRtcVideoSinkFrameSize* resolutionsCapacity;
    WebRtcVideoSinkFrameSize requestedResolution;
    bool hasRequestedResolution;
    std::uint8_t requestedResolutionPadding[3];
    bool isActive;
    bool anyActiveWithoutRequestedResolution;
    bool hasAggregates;
};

static_assert(sizeof(WebRtcVideoSinkFrameSize) == 8);
static_assert(offsetof(WebRtcVideoSinkFrameSize, width) == 0);
static_assert(offsetof(WebRtcVideoSinkFrameSize, height) == 4);
static_assert(sizeof(WebRtcVideoSinkWants) == 64 && alignof(WebRtcVideoSinkWants) == 8);
static_assert(offsetof(WebRtcVideoSinkWants, rotationApplied) == 0);
static_assert(offsetof(WebRtcVideoSinkWants, blackFrames) == 1);
static_assert(offsetof(WebRtcVideoSinkWants, maxPixelCount) == 4);
static_assert(offsetof(WebRtcVideoSinkWants, targetPixelCount) == 8);
static_assert(offsetof(WebRtcVideoSinkWants, hasTargetPixelCount) == 12);
static_assert(offsetof(WebRtcVideoSinkWants, maxFramerateFps) == 16);
static_assert(offsetof(WebRtcVideoSinkWants, resolutionAlignment) == 20);
static_assert(offsetof(WebRtcVideoSinkWants, resolutionsBegin) == 24);
static_assert(offsetof(WebRtcVideoSinkWants, resolutionsEnd) == 32);
static_assert(offsetof(WebRtcVideoSinkWants, resolutionsCapacity) == 40);
static_assert(offsetof(WebRtcVideoSinkWants, requestedResolution) == 48);
static_assert(offsetof(WebRtcVideoSinkWants, hasRequestedResolution) == 56);
static_assert(offsetof(WebRtcVideoSinkWants, isActive) == 60);
static_assert(offsetof(WebRtcVideoSinkWants, anyActiveWithoutRequestedResolution) == 61);
static_assert(offsetof(WebRtcVideoSinkWants, hasAggregates) == 62);
