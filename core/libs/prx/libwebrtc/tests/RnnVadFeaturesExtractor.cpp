#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

using ConstructFn = void (APS5_VABI*)(WebRtcRnnVadFeaturesExtractor*, const WebRtcAvailableCpuFeatures*);
using DestroyFn = void (APS5_VABI*)(WebRtcRnnVadFeaturesExtractor*);

const auto construct = reinterpret_cast<ConstructFn>(&_ZN6webrtc7rnn_vad17FeaturesExtractorC1ERKNS_20AvailableCpuFeaturesE_nid_postfix);
const auto destroy = reinterpret_cast<DestroyFn>(&_ZN6webrtc7rnn_vad17FeaturesExtractorD1Ev_nid_postfix);

constexpr WebRtcAvailableCpuFeatures AllFeatures{true, true, false};

std::size_t liveAllocations = 0;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer)
        ++liveAllocations;
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    if (pointer) {
        --liveAllocations;
        std::free(pointer);
    }
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    if (pointer)
        ++liveAllocations;
    return pointer;
}

void* APS5_VABI TestReallocate(void* pointer, std::size_t size) {
    void* updated = std::realloc(pointer, size);
    if (!pointer && updated)
        ++liveAllocations;
    else if (pointer && size == 0 && !updated)
        --liveAllocations;
    return updated;
}

void* APS5_VABI TestAlign(std::size_t, std::size_t size) {
    return TestAllocate(size);
}

void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) {
    return TestReallocate(pointer, size);
}

int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition)
        return;
    std::fprintf(stderr, "libwebrtc RnnVadFeaturesExtractor check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

std::size_t VectorSize(const WebRtcRnnVadFloatVector& vec) {
    return static_cast<std::size_t>(vec.end - vec.begin);
}

bool AllZeroFloats(const float* values, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        if (values[index] != 0.f)
            return false;
    }
    return true;
}

void CheckPffftBuffer(const WebRtcPffftFloatBuffer* buffer, std::size_t expectedSize) {
    Require(buffer != nullptr);
    Require(buffer->size == expectedSize);
    Require(buffer->data != nullptr);
    Require(AllZeroFloats(buffer->data, expectedSize));
}

