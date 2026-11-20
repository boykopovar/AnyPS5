#pragma once

#include <cstddef>
#include <cstdint>

#include "prx/libwebrtc/include/Resampler.hpp"

struct WebRtcStreamConfig {
    int sampleRateHz;
    std::size_t numChannels;
    std::size_t numFrames;
};

struct WebRtcFloatView {
    float* data;
    std::size_t size;
};

struct WebRtcFloatViewVector {
    WebRtcFloatView* begin;
    WebRtcFloatView* end;
    WebRtcFloatView* capacity;
};

struct WebRtcFloatViewTable {
    WebRtcFloatViewVector* begin;
    WebRtcFloatViewVector* end;
    WebRtcFloatViewVector* capacity;
};

struct WebRtcChannelBufferFloat {
    float* data;
    float** channels;
    float** bands;
    std::size_t numFrames;
    std::size_t numFramesPerBand;
    std::size_t numAllocatedChannels;
    std::size_t numChannels;
    std::size_t numBands;
    WebRtcFloatViewTable bandsView;
    WebRtcFloatViewTable channelsView;
};

struct WebRtcTwoBandsStates {
    int analysisState1[6];
    int analysisState2[6];
    int synthesisState1[6];
    int synthesisState2[6];
};

struct WebRtcThreeBandFilterBank {
    float stateAnalysis[10][15];
    float stateSynthesis[10][15];
};

struct WebRtcSplittingFilter {
    std::size_t numBands;
    WebRtcTwoBandsStates* twoBandsBegin;
    WebRtcTwoBandsStates* twoBandsEnd;
    WebRtcTwoBandsStates* twoBandsCapacity;
    WebRtcThreeBandFilterBank* threeBandsBegin;
    WebRtcThreeBandFilterBank* threeBandsEnd;
    WebRtcThreeBandFilterBank* threeBandsCapacity;
};

struct WebRtcResamplerVector {
    WebRtcPushSincResampler** begin;
    WebRtcPushSincResampler** end;
    WebRtcPushSincResampler** capacity;
};

struct WebRtcAudioBuffer {
    const void* vtable;
    std::size_t inputNumFrames;
    std::size_t inputNumChannels;
    std::size_t bufferNumFrames;
    std::size_t bufferNumChannels;
    std::size_t outputNumFrames;
    std::size_t outputNumChannels;
    std::size_t numChannels;
    std::size_t numBands;
    std::size_t numSplitFrames;
    WebRtcChannelBufferFloat* data;
    WebRtcChannelBufferFloat* splitData;
    WebRtcSplittingFilter* splittingFilter;
    WebRtcResamplerVector inputResamplers;
    WebRtcResamplerVector outputResamplers;
    bool downmixByAveraging;
    std::size_t channelForDownmixing;
};

static_assert(sizeof(WebRtcStreamConfig) == 24);
static_assert(offsetof(WebRtcStreamConfig, numChannels) == 8);
static_assert(offsetof(WebRtcStreamConfig, numFrames) == 16);
static_assert(sizeof(WebRtcFloatView) == 16);
static_assert(sizeof(WebRtcChannelBufferFloat) == 112);
static_assert(offsetof(WebRtcChannelBufferFloat, numFrames) == 24);
static_assert(offsetof(WebRtcChannelBufferFloat, numBands) == 56);
static_assert(offsetof(WebRtcChannelBufferFloat, bandsView) == 64);
static_assert(offsetof(WebRtcChannelBufferFloat, channelsView) == 88);
static_assert(sizeof(WebRtcTwoBandsStates) == 96);
static_assert(sizeof(WebRtcThreeBandFilterBank) == 1200);
static_assert(sizeof(WebRtcSplittingFilter) == 56);
static_assert(offsetof(WebRtcSplittingFilter, threeBandsBegin) == 32);
static_assert(sizeof(WebRtcAudioBuffer) == 168);
static_assert(offsetof(WebRtcAudioBuffer, numChannels) == 56);
static_assert(offsetof(WebRtcAudioBuffer, data) == 80);
static_assert(offsetof(WebRtcAudioBuffer, splitData) == 88);
static_assert(offsetof(WebRtcAudioBuffer, splittingFilter) == 96);
static_assert(offsetof(WebRtcAudioBuffer, inputResamplers) == 104);
static_assert(offsetof(WebRtcAudioBuffer, outputResamplers) == 128);
static_assert(offsetof(WebRtcAudioBuffer, downmixByAveraging) == 152);
static_assert(offsetof(WebRtcAudioBuffer, channelForDownmixing) == 160);
