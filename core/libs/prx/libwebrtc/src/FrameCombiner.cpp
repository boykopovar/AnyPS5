/* Derived from modules/audio_mixer/frame_combiner.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_mixer/audio_frame_manipulator.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/agc2/limiter.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/agc2/interpolated_gain_curve.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/agc2/interpolated_gain_curve.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/agc2/fixed_digital_level_estimator.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from audio/utility/audio_frame_operations.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from common_audio/include/audio_util.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::size_t MaxDataSamples = WebRtcAudioFrameMaxDataSizeSamples;
constexpr std::size_t MaxChannels = WebRtcFrameCombinerMaxChannels;
constexpr std::size_t MaxChannelSize = WebRtcFrameCombinerMaxChannelSize;
constexpr int SubFrames = static_cast<int>(WebRtcLimiterSubFrames);
constexpr std::size_t BuffersPerSecond = 100;
constexpr float MinFloatS16Value = -32768.0f;
constexpr float MaxFloatS16Value = 32767.0f;
constexpr float InitialFilterStateLevel = 0.0f;
constexpr float AttackFilterConstant = 0.0f;
constexpr float DecayFilterConstant = 0.9971259f;
constexpr float AttackFirstSubframeInterpolationPower = 8.0f;
constexpr float MaxInputLevelLinear = static_cast<float>(36766.300710566735);
constexpr int KneePoints = 22;
constexpr std::int32_t RegionIdentity = 0;
constexpr std::int32_t RegionKnee = 1;
constexpr std::int32_t RegionLimiter = 2;
constexpr std::int32_t RegionSaturation = 3;

constexpr std::array<float, 32> ApproximationX = {
    30057.296875,    30148.986328125, 30240.67578125,  30424.052734375,
    30607.4296875,   30790.806640625, 30974.18359375,  31157.560546875,
    31340.939453125, 31524.31640625,  31707.693359375, 31891.0703125,
    32074.447265625, 32257.82421875,  32441.201171875, 32624.580078125,
    32807.95703125,  32991.33203125,  33174.7109375,   33358.08984375,
    33541.46484375,  33724.84375,     33819.53515625,  34009.5390625,
    34200.05859375,  34389.81640625,  34674.48828125,  35054.375,
    35434.86328125,  35814.81640625,  36195.16796875,  36575.03125};

constexpr std::array<float, 32> ApproximationM = {
    -3.515235675877192989e-07, -1.050251626111275982e-06,
    -2.085213736791047268e-06, -3.443004743530764244e-06,
    -4.773849468620028347e-06, -6.077375928725814447e-06,
    -7.353257842623861507e-06, -8.601219633419532329e-06,
    -9.821013009059242904e-06, -1.101243378798244521e-05,
    -1.217532644659513608e-05, -1.330956911260727793e-05,
    -1.441507538402220234e-05, -1.549179251014720649e-05,
    -1.653970684856176376e-05, -1.755882840370759368e-05,
    -1.854918446042574942e-05, -1.951086778717581183e-05,
    -2.044398024736437947e-05, -2.1348627342376858e-05,
    -2.222496914328075945e-05, -2.265374678245279938e-05,
    -2.242570917587727308e-05, -2.220122041762806475e-05,
    -2.19802095671184361e-05,  -2.176260204578284174e-05,
    -2.133731686626560986e-05, -2.092481918225530535e-05,
    -2.052459603874012828e-05, -2.013615448959171772e-05,
    -1.975903069251216948e-05, -1.939277899509761482e-05};

constexpr std::array<float, 32> ApproximationQ = {
    1.010565876960754395, 1.031631827354431152, 1.062929749488830566,
    1.104239225387573242, 1.144973039627075195, 1.185109615325927734,
    1.224629044532775879, 1.263512492179870605, 1.301741957664489746,
    1.339300632476806641, 1.376173257827758789, 1.412345528602600098,
    1.447803974151611328, 1.482536554336547852, 1.516532182693481445,
    1.549780607223510742, 1.582272171974182129, 1.613999366760253906,
    1.644955039024353027, 1.675132393836975098, 1.704526185989379883,
    1.718986630439758301, 1.711274504661560059, 1.703639745712280273,
    1.696081161499023438, 1.688597679138183594, 1.673851132392883301,
    1.659391283988952637, 1.645209431648254395, 1.631297469139099121,
    1.617647409439086914, 1.604251742362976074};

std::size_t RemixedChannels(const WebRtcAudioFrame* frame, std::size_t target) {
    const std::size_t channels = frame->numChannels;
    if (channels == target)
        return channels;
    if (target >= 3 || channels >= 3)
        throw std::invalid_argument("FrameCombiner::Combine: remixing to or from more than two channels needs ChannelMixer, which is not implemented");
    if (channels > target)
        return target == 1 ? 1 : channels;
    if (channels != 1 || frame->samplesPerChannel > MaxDataSamples / target)
        return channels;
    return target;
}

WebRtcMutableInterleavedView16 MutableData(WebRtcAudioFrame* frame, std::size_t samplesPerChannel, std::size_t channels) {
    const std::size_t total = samplesPerChannel * channels;
    if (frame->muted) {
        std::memset(frame->data, 0, total * sizeof(std::int16_t));
        frame->muted = false;
    }
    frame->samplesPerChannel = samplesPerChannel;
    frame->numChannels = channels;
    return {channels, samplesPerChannel, frame->data, total};
}

void RemixFrame(std::size_t target, WebRtcAudioFrame* frame) {
    const std::size_t channels = frame->numChannels;
    if (channels == target)
        return;
    if (channels > target) {
        if (channels > 1 && target == 1) {
            if (!frame->muted) {
                const std::size_t frames = frame->samplesPerChannel;
                for (std::size_t i = 0; i < frames; ++i) {
                    std::int32_t value = frame->data[i * channels];
                    for (std::size_t channel = 1; channel < channels; ++channel)
                        value += frame->data[i * channels + channel];
                    frame->data[i] = static_cast<std::int16_t>(value / static_cast<int>(channels));
                }
            }
            frame->numChannels = 1;
        }
        return;
    }
    if (channels != 1 || frame->samplesPerChannel > MaxDataSamples / target)
        return;
    if (frame->muted) {
        frame->numChannels = target;
        return;
    }
    const WebRtcMutableInterleavedView16 data = MutableData(frame, frame->samplesPerChannel, target);
    for (int i = static_cast<int>(frame->samplesPerChannel) - 1; i >= 0; --i) {
        for (std::size_t j = 0; j < target; ++j)
            data.data[target * i + j] = data.data[i];
    }
}

void UpdateStats(WebRtcGainCurveStats& stats, float inputLevel) {
    stats.available = true;
    std::int32_t region;
    if (inputLevel < ApproximationX[0]) {
        stats.lookUpsIdentityRegion++;
        region = RegionIdentity;
    } else if (inputLevel < ApproximationX[KneePoints - 1]) {
        stats.lookUpsKneeRegion++;
        region = RegionKnee;
    } else if (inputLevel < MaxInputLevelLinear) {
        stats.lookUpsLimiterRegion++;
        region = RegionLimiter;
    } else {
        stats.lookUpsSaturationRegion++;
        region = RegionSaturation;
    }
    if (region == stats.region) {
        ++stats.regionDurationFrames;
    } else {
        stats.regionDurationFrames = 0;
        stats.region = region;
    }
}

float LookUpGainToApply(WebRtcGainCurveStats& stats, float inputLevel) {
    UpdateStats(stats, inputLevel);
    if (inputLevel <= ApproximationX[0])
        return 1.0f;
    if (inputLevel >= MaxInputLevelLinear)
        return 32768.f / inputLevel;
    const auto it = std::lower_bound(ApproximationX.begin(), ApproximationX.end(), inputLevel);
    const std::size_t index = static_cast<std::size_t>(it - ApproximationX.begin()) - 1;
    return ApproximationM[index] * inputLevel + ApproximationQ[index];
}

std::array<float, WebRtcLimiterSubFrames> ComputeLevel(WebRtcFixedDigitalLevelEstimator& estimator, const float* signal, std::size_t samplesPerChannel, std::size_t channels) {
    const int subFrameSize = estimator.samplesInSubFrame;
    std::array<float, WebRtcLimiterSubFrames> envelope{};
    for (std::size_t channelIndex = 0; channelIndex < channels; ++channelIndex) {
        const float* channel = signal + channelIndex * samplesPerChannel;
        for (int subFrame = 0; subFrame < SubFrames; ++subFrame) {
            for (int sample = 0; sample < subFrameSize; ++sample)
                envelope[subFrame] = std::max(envelope[subFrame], std::abs(channel[subFrame * subFrameSize + sample]));
        }
    }
    for (int subFrame = 0; subFrame < SubFrames - 1; ++subFrame) {
        if (envelope[subFrame] < envelope[subFrame + 1])
            envelope[subFrame] = envelope[subFrame + 1];
    }
    for (int subFrame = 0; subFrame < SubFrames; ++subFrame) {
        const float value = envelope[subFrame];
        if (value > estimator.filterStateLevel)
            envelope[subFrame] = value * (1 - AttackFilterConstant) + estimator.filterStateLevel * AttackFilterConstant;
        else
            envelope[subFrame] = value * (1 - DecayFilterConstant) + estimator.filterStateLevel * DecayFilterConstant;
        estimator.filterStateLevel = envelope[subFrame];
    }
    return envelope;
}

void ComputePerSampleSubframeFactors(const float* scalingFactors, float* perSample, std::size_t samplesPerChannel) {
    const std::size_t subFrames = WebRtcLimiterSubFrames;
    const int subFrameSize = static_cast<int>(samplesPerChannel / subFrames);
    const bool isAttack = scalingFactors[0] > scalingFactors[1];
    if (isAttack) {
        const int n = subFrameSize;
        constexpr float p = AttackFirstSubframeInterpolationPower;
        for (int i = 0; i < n; ++i)
            perSample[i] = std::pow(1.f - i / n, p) * (scalingFactors[0] - scalingFactors[1]) + scalingFactors[1];
    }
    for (std::size_t i = isAttack ? 1 : 0; i < subFrames; ++i) {
        const int start = static_cast<int>(i) * subFrameSize;
        const float scalingStart = scalingFactors[i];
        const float scalingEnd = scalingFactors[i + 1];
        const float scalingDiff = (scalingEnd - scalingStart) / subFrameSize;
        for (int j = 0; j < subFrameSize; ++j)
            perSample[start + j] = scalingStart + scalingDiff * j;
    }
}

void ProcessLimiter(WebRtcLimiter& limiter, float* signal, std::size_t samplesPerChannel, std::size_t channels) {
    limiter.levelEstimator.samplesInFrame = static_cast<std::int32_t>(samplesPerChannel);
    limiter.levelEstimator.samplesInSubFrame = static_cast<std::int32_t>(samplesPerChannel / WebRtcLimiterSubFrames);
    const std::array<float, WebRtcLimiterSubFrames> level = ComputeLevel(limiter.levelEstimator, signal, samplesPerChannel, channels);
    limiter.scalingFactors[0] = limiter.lastScalingFactor;
    for (std::size_t i = 0; i < level.size(); ++i)
        limiter.scalingFactors[i + 1] = LookUpGainToApply(limiter.interpGainCurve.stats, level[i]);
    ComputePerSampleSubframeFactors(limiter.scalingFactors, limiter.perSampleScalingFactors, samplesPerChannel);
    for (std::size_t i = 0; i < channels; ++i) {
        float* channel = signal + i * samplesPerChannel;
        for (std::size_t j = 0; j < samplesPerChannel; ++j) {
            const float value = channel[j] * limiter.perSampleScalingFactors[j];
            channel[j] = value <= MinFloatS16Value ? MinFloatS16Value : value >= MaxFloatS16Value ? MaxFloatS16Value : value;
        }
    }
    limiter.lastScalingFactor = limiter.scalingFactors[WebRtcLimiterSubFrames];
}

std::int16_t FloatS16ToS16(float v) {
    v = std::min(v, 32767.f);
    v = std::max(v, -32768.f);
    return static_cast<std::int16_t>(v + std::copysign(0.5f, v));
}

const std::int16_t ZeroedData[MaxDataSamples] = {};

const std::int16_t* FrameData(const WebRtcAudioFrame* frame) {
    return frame->muted ? ZeroedData : frame->data;
}

void Validate(const WebRtcFrameCombiner* combiner, WebRtcAudioFrameList mixList, std::size_t channels, int sampleRate, std::size_t numberOfStreams,
              const WebRtcAudioFrame* output) {
    if (!combiner || !combiner->dataDumper || !output)
        throw std::invalid_argument("FrameCombiner::Combine requires a constructed FrameCombiner and an output frame");
    if (mixList.size != 0 && !mixList.data)
        throw std::invalid_argument("FrameCombiner::Combine requires the mix list data");
    if (sampleRate < 0)
        throw std::invalid_argument("FrameCombiner::Combine needs a non-negative sample rate");
    const std::size_t samplesPerChannel = static_cast<std::size_t>(sampleRate) / BuffersPerSecond;
    if (channels != 0 && samplesPerChannel > MaxDataSamples / channels)
        throw std::invalid_argument("FrameCombiner::Combine: the output frame cannot hold 10 ms at this rate and channel count");
    const std::size_t mixSamples = std::min(samplesPerChannel, MaxChannelSize);
    for (std::size_t i = 0; i < mixList.size; ++i) {
        const WebRtcAudioFrame* frame = mixList.data[i];
        if (!frame)
            throw std::invalid_argument("FrameCombiner::Combine requires every mixed frame");
        if (frame == output)
            throw std::invalid_argument("FrameCombiner::Combine: the output frame cannot also be mixed");
        if (frame->packetInfos || output->packetInfos)
            throw std::invalid_argument("FrameCombiner::Combine: merging RtpPacketInfos is not implemented");
        const std::size_t remixed = RemixedChannels(frame, channels);
        if (remixed < frame->numChannels && !frame->muted && frame->samplesPerChannel > MaxDataSamples / frame->numChannels)
            throw std::invalid_argument("FrameCombiner::Combine: a frame to downmix holds more samples than an AudioFrame");
        if (numberOfStreams > 1 && (frame->samplesPerChannel == 0 || remixed == 0))
            throw std::invalid_argument("FrameCombiner::Combine: a mixed frame has no samples");
        if (numberOfStreams <= 1 && i == 0 && remixed != 0 && frame->samplesPerChannel > MaxDataSamples / remixed)
            throw std::invalid_argument("FrameCombiner::Combine: the first frame holds more samples than an AudioFrame");
    }
    if (numberOfStreams > 1 && combiner->useLimiter && mixSamples % WebRtcLimiterSubFrames != 0)
        throw std::invalid_argument("FrameCombiner::Combine: the limiter needs a multiple of 20 samples per channel");
}

}

extern "C" {

void APS5_VABI _ZN6webrtc13FrameCombinerC1Eb_nid_postfix(WebRtcFrameCombiner* combiner, bool useLimiter) {
    if (!combiner)
        throw std::invalid_argument("FrameCombiner constructor requires an object");
    void* dataDumper = _Znwm_nid_postfix(1);
    std::memset(static_cast<void*>(combiner), 0, sizeof(*combiner));
    combiner->dataDumper = dataDumper;
    combiner->limiter.interpGainCurve.apmDataDumper = dataDumper;
    combiner->limiter.interpGainCurve.stats.region = RegionIdentity;
    combiner->limiter.levelEstimator.apmDataDumper = dataDumper;
    combiner->limiter.levelEstimator.filterStateLevel = InitialFilterStateLevel;
    combiner->limiter.levelEstimator.samplesInFrame = static_cast<std::int32_t>(MaxChannelSize);
    combiner->limiter.levelEstimator.samplesInSubFrame = static_cast<std::int32_t>(MaxChannelSize / WebRtcLimiterSubFrames);
    combiner->limiter.apmDataDumper = dataDumper;
    combiner->limiter.lastScalingFactor = 1.f;
    combiner->useLimiter = useLimiter;
}

void APS5_VABI _ZN6webrtc13FrameCombinerD1Ev_nid_postfix(WebRtcFrameCombiner* combiner) {
    if (!combiner)
        throw std::invalid_argument("FrameCombiner destructor requires an object");
    if (combiner->dataDumper)
        _ZdlPv_nid_postfix(combiner->dataDumper);
    combiner->dataDumper = nullptr;
}

void APS5_VABI _ZN6webrtc13FrameCombiner7CombineEN3rtc9ArrayViewIKPNS_10AudioFrameELln4711EEEmimS4__nid_postfix(
    WebRtcFrameCombiner* combiner, WebRtcAudioFrameList mixList, std::size_t numberOfChannels, int sampleRate, std::size_t numberOfStreams,
    WebRtcAudioFrame* output) {
    numberOfChannels = std::min(numberOfChannels, MaxChannels);
    Validate(combiner, mixList, numberOfChannels, sampleRate, numberOfStreams, output);
    const std::size_t samplesPerChannel = static_cast<std::size_t>(sampleRate) / BuffersPerSecond;
    _ZN6webrtc10AudioFrame11UpdateFrameEjPKsmiNS0_10SpeechTypeENS0_11VADActivityEm_nid_postfix(
        output, 0, nullptr, samplesPerChannel, sampleRate, WebRtcAudioFrameSpeechUndefined, WebRtcAudioFrameVadUnknown, numberOfChannels);
    if (mixList.size == 0) {
        output->elapsedTimeMs = -1;
    } else {
        output->timestamp = mixList.data[0]->timestamp;
        output->elapsedTimeMs = mixList.data[0]->elapsedTimeMs;
        output->ntpTimeMs = mixList.data[0]->ntpTimeMs;
        for (std::size_t i = 0; i < mixList.size; ++i) {
            const WebRtcAudioFrame* frame = mixList.data[i];
            output->timestamp = std::min(output->timestamp, frame->timestamp);
            output->ntpTimeMs = std::min(output->ntpTimeMs, frame->ntpTimeMs);
            output->elapsedTimeMs = std::max(output->elapsedTimeMs, frame->elapsedTimeMs);
        }
    }
    for (std::size_t i = 0; i < mixList.size; ++i)
        RemixFrame(numberOfChannels, mixList.data[i]);
    if (numberOfStreams <= 1) {
        if (mixList.size == 0) {
            output->muted = true;
            return;
        }
        const WebRtcAudioFrame* first = mixList.data[0];
        const std::size_t total = first->samplesPerChannel * first->numChannels;
        const std::int16_t* source = FrameData(first);
        const WebRtcMutableInterleavedView16 destination = MutableData(output, first->samplesPerChannel, first->numChannels);
        if (total != 0)
            std::memcpy(destination.data, source, total * sizeof(std::int16_t));
        return;
    }
    const std::size_t mixSamples = std::min(samplesPerChannel, MaxChannelSize);
    float* mixing = combiner->mixingBuffer;
    std::fill(mixing, mixing + mixSamples * numberOfChannels, 0.0f);
    for (std::size_t i = 0; i < mixList.size; ++i) {
        const std::int16_t* frameData = FrameData(mixList.data[i]);
        for (std::size_t j = 0; j < numberOfChannels; ++j) {
            float* channel = mixing + j * mixSamples;
            for (std::size_t k = 0; k < mixSamples; ++k)
                channel[k] += frameData[numberOfChannels * k + j];
        }
    }
    if (combiner->useLimiter)
        ProcessLimiter(combiner->limiter, mixing, mixSamples, numberOfChannels);
    const WebRtcMutableInterleavedView16 destination = MutableData(output, mixSamples, numberOfChannels);
    for (std::size_t i = 0; i < numberOfChannels; ++i) {
        const float* channel = mixing + i * mixSamples;
        for (std::size_t j = 0; j < mixSamples; ++j)
            destination.data[numberOfChannels * j + i] = FloatS16ToS16(channel[j]);
    }
}

}