void TestConstructDestroy() {
    liveAllocations = 0;
    WebRtcRnnVadFeaturesExtractor extractor;
    std::memset(&extractor, 0x5a, sizeof(extractor));
    errno = 123;
    construct(&extractor, &AllFeatures);
    Require(errno == 123);
    Require(liveAllocations > 0);

    Require(!extractor.useHighPassFilter);
    Require(std::fabs(extractor.hpf.configB[0] - 0.99446179f) < 1e-7f);
    Require(std::fabs(extractor.hpf.configB[1] - (-1.98892358f)) < 1e-7f);
    Require(std::fabs(extractor.hpf.configB[2] - 0.99446179f) < 1e-7f);
    Require(std::fabs(extractor.hpf.configA[0] - (-1.98889291f)) < 1e-7f);
    Require(std::fabs(extractor.hpf.configA[1] - 0.98895425f) < 1e-7f);
    Require(AllZeroFloats(extractor.hpf.stateB, 2));
    Require(AllZeroFloats(extractor.hpf.stateA, 2));

    Require(VectorSize(extractor.pitchBuf24kHz) == WebRtcRnnVadPitchBufSize24kHz);
    Require(extractor.pitchBuf24kHz.capacity == extractor.pitchBuf24kHz.end);
    Require(AllZeroFloats(extractor.pitchBuf24kHz.begin, WebRtcRnnVadPitchBufSize24kHz));
    Require(extractor.pitchBuf24kHzView == extractor.pitchBuf24kHz.begin);
    Require(VectorSize(extractor.lpResidual) == WebRtcRnnVadPitchBufSize24kHz);
    Require(extractor.lpResidual.capacity == extractor.lpResidual.end);
    Require(AllZeroFloats(extractor.lpResidual.begin, WebRtcRnnVadPitchBufSize24kHz));
    Require(extractor.lpResidualView == extractor.lpResidual.begin);
    Require(extractor.referenceFrameView == extractor.pitchBuf24kHz.begin + 384);

    Require(extractor.pitchEstimator.cpuFeatures.sse2);
    Require(extractor.pitchEstimator.cpuFeatures.avx2);
    Require(!extractor.pitchEstimator.cpuFeatures.neon);
    Require(extractor.pitchEstimator.lastPitch48kHz.period == 0);
    Require(extractor.pitchEstimator.lastPitch48kHz.strength == 0.f);
    Require(extractor.pitchEstimator.autoCorrCalculator.fft.fftSize == WebRtcRnnVadAutoCorrFftSize);
    Require(extractor.pitchEstimator.autoCorrCalculator.fft.fftType == 0);
    Require(extractor.pitchEstimator.autoCorrCalculator.fft.pffftStatus != nullptr);
    Require(extractor.pitchEstimator.autoCorrCalculator.fft.scratchBuffer != nullptr);
    CheckPffftBuffer(extractor.pitchEstimator.autoCorrCalculator.tmp, WebRtcRnnVadAutoCorrFftSize);
    CheckPffftBuffer(extractor.pitchEstimator.autoCorrCalculator.x, WebRtcRnnVadAutoCorrFftSize);
    CheckPffftBuffer(extractor.pitchEstimator.autoCorrCalculator.h, WebRtcRnnVadAutoCorrFftSize);
    Require(VectorSize(extractor.pitchEstimator.yEnergy24kHz) == WebRtcRnnVadRefineNumLags24kHz);
    Require(VectorSize(extractor.pitchEstimator.pitchBuffer12kHz) == WebRtcRnnVadPitchBufSize12kHz);
    Require(VectorSize(extractor.pitchEstimator.autoCorrelation12kHz) == WebRtcRnnVadNumLags12kHz);

    Require(extractor.spectralFeaturesExtractor.halfWindow[0] > 0.f);
    Require(extractor.spectralFeaturesExtractor.fft.fftSize == WebRtcRnnVadFrameSize20ms24kHz);
    Require(extractor.spectralFeaturesExtractor.fft.fftType == 0);
    Require(extractor.spectralFeaturesExtractor.fft.pffftStatus != nullptr);
    Require(extractor.spectralFeaturesExtractor.fft.scratchBuffer != nullptr);
    CheckPffftBuffer(extractor.spectralFeaturesExtractor.fftBuffer, WebRtcRnnVadFrameSize20ms24kHz);
    CheckPffftBuffer(extractor.spectralFeaturesExtractor.referenceFrameFft, WebRtcRnnVadFrameSize20ms24kHz);
    CheckPffftBuffer(extractor.spectralFeaturesExtractor.laggedFrameFft, WebRtcRnnVadFrameSize20ms24kHz);
    Require(VectorSize(extractor.spectralFeaturesExtractor.weights) == 240);
    Require(extractor.spectralFeaturesExtractor.weights.begin[0] == 0.f);
    Require(extractor.spectralFeaturesExtractor.weights.begin[1] == 0.25f);
    Require(extractor.spectralFeaturesExtractor.weights.begin[239] == 0.979167f);
    Require(AllZeroFloats(extractor.spectralFeaturesExtractor.referenceFrameBandsEnergy, WebRtcRnnVadOpusBands24kHz));
    Require(AllZeroFloats(extractor.spectralFeaturesExtractor.laggedFrameBandsEnergy, WebRtcRnnVadOpusBands24kHz));
    Require(AllZeroFloats(extractor.spectralFeaturesExtractor.bandsCrossCorr, WebRtcRnnVadOpusBands24kHz));
    Require(std::fabs(extractor.spectralFeaturesExtractor.dctTable[0] - std::sqrt(0.5f)) < 1e-6f);
    Require(extractor.spectralFeaturesExtractor.cepstralCoeffsRingBufTail == 0);
    Require(AllZeroFloats(extractor.spectralFeaturesExtractor.cepstralCoeffsRingBuf, WebRtcRnnVadNumBands * WebRtcRnnVadCepstralHistorySize));
    Require(AllZeroFloats(extractor.spectralFeaturesExtractor.cepstralDiffsBuf, 49));
    Require(extractor.pitchPeriod48kHz == 0);

    errno = 77;
    destroy(&extractor);
    Require(errno == 77);
    Require(liveAllocations == 0);
}

void TestInvalid() {
    liveAllocations = 0;
    WebRtcRnnVadFeaturesExtractor extractor;
    RequireInvalidArgument([] { construct(nullptr, &AllFeatures); });
    RequireInvalidArgument([&] { construct(&extractor, nullptr); });
    RequireInvalidArgument([] { destroy(nullptr); });
    Require(liveAllocations == 0);

    std::memset(&extractor, 0, sizeof(extractor));
    RequireInvalidArgument([&] { destroy(&extractor); });

    construct(&extractor, &AllFeatures);
    float* savedView = extractor.lpResidualView;
    extractor.lpResidualView = nullptr;
    RequireInvalidArgument([&] { destroy(&extractor); });
    extractor.lpResidualView = savedView;
    destroy(&extractor);
    Require(liveAllocations == 0);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestConstructDestroy();
    TestInvalid();
    std::puts("libwebrtc RnnVadFeaturesExtractor tests passed");
    return 0;
}
