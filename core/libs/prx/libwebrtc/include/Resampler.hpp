#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcPushSincResampler {
    const void* vtable;
    void* resampler;
    float* floatBuffer;
    const float* sourcePtr;
    const std::int16_t* sourcePtrInt;
    std::size_t destinationFrames;
    bool firstPass;
    std::size_t sourceAvailable;
};

struct WebRtcMonoView16 {
    std::int16_t* data;
    std::size_t size;
};

struct WebRtcConstMonoView16 {
    const std::int16_t* data;
    std::size_t size;
};

struct WebRtcMutableInterleavedView16 {
    std::size_t numChannels;
    std::size_t samplesPerChannel;
    std::int16_t* data;
    std::size_t size;
};

struct WebRtcDeinterleavedView16 {
    std::size_t numChannels;
    std::size_t samplesPerChannel;
    std::int16_t* data;
    std::size_t size;
};

struct WebRtcPushResampler16 {
    std::int16_t* source;
    std::int16_t* destination;
    WebRtcDeinterleavedView16 sourceView;
    WebRtcDeinterleavedView16 destinationView;
    WebRtcPushSincResampler** resamplersBegin;
    WebRtcPushSincResampler** resamplersEnd;
    WebRtcPushSincResampler** resamplersCapacity;
};

static_assert(sizeof(WebRtcPushSincResampler) == 64);
static_assert(offsetof(WebRtcPushSincResampler, destinationFrames) == 40);
static_assert(offsetof(WebRtcPushSincResampler, firstPass) == 48);
static_assert(sizeof(WebRtcMutableInterleavedView16) == 32);
static_assert(sizeof(WebRtcPushResampler16) == 104);
static_assert(offsetof(WebRtcPushResampler16, sourceView) == 16);
static_assert(offsetof(WebRtcPushResampler16, destinationView) == 48);
static_assert(offsetof(WebRtcPushResampler16, resamplersBegin) == 80);
