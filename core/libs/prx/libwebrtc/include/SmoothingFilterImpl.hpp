#pragma once

#include <cstddef>
#include <cstdint>
#include "prx/libwebrtc/include/AudioEncoder.hpp"

struct WebRtcSmoothingFilterImpl {
    void* const* vtable;
    std::int32_t initTimeMilliseconds;
    float initFactor;
    float initConst;
    std::int64_t initEndTimeMilliseconds;
    bool hasInitEndTime;
    unsigned char initEndTimePadding[7];
    float lastSample;
    float alpha;
    float state;
    std::int64_t lastStateTimeMilliseconds;
};

static_assert(sizeof(WebRtcSmoothingFilterImpl) == 64);
static_assert(offsetof(WebRtcSmoothingFilterImpl, vtable) == 0);
static_assert(offsetof(WebRtcSmoothingFilterImpl, initTimeMilliseconds) == 8);
static_assert(offsetof(WebRtcSmoothingFilterImpl, initFactor) == 12);
static_assert(offsetof(WebRtcSmoothingFilterImpl, initConst) == 16);
static_assert(offsetof(WebRtcSmoothingFilterImpl, initEndTimeMilliseconds) == 24);
static_assert(offsetof(WebRtcSmoothingFilterImpl, hasInitEndTime) == 32);
static_assert(offsetof(WebRtcSmoothingFilterImpl, lastSample) == 40);
static_assert(offsetof(WebRtcSmoothingFilterImpl, alpha) == 44);
static_assert(offsetof(WebRtcSmoothingFilterImpl, state) == 48);
static_assert(offsetof(WebRtcSmoothingFilterImpl, lastStateTimeMilliseconds) == 56);
