/* Derived from modules/audio_processing/agc2/rnn_vad/features_extraction.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/agc2/rnn_vad/pitch_search.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/agc2/rnn_vad/spectral_features.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/agc2/rnn_vad/spectral_features_internal.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/agc2/rnn_vad/auto_correlation.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/agc2/biquad_filter.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/utility/pffft_wrapper.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestPtr.hpp"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr double Pi = 3.14159265358979323846;

constexpr WebRtcBiQuadFilter HpfConfig24k{
    {0.99446179f, -1.98892358f, 0.99446179f},
    {-1.98889291f, 0.98895425f},
    {0.f, 0.f},
    {0.f, 0.f},
};

constexpr std::array<float, WebRtcRnnVadFrameSize20ms24kHz / 2> OpusBandWeights24kHz20ms{{
    0.f,       0.25f,      0.5f,       0.75f,
    0.f,       0.25f,      0.5f,       0.75f,
    0.f,       0.25f,      0.5f,       0.75f,
    0.f,       0.25f,      0.5f,       0.75f,
    0.f,       0.25f,      0.5f,       0.75f,
    0.f,       0.25f,      0.5f,       0.75f,
    0.f,       0.25f,      0.5f,       0.75f,
    0.f,       0.25f,      0.5f,       0.75f,
    0.f,       0.125f,     0.25f,      0.375f,    0.5f,
    0.625f,    0.75f,      0.875f,
    0.f,       0.125f,     0.25f,      0.375f,    0.5f,
    0.625f,    0.75f,      0.875f,
    0.f,       0.125f,     0.25f,      0.375f,    0.5f,
    0.625f,    0.75f,      0.875f,
    0.f,       0.125f,     0.25f,      0.375f,    0.5f,
    0.625f,    0.75f,      0.875f,
    0.f,       0.0625f,    0.125f,     0.1875f,   0.25f,
    0.3125f,   0.375f,     0.4375f,    0.5f,      0.5625f,
    0.625f,    0.6875f,    0.75f,      0.8125f,   0.875f,
    0.9375f,
    0.f,       0.0625f,    0.125f,     0.1875f,   0.25f,
    0.3125f,   0.375f,     0.4375f,    0.5f,      0.5625f,
    0.625f,    0.6875f,    0.75f,      0.8125f,   0.875f,
    0.9375f,
    0.f,       0.0625f,    0.125f,     0.1875f,   0.25f,
    0.3125f,   0.375f,     0.4375f,    0.5f,      0.5625f,
    0.625f,    0.6875f,    0.75f,      0.8125f,   0.875f,
    0.9375f,
    0.f,       0.0416667f, 0.0833333f, 0.125f,    0.166667f,
    0.208333f, 0.25f,      0.291667f,  0.333333f, 0.375f,
    0.416667f, 0.458333f,  0.5f,       0.541667f, 0.583333f,
    0.625f,    0.666667f,  0.708333f,  0.75f,     0.791667f,
    0.833333f, 0.875f,     0.916667f,  0.958333f,
    0.f,       0.0416667f, 0.0833333f, 0.125f,    0.166667f,
    0.208333f, 0.25f,      0.291667f,  0.333333f, 0.375f,
    0.416667f, 0.458333f,  0.5f,       0.541667f, 0.583333f,
    0.625f,    0.666667f,  0.708333f,  0.75f,     0.791667f,
    0.833333f, 0.875f,     0.916667f,  0.958333f,
    0.f,       0.03125f,   0.0625f,    0.09375f,  0.125f,
    0.15625f,  0.1875f,    0.21875f,   0.25f,     0.28125f,
    0.3125f,   0.34375f,   0.375f,     0.40625f,  0.4375f,
    0.46875f,  0.5f,       0.53125f,   0.5625f,   0.59375f,
    0.625f,    0.65625f,   0.6875f,    0.71875f,  0.75f,
    0.78125f,  0.8125f,    0.84375f,   0.875f,    0.90625f,
    0.9375f,   0.96875f,
    0.f,       0.0208333f, 0.0416667f, 0.0625f,   0.0833333f,
    0.104167f, 0.125f,     0.145833f,  0.166667f, 0.1875f,
    0.208333f, 0.229167f,  0.25f,      0.270833f, 0.291667f,
    0.3125f,   0.333333f,  0.354167f,  0.375f,    0.395833f,
    0.416667f, 0.4375f,    0.458333f,  0.479167f, 0.5f,
    0.520833f, 0.541667f,  0.5625f,    0.583333f, 0.604167f,
    0.625f,    0.645833f,  0.666667f,  0.6875f,   0.708333f,
    0.729167f, 0.75f,      0.770833f,  0.791667f, 0.8125f,
    0.833333f, 0.854167f,  0.875f,     0.895833f, 0.916667f,
    0.9375f,   0.958333f,  0.979167f,
}};

struct WebRtcPffftSetupTag {
    std::size_t fftSize;
    std::int32_t fftType;
};

class ErrnoGuard {
public:
    ErrnoGuard() : savedErrno(errno) {}
    ~ErrnoGuard() { errno = savedErrno; }

private:
    int savedErrno;
};

class GuestAllocTracker {
public:
    GuestAllocTracker() = default;
    ~GuestAllocTracker() {
        while (count > 0)
            _ZdlPv_nid_postfix(ptrs[--count]);
    }

    void* Allocate(std::size_t bytes) {
        if (count >= ptrs.size())
            throw std::length_error("GuestAllocTracker capacity exceeded");
        void* ptr = _Znwm_nid_postfix(bytes);
        if (!ptr)
            throw std::bad_alloc();
        ptrs[count++] = ptr;
        return ptr;
    }

    void Release() {
        count = 0;
    }

private:
    std::array<void*, 32> ptrs{};
    std::size_t count = 0;
};

float* AllocateZeroedFloats(GuestAllocTracker& tracker, std::size_t count) {
    auto* ptr = static_cast<float*>(tracker.Allocate(count * sizeof(float)));
    std::array<float, WebRtcRnnVadPitchBufSize24kHz> zeros{};
    if (!GuestPtr<float>(ptr).CopyOut(zeros.data(), count))
        throw std::invalid_argument("FeaturesExtractor constructor could not write guest float buffer");
    return ptr;
}

WebRtcRnnVadFloatVector AllocateFloatVector(GuestAllocTracker& tracker, std::size_t count, const float* initial = nullptr) {
    float* ptr = initial ? static_cast<float*>(tracker.Allocate(count * sizeof(float))) : AllocateZeroedFloats(tracker, count);
    if (initial && !GuestPtr<float>(ptr).CopyOut(initial, count))
        throw std::invalid_argument("FeaturesExtractor constructor could not write guest vector data");
    return {ptr, ptr + count, ptr + count};
}

WebRtcPffftFloatBuffer* AllocateFloatBuffer(GuestAllocTracker& tracker, std::size_t fftSize) {
    float* data = AllocateZeroedFloats(tracker, fftSize);
    auto* buffer = static_cast<WebRtcPffftFloatBuffer*>(tracker.Allocate(sizeof(WebRtcPffftFloatBuffer)));
    const WebRtcPffftFloatBuffer value{fftSize, data};
    if (!GuestPtr<WebRtcPffftFloatBuffer>(buffer).CopyOut(value))
        throw std::invalid_argument("FeaturesExtractor constructor could not write FloatBuffer");
    return buffer;
}

WebRtcPffft AllocatePffft(GuestAllocTracker& tracker, std::size_t fftSize) {
    auto* setup = static_cast<WebRtcPffftSetupTag*>(tracker.Allocate(sizeof(WebRtcPffftSetupTag)));
    const WebRtcPffftSetupTag tag{fftSize, 0};
    if (!GuestPtr<WebRtcPffftSetupTag>(setup).CopyOut(tag))
        throw std::invalid_argument("FeaturesExtractor constructor could not write Pffft setup");
    float* scratch = AllocateZeroedFloats(tracker, fftSize);
    return {fftSize, 0, setup, scratch};
}

void ComputeScaledHalfVorbisWindow(float* halfWindow, float scaling) {
    constexpr std::size_t halfSize = WebRtcRnnVadFrameSize20ms24kHz / 2;
    for (std::size_t i = 0; i < halfSize; ++i) {
        const double inner = std::sin(0.5 * Pi * (static_cast<double>(i) + 0.5) / static_cast<double>(halfSize));
        halfWindow[i] = static_cast<float>(static_cast<double>(scaling) * std::sin(0.5 * Pi * inner * inner));
    }
}

void ComputeDctTable(float* dctTable) {
    const double scale0 = std::sqrt(0.5);
    for (std::size_t i = 0; i < WebRtcRnnVadNumBands; ++i) {
        for (std::size_t j = 0; j < WebRtcRnnVadNumBands; ++j)
            dctTable[i * WebRtcRnnVadNumBands + j] = static_cast<float>(std::cos((static_cast<double>(i) + 0.5) * static_cast<double>(j) * Pi / static_cast<double>(WebRtcRnnVadNumBands)));
        dctTable[i * WebRtcRnnVadNumBands] = static_cast<float>(static_cast<double>(dctTable[i * WebRtcRnnVadNumBands]) * scale0);
    }
}

bool ValidFloatVector(const WebRtcRnnVadFloatVector& vec, std::size_t expectedSize) {
    return vec.begin != nullptr && vec.end == vec.begin + expectedSize && vec.capacity == vec.end;
}

void ValidateFloatBuffer(const WebRtcPffftFloatBuffer* bufferPtr, std::size_t expectedSize, WebRtcPffftFloatBuffer& outBuffer) {
    if (!GuestPtr<const WebRtcPffftFloatBuffer>(bufferPtr).CopyIn(outBuffer))
        throw std::invalid_argument("FeaturesExtractor destructor requires valid FloatBuffer pointers");
    if (outBuffer.size != expectedSize || !outBuffer.data)
        throw std::invalid_argument("FeaturesExtractor destructor encountered invalid FloatBuffer");
}

void ValidatePffft(const WebRtcPffft& fft, std::size_t expectedSize) {
    if (fft.fftSize != expectedSize || fft.fftType != 0 || !fft.pffftStatus || !fft.scratchBuffer)
        throw std::invalid_argument("FeaturesExtractor destructor encountered invalid Pffft state");
    WebRtcPffftSetupTag tag{};
    if (!GuestPtr<const WebRtcPffftSetupTag>(fft.pffftStatus).CopyIn(tag))
        throw std::invalid_argument("FeaturesExtractor destructor requires a readable Pffft setup");
    if (tag.fftSize != expectedSize || tag.fftType != 0)
        throw std::invalid_argument("FeaturesExtractor destructor encountered mismatched Pffft setup");
}

void ReleaseFloatBuffer(WebRtcPffftFloatBuffer* bufferPtr, const WebRtcPffftFloatBuffer& buffer) {
    _ZdlPv_nid_postfix(bufferPtr);
    _ZdlPv_nid_postfix(buffer.data);
}

void ReleasePffft(const WebRtcPffft& fft) {
    _ZdlPv_nid_postfix(fft.scratchBuffer);
    _ZdlPv_nid_postfix(fft.pffftStatus);
}

}

extern "C" {

void APS5_VABI _ZN6webrtc7rnn_vad17FeaturesExtractorC1ERKNS_20AvailableCpuFeaturesE_nid_postfix(WebRtcRnnVadFeaturesExtractor* extractor, const WebRtcAvailableCpuFeatures* cpuFeatures) {
    ErrnoGuard errnoGuard;
    WebRtcAvailableCpuFeatures features{};
    if (!GuestPtr<const WebRtcAvailableCpuFeatures>(cpuFeatures).CopyIn(features))
        throw std::invalid_argument("FeaturesExtractor constructor requires CPU features");
    if (!extractor || !GuestWritable_nid_no_patch(extractor, sizeof(*extractor)))
        throw std::invalid_argument("FeaturesExtractor constructor requires a writable object");

    GuestAllocTracker tracker;
    WebRtcRnnVadFeaturesExtractor object{};
    object.useHighPassFilter = false;
    object.hpf = HpfConfig24k;

    object.pitchBuf24kHz = AllocateFloatVector(tracker, WebRtcRnnVadPitchBufSize24kHz);
    object.pitchBuf24kHzView = object.pitchBuf24kHz.begin;
    object.lpResidual = AllocateFloatVector(tracker, WebRtcRnnVadPitchBufSize24kHz);
    object.lpResidualView = object.lpResidual.begin;

    object.pitchEstimator.cpuFeatures = features;
    object.pitchEstimator.lastPitch48kHz = {0, 0.f};
    object.pitchEstimator.autoCorrCalculator.fft = AllocatePffft(tracker, WebRtcRnnVadAutoCorrFftSize);
    object.pitchEstimator.autoCorrCalculator.tmp = AllocateFloatBuffer(tracker, WebRtcRnnVadAutoCorrFftSize);
    object.pitchEstimator.autoCorrCalculator.x = AllocateFloatBuffer(tracker, WebRtcRnnVadAutoCorrFftSize);
    object.pitchEstimator.autoCorrCalculator.h = AllocateFloatBuffer(tracker, WebRtcRnnVadAutoCorrFftSize);
    object.pitchEstimator.yEnergy24kHz = AllocateFloatVector(tracker, WebRtcRnnVadRefineNumLags24kHz);
    object.pitchEstimator.pitchBuffer12kHz = AllocateFloatVector(tracker, WebRtcRnnVadPitchBufSize12kHz);
    object.pitchEstimator.autoCorrelation12kHz = AllocateFloatVector(tracker, WebRtcRnnVadNumLags12kHz);

    object.referenceFrameView = object.pitchBuf24kHz.begin + (WebRtcRnnVadPitchBufSize24kHz - WebRtcRnnVadFrameSize20ms24kHz);

    ComputeScaledHalfVorbisWindow(object.spectralFeaturesExtractor.halfWindow, 1.f / static_cast<float>(WebRtcRnnVadFrameSize20ms24kHz));
    object.spectralFeaturesExtractor.fft = AllocatePffft(tracker, WebRtcRnnVadFrameSize20ms24kHz);
    object.spectralFeaturesExtractor.fftBuffer = AllocateFloatBuffer(tracker, WebRtcRnnVadFrameSize20ms24kHz);
    object.spectralFeaturesExtractor.referenceFrameFft = AllocateFloatBuffer(tracker, WebRtcRnnVadFrameSize20ms24kHz);
    object.spectralFeaturesExtractor.laggedFrameFft = AllocateFloatBuffer(tracker, WebRtcRnnVadFrameSize20ms24kHz);
    object.spectralFeaturesExtractor.weights = AllocateFloatVector(tracker, WebRtcRnnVadFrameSize20ms24kHz / 2, OpusBandWeights24kHz20ms.data());
    ComputeDctTable(object.spectralFeaturesExtractor.dctTable);

    if (!GuestPtr<WebRtcRnnVadFeaturesExtractor>(extractor).CopyOut(object))
        throw std::invalid_argument("FeaturesExtractor constructor requires a writable object");
    tracker.Release();
}

void APS5_VABI _ZN6webrtc7rnn_vad17FeaturesExtractorD1Ev_nid_postfix(WebRtcRnnVadFeaturesExtractor* extractor) {
    ErrnoGuard errnoGuard;
    WebRtcRnnVadFeaturesExtractor object{};
    if (!GuestPtr<const WebRtcRnnVadFeaturesExtractor>(extractor).CopyIn(object))
        throw std::invalid_argument("FeaturesExtractor destructor requires a constructed object");
    if (!ValidFloatVector(object.pitchBuf24kHz, WebRtcRnnVadPitchBufSize24kHz) ||
        object.pitchBuf24kHzView != object.pitchBuf24kHz.begin ||
        !ValidFloatVector(object.lpResidual, WebRtcRnnVadPitchBufSize24kHz) ||
        object.lpResidualView != object.lpResidual.begin ||
        object.referenceFrameView != object.pitchBuf24kHz.begin + (WebRtcRnnVadPitchBufSize24kHz - WebRtcRnnVadFrameSize20ms24kHz) ||
        !ValidFloatVector(object.pitchEstimator.yEnergy24kHz, WebRtcRnnVadRefineNumLags24kHz) ||
        !ValidFloatVector(object.pitchEstimator.pitchBuffer12kHz, WebRtcRnnVadPitchBufSize12kHz) ||
        !ValidFloatVector(object.pitchEstimator.autoCorrelation12kHz, WebRtcRnnVadNumLags12kHz) ||
        !ValidFloatVector(object.spectralFeaturesExtractor.weights, WebRtcRnnVadFrameSize20ms24kHz / 2))
        throw std::invalid_argument("FeaturesExtractor destructor requires a constructed object");

    ValidatePffft(object.pitchEstimator.autoCorrCalculator.fft, WebRtcRnnVadAutoCorrFftSize);
    WebRtcPffftFloatBuffer autoCorrTmp{}, autoCorrX{}, autoCorrH{};
    ValidateFloatBuffer(object.pitchEstimator.autoCorrCalculator.tmp, WebRtcRnnVadAutoCorrFftSize, autoCorrTmp);
    ValidateFloatBuffer(object.pitchEstimator.autoCorrCalculator.x, WebRtcRnnVadAutoCorrFftSize, autoCorrX);
    ValidateFloatBuffer(object.pitchEstimator.autoCorrCalculator.h, WebRtcRnnVadAutoCorrFftSize, autoCorrH);

    ValidatePffft(object.spectralFeaturesExtractor.fft, WebRtcRnnVadFrameSize20ms24kHz);
    WebRtcPffftFloatBuffer fftBuf{}, refFft{}, lagFft{};
    ValidateFloatBuffer(object.spectralFeaturesExtractor.fftBuffer, WebRtcRnnVadFrameSize20ms24kHz, fftBuf);
    ValidateFloatBuffer(object.spectralFeaturesExtractor.referenceFrameFft, WebRtcRnnVadFrameSize20ms24kHz, refFft);
    ValidateFloatBuffer(object.spectralFeaturesExtractor.laggedFrameFft, WebRtcRnnVadFrameSize20ms24kHz, lagFft);

    _ZdlPv_nid_postfix(object.spectralFeaturesExtractor.weights.begin);
    ReleaseFloatBuffer(object.spectralFeaturesExtractor.laggedFrameFft, lagFft);
    ReleaseFloatBuffer(object.spectralFeaturesExtractor.referenceFrameFft, refFft);
    ReleaseFloatBuffer(object.spectralFeaturesExtractor.fftBuffer, fftBuf);
    ReleasePffft(object.spectralFeaturesExtractor.fft);

    _ZdlPv_nid_postfix(object.pitchEstimator.autoCorrelation12kHz.begin);
    _ZdlPv_nid_postfix(object.pitchEstimator.pitchBuffer12kHz.begin);
    _ZdlPv_nid_postfix(object.pitchEstimator.yEnergy24kHz.begin);
    ReleaseFloatBuffer(object.pitchEstimator.autoCorrCalculator.h, autoCorrH);
    ReleaseFloatBuffer(object.pitchEstimator.autoCorrCalculator.x, autoCorrX);
    ReleaseFloatBuffer(object.pitchEstimator.autoCorrCalculator.tmp, autoCorrTmp);
    ReleasePffft(object.pitchEstimator.autoCorrCalculator.fft);

    _ZdlPv_nid_postfix(object.lpResidual.begin);
    _ZdlPv_nid_postfix(object.pitchBuf24kHz.begin);
}

}
