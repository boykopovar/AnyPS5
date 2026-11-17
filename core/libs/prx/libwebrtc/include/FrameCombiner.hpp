#pragma once

#include <cstddef>
#include <cstdint>

#include "prx/libwebrtc/include/AudioFrame.hpp"

constexpr std::size_t WebRtcFrameCombinerMaxChannels = 8;
constexpr std::size_t WebRtcFrameCombinerMaxChannelSize = 480;
constexpr std::size_t WebRtcLimiterSubFrames = 20;

struct WebRtcGainCurveStats {
    std::size_t lookUpsIdentityRegion;
    std::size_t lookUpsKneeRegion;
    std::size_t lookUpsLimiterRegion;
    std::size_t lookUpsSaturationRegion;
    bool available;
    std::int32_t region;
    std::int64_t regionDurationFrames;
};

struct WebRtcInterpolatedGainCurve {
    void* identityHistogram;
    void* kneeHistogram;
    void* limiterHistogram;
    void* saturationHistogram;
    void* apmDataDumper;
    WebRtcGainCurveStats stats;
};

struct WebRtcFixedDigitalLevelEstimator {
    void* apmDataDumper;
    float filterStateLevel;
    std::int32_t samplesInFrame;
    std::int32_t samplesInSubFrame;
};

struct WebRtcLimiter {
    WebRtcInterpolatedGainCurve interpGainCurve;
    WebRtcFixedDigitalLevelEstimator levelEstimator;
    void* apmDataDumper;
    float scalingFactors[WebRtcLimiterSubFrames + 1];
    float perSampleScalingFactors[WebRtcFrameCombinerMaxChannelSize];
    float lastScalingFactor;
};

struct WebRtcFrameCombiner {
    void* dataDumper;
    WebRtcLimiter limiter;
    bool useLimiter;
    float mixingBuffer[WebRtcFrameCombinerMaxChannelSize * WebRtcFrameCombinerMaxChannels];
};

struct WebRtcAudioFrameList {
    WebRtcAudioFrame* const* data;
    std::size_t size;
};

static_assert(sizeof(WebRtcGainCurveStats) == 48);
static_assert(offsetof(WebRtcGainCurveStats, available) == 32);
static_assert(offsetof(WebRtcGainCurveStats, region) == 36);
static_assert(offsetof(WebRtcGainCurveStats, regionDurationFrames) == 40);
static_assert(sizeof(WebRtcInterpolatedGainCurve) == 88);
static_assert(offsetof(WebRtcInterpolatedGainCurve, stats) == 40);
static_assert(sizeof(WebRtcFixedDigitalLevelEstimator) == 24);
static_assert(sizeof(WebRtcLimiter) == 2128);
static_assert(offsetof(WebRtcLimiter, levelEstimator) == 88);
static_assert(offsetof(WebRtcLimiter, apmDataDumper) == 112);
static_assert(offsetof(WebRtcLimiter, scalingFactors) == 120);
static_assert(offsetof(WebRtcLimiter, perSampleScalingFactors) == 204);
static_assert(offsetof(WebRtcLimiter, lastScalingFactor) == 2124);
static_assert(sizeof(WebRtcFrameCombiner) == 17504);
static_assert(offsetof(WebRtcFrameCombiner, limiter) == 8);
static_assert(offsetof(WebRtcFrameCombiner, useLimiter) == 2136);
static_assert(offsetof(WebRtcFrameCombiner, mixingBuffer) == 2140);
static_assert(sizeof(WebRtcAudioFrameList) == 16);
