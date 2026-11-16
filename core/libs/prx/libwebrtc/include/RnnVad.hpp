#pragma once

#include "prx/libwebrtc/include/CpuFeatures.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcRnnVadFeatureVectorSize = 42;
constexpr std::size_t WebRtcRnnVadMaxLayerUnits = 24;

struct WebRtcRnnVadFloatVector {
    float* begin;
    float* end;
    float* capacity;
};

struct WebRtcRnnVadActivation {
    float (*function)(float);
    float (*call)(float (*)(float), float);
};

struct WebRtcRnnVadFullyConnectedLayer {
    std::int32_t inputSize;
    std::int32_t outputSize;
    WebRtcRnnVadFloatVector bias;
    WebRtcRnnVadFloatVector weights;
    WebRtcAvailableCpuFeatures cpuFeatures;
    WebRtcRnnVadActivation activation;
    float output[WebRtcRnnVadMaxLayerUnits];
};

struct WebRtcRnnVadGatedRecurrentLayer {
    std::int32_t inputSize;
    std::int32_t outputSize;
    WebRtcRnnVadFloatVector bias;
    WebRtcRnnVadFloatVector weights;
    WebRtcRnnVadFloatVector recurrentWeights;
    WebRtcAvailableCpuFeatures cpuFeatures;
    float state[WebRtcRnnVadMaxLayerUnits];
};

struct WebRtcRnnVad {
    WebRtcRnnVadFullyConnectedLayer input;
    WebRtcRnnVadGatedRecurrentLayer hidden;
    WebRtcRnnVadFullyConnectedLayer output;
};

static_assert(sizeof(WebRtcRnnVadFloatVector) == 24);
static_assert(sizeof(WebRtcRnnVadActivation) == 16);
static_assert(sizeof(WebRtcRnnVadFullyConnectedLayer) == 176);
static_assert(offsetof(WebRtcRnnVadFullyConnectedLayer, bias) == 8);
static_assert(offsetof(WebRtcRnnVadFullyConnectedLayer, weights) == 32);
static_assert(offsetof(WebRtcRnnVadFullyConnectedLayer, cpuFeatures) == 56);
static_assert(offsetof(WebRtcRnnVadFullyConnectedLayer, activation) == 64);
static_assert(offsetof(WebRtcRnnVadFullyConnectedLayer, output) == 80);
static_assert(sizeof(WebRtcRnnVadGatedRecurrentLayer) == 184);
static_assert(offsetof(WebRtcRnnVadGatedRecurrentLayer, recurrentWeights) == 56);
static_assert(offsetof(WebRtcRnnVadGatedRecurrentLayer, cpuFeatures) == 80);
static_assert(offsetof(WebRtcRnnVadGatedRecurrentLayer, state) == 84);
static_assert(sizeof(WebRtcRnnVad) == 536);
static_assert(offsetof(WebRtcRnnVad, hidden) == 176);
static_assert(offsetof(WebRtcRnnVad, output) == 360);
