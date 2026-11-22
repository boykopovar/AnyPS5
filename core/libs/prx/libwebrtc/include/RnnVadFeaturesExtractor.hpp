#pragma once

#include "prx/libwebrtc/include/CpuFeatures.hpp"
#include "prx/libwebrtc/include/RnnVad.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcRnnVadPitchBufSize24kHz = 864;
constexpr std::size_t WebRtcRnnVadFrameSize10ms24kHz = 240;
constexpr std::size_t WebRtcRnnVadFrameSize20ms24kHz = 480;
constexpr std::size_t WebRtcRnnVadRefineNumLags24kHz = 385;
constexpr std::size_t WebRtcRnnVadPitchBufSize12kHz = 432;
constexpr std::size_t WebRtcRnnVadNumLags12kHz = 147;
constexpr std::size_t WebRtcRnnVadOpusBands24kHz = 20;
constexpr std::size_t WebRtcRnnVadNumBands = 22;
constexpr std::size_t WebRtcRnnVadCepstralHistorySize = 8;
constexpr std::size_t WebRtcRnnVadAutoCorrFftSize = 512;

struct WebRtcBiQuadFilter {
    float configB[3];
    float configA[2];
    float stateB[2];
    float stateA[2];
};

struct WebRtcPffftFloatBuffer {
    std::size_t size;
    float* data;
};

struct WebRtcPffft {
    std::size_t fftSize;
    std::int32_t fftType;
    void* pffftStatus;
    float* scratchBuffer;
};

struct WebRtcPitchInfo {
    int period;
    float strength;
};

struct WebRtcAutoCorrelationCalculator {
    WebRtcPffft fft;
    WebRtcPffftFloatBuffer* tmp;
    WebRtcPffftFloatBuffer* x;
    WebRtcPffftFloatBuffer* h;
};

struct WebRtcPitchEstimator {
    WebRtcAvailableCpuFeatures cpuFeatures;
    WebRtcPitchInfo lastPitch48kHz;
    WebRtcAutoCorrelationCalculator autoCorrCalculator;
    WebRtcRnnVadFloatVector yEnergy24kHz;
    WebRtcRnnVadFloatVector pitchBuffer12kHz;
    WebRtcRnnVadFloatVector autoCorrelation12kHz;
};

struct WebRtcSpectralFeaturesExtractor {
    float halfWindow[WebRtcRnnVadFrameSize20ms24kHz / 2];
    WebRtcPffft fft;
    WebRtcPffftFloatBuffer* fftBuffer;
    WebRtcPffftFloatBuffer* referenceFrameFft;
    WebRtcPffftFloatBuffer* laggedFrameFft;
    WebRtcRnnVadFloatVector weights;
    float referenceFrameBandsEnergy[WebRtcRnnVadOpusBands24kHz];
    float laggedFrameBandsEnergy[WebRtcRnnVadOpusBands24kHz];
    float bandsCrossCorr[WebRtcRnnVadOpusBands24kHz];
    float dctTable[WebRtcRnnVadNumBands * WebRtcRnnVadNumBands];
    int cepstralCoeffsRingBufTail;
    float cepstralCoeffsRingBuf[WebRtcRnnVadNumBands * WebRtcRnnVadCepstralHistorySize];
    float cepstralDiffsBuf[(WebRtcRnnVadCepstralHistorySize - 1) * (WebRtcRnnVadCepstralHistorySize - 1)];
};

struct WebRtcRnnVadFeaturesExtractor {
    bool useHighPassFilter;
    WebRtcBiQuadFilter hpf;
    WebRtcRnnVadFloatVector pitchBuf24kHz;
    const float* pitchBuf24kHzView;
    WebRtcRnnVadFloatVector lpResidual;
    float* lpResidualView;
    WebRtcPitchEstimator pitchEstimator;
    const float* referenceFrameView;
    WebRtcSpectralFeaturesExtractor spectralFeaturesExtractor;
    int pitchPeriod48kHz;
};

static_assert(sizeof(WebRtcBiQuadFilter) == 36);
static_assert(sizeof(WebRtcPffftFloatBuffer) == 16);
static_assert(sizeof(WebRtcPffft) == 32);
static_assert(offsetof(WebRtcPffft, fftType) == 8);
static_assert(offsetof(WebRtcPffft, pffftStatus) == 16);
static_assert(offsetof(WebRtcPffft, scratchBuffer) == 24);
static_assert(sizeof(WebRtcPitchInfo) == 8);
static_assert(sizeof(WebRtcAutoCorrelationCalculator) == 56);
static_assert(offsetof(WebRtcAutoCorrelationCalculator, tmp) == 32);
static_assert(sizeof(WebRtcPitchEstimator) == 144);
static_assert(offsetof(WebRtcPitchEstimator, lastPitch48kHz) == 4);
static_assert(offsetof(WebRtcPitchEstimator, autoCorrCalculator) == 16);
static_assert(offsetof(WebRtcPitchEstimator, yEnergy24kHz) == 72);
static_assert(offsetof(WebRtcPitchEstimator, pitchBuffer12kHz) == 96);
static_assert(offsetof(WebRtcPitchEstimator, autoCorrelation12kHz) == 120);
static_assert(sizeof(WebRtcSpectralFeaturesExtractor) == 4120);
static_assert(offsetof(WebRtcSpectralFeaturesExtractor, fft) == 960);
static_assert(offsetof(WebRtcSpectralFeaturesExtractor, fftBuffer) == 992);
static_assert(offsetof(WebRtcSpectralFeaturesExtractor, weights) == 1016);
static_assert(offsetof(WebRtcSpectralFeaturesExtractor, referenceFrameBandsEnergy) == 1040);
static_assert(offsetof(WebRtcSpectralFeaturesExtractor, dctTable) == 1280);
static_assert(offsetof(WebRtcSpectralFeaturesExtractor, cepstralCoeffsRingBufTail) == 3216);
static_assert(offsetof(WebRtcSpectralFeaturesExtractor, cepstralDiffsBuf) == 3924);
static_assert(sizeof(WebRtcRnnVadFeaturesExtractor) == 4384);
static_assert(offsetof(WebRtcRnnVadFeaturesExtractor, hpf) == 4);
static_assert(offsetof(WebRtcRnnVadFeaturesExtractor, pitchBuf24kHz) == 40);
static_assert(offsetof(WebRtcRnnVadFeaturesExtractor, pitchBuf24kHzView) == 64);
static_assert(offsetof(WebRtcRnnVadFeaturesExtractor, lpResidual) == 72);
static_assert(offsetof(WebRtcRnnVadFeaturesExtractor, lpResidualView) == 96);
static_assert(offsetof(WebRtcRnnVadFeaturesExtractor, pitchEstimator) == 104);
static_assert(offsetof(WebRtcRnnVadFeaturesExtractor, referenceFrameView) == 248);
static_assert(offsetof(WebRtcRnnVadFeaturesExtractor, spectralFeaturesExtractor) == 256);
static_assert(offsetof(WebRtcRnnVadFeaturesExtractor, pitchPeriod48kHz) == 4376);
