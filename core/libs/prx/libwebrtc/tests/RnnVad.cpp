#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

using ConstructFn = void (APS5_VABI*)(WebRtcRnnVad*, const WebRtcAvailableCpuFeatures*);
using DestroyFn = void (APS5_VABI*)(WebRtcRnnVad*);
using ComputeFn = float (APS5_VABI*)(WebRtcRnnVad*, const float*, bool);

const auto construct = reinterpret_cast<ConstructFn>(&_ZN6webrtc7rnn_vad6RnnVadC1ERKNS_20AvailableCpuFeaturesE_nid_postfix);
const auto destroy = reinterpret_cast<DestroyFn>(&_ZN6webrtc7rnn_vad6RnnVadD1Ev_nid_postfix);
const auto compute = reinterpret_cast<ComputeFn>(&_ZN6webrtc7rnn_vad6RnnVad21ComputeVadProbabilityEN3rtc9ArrayViewIKfLl42EEEb_nid_postfix);

constexpr float Features[WebRtcRnnVadFeatureVectorSize] = {
    -1.00131f,   -0.627069f, -7.81097f,  7.86285f,  -2.87145f,  3.32365f,   -0.653161f, 0.529839f,  -0.425307f, 0.25583f,  0.235094f,
    0.230527f,   -0.144687f, 0.182785f,  0.57102f,  0.125039f,  0.479482f,  -0.0255439f, -0.0073141f, -0.147346f, -0.217106f, -0.0846906f,
    -8.34943f,   3.09065f,   1.42628f,   -0.85235f, -0.220207f, -0.811163f, 2.09032f,   -2.01425f,  -0.690268f, -0.925327f, -0.541354f,
    0.58455f,    -0.606726f, -0.0372358f, 0.565991f, 0.435854f, 0.420812f,  0.162198f,  -2.13f,     10.0089f};

constexpr float InputLayerOutput[WebRtcRnnVadMaxLayerUnits] = {
    -0.623293f, -0.988299f, 0.999378f, 0.967168f, 0.103087f, -0.978545f, -0.856347f, 0.346675f, 1.f,       -0.717442f, -0.544176f, 0.960363f,
    0.983443f,  0.999991f,  -0.824335f, 0.984742f, 0.990208f, 0.938179f,  0.875092f,  0.999846f, 0.997707f, -0.999382f, 0.973153f,  -0.966605f};

constexpr float Probabilities[] = {0x1.37754cp-3f, 0x1.706e5p-5f, 0x1.7a9cp-6f,  0x1.04328p-6f, 0x1.99cbcp-7f, 0x1.58f98p-7f,
                                   0x1.2a254p-7f,  0x1.03d44p-7f, 0x1.c5fb8p-8f, 0x1.8d2fp-8f,  0x1.5c2d8p-8f};

constexpr WebRtcAvailableCpuFeatures NoFeatures{false, false, false};
constexpr WebRtcAvailableCpuFeatures Sse2{true, false, false};
constexpr WebRtcAvailableCpuFeatures Avx2{false, true, false};

bool Same(const WebRtcAvailableCpuFeatures& left, const WebRtcAvailableCpuFeatures& right) {
    return left.sse2 == right.sse2 && left.avx2 == right.avx2 && left.neon == right.neon;
}

std::size_t Size(const WebRtcRnnVadFloatVector& vector) {
    return static_cast<std::size_t>(vector.end - vector.begin);
}

bool Zero(const float* values) {
    for (std::size_t index = 0; index < WebRtcRnnVadMaxLayerUnits; ++index)
        if (values[index] != 0.f) return false;
    return true;
}

void Construct(WebRtcRnnVad& vad, const WebRtcAvailableCpuFeatures& features) {
    std::memset(&vad, 0x5a, sizeof(vad));
    construct(&vad, &features);
}

void TestConstruct() {
    const WebRtcAvailableCpuFeatures features{true, true, true};
    WebRtcRnnVad vad;
    Construct(vad, features);
    Require(vad.input.inputSize == 42 && vad.input.outputSize == 24);
    Require(Size(vad.input.bias) == 24 && vad.input.bias.capacity == vad.input.bias.end);
    Require(Size(vad.input.weights) == 42 * 24 && vad.input.weights.capacity == vad.input.weights.end);
    Require(vad.input.bias.begin[0] == 38.f / 256.f && vad.input.bias.begin[14] == -128.f / 256.f && vad.input.bias.begin[23] == -20.f / 256.f);
    Require(vad.input.weights.begin[0] == -10.f / 256.f && vad.input.weights.begin[1] == -7.f / 256.f && vad.input.weights.begin[42] == 0.f);
    Require(Same(vad.input.cpuFeatures, features));
    Require(vad.input.activation.function && vad.input.activation.call);
    Require(vad.input.activation.call(vad.input.activation.function, 0.f) == 0.f);
    Require(vad.input.activation.call(vad.input.activation.function, 9.f) == 1.f);
    Require(vad.hidden.inputSize == 24 && vad.hidden.outputSize == 24);
    Require(Size(vad.hidden.bias) == 72 && Size(vad.hidden.weights) == 1728 && Size(vad.hidden.recurrentWeights) == 1728);
    Require(vad.hidden.bias.begin[0] == 124.f / 256.f && vad.hidden.bias.begin[71] == 62.f / 256.f);
    Require(Same(vad.hidden.cpuFeatures, features));
    Require(Zero(vad.hidden.state));
    Require(vad.output.inputSize == 24 && vad.output.outputSize == 1);
    Require(Size(vad.output.bias) == 1 && vad.output.bias.begin[0] == -50.f / 256.f);
    Require(Size(vad.output.weights) == 24 && vad.output.weights.begin[0] == 127.f / 256.f && vad.output.weights.begin[23] == 80.f / 256.f);
    Require(Same(vad.output.cpuFeatures, NoFeatures));
    Require(vad.output.activation.call(vad.output.activation.function, 0.f) == 0.5f);
    destroy(&vad);
}

void TestProbabilities(const WebRtcAvailableCpuFeatures& features, float tolerance) {
    WebRtcRnnVad vad;
    Construct(vad, features);
    for (const float expected : Probabilities) {
        const float probability = compute(&vad, Features, false);
        Require(std::fabs(probability - expected) <= tolerance);
        Require(probability == vad.output.output[0]);
    }
    for (std::size_t index = 0; index < WebRtcRnnVadMaxLayerUnits; ++index)
        Require(std::fabs(vad.input.output[index] - InputLayerOutput[index]) <= 1e-5f);
    destroy(&vad);
}

void TestSilence(const WebRtcAvailableCpuFeatures& features) {
    WebRtcRnnVad vad;
    Construct(vad, features);
    for (int frame = 0; frame < 10; ++frame)
        compute(&vad, Features, false);
    const float before = compute(&vad, Features, false);
    Require(!Zero(vad.hidden.state));
    float inputOutput[WebRtcRnnVadMaxLayerUnits];
    std::memcpy(inputOutput, vad.input.output, sizeof(inputOutput));
    Require(compute(&vad, nullptr, true) == 0.f);
    Require(Zero(vad.hidden.state));
    Require(std::memcmp(inputOutput, vad.input.output, sizeof(inputOutput)) == 0);
    Require(vad.output.output[0] == before);
    for (int frame = 0; frame < 10; ++frame)
        compute(&vad, Features, false);
    Require(compute(&vad, Features, false) == before);
    destroy(&vad);
}

void TestInvalid() {
    WebRtcRnnVad vad;
    ExpectErr<std::invalid_argument>([] { construct(nullptr, &NoFeatures); });
    ExpectErr<std::invalid_argument>([&] { construct(&vad, nullptr); });
    std::memset(&vad, 0, sizeof(vad));
    ExpectErr<std::invalid_argument>([&] { compute(&vad, Features, false); });
    ExpectErr<std::invalid_argument>([&] { compute(&vad, nullptr, true); });
    ExpectErr<std::invalid_argument>([&] { destroy(&vad); });
    ExpectErr<std::invalid_argument>([] { compute(nullptr, Features, false); });
    ExpectErr<std::invalid_argument>([] { destroy(nullptr); });
    Construct(vad, NoFeatures);
    ExpectErr<std::invalid_argument>([&] { compute(&vad, nullptr, false); });
    destroy(&vad);
}

}

int main() {
    TestConstruct();
    TestProbabilities(NoFeatures, 1e-7f);
    TestProbabilities(Sse2, 1e-5f);
    TestProbabilities(Avx2, 1e-5f);
    TestSilence(NoFeatures);
    TestSilence(Sse2);
    TestSilence(Avx2);
    TestInvalid();
    std::puts("libwebrtc RnnVad tests passed");
    return 0;
}
