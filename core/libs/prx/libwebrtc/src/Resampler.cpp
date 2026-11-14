/* Derived from common_audio/resampler/sinc_resampler.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from common_audio/resampler/push_sinc_resampler.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from common_audio/resampler/push_resampler.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from audio/remix_resample.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t MaxChannels = 8;
constexpr std::size_t MaxSamplesPerChannel = 3840;

void RunCallback(WebRtcPushSincResampler* push, std::size_t frames, float* destination);

class SincResampler {
public:
    static constexpr std::size_t KernelSize = 32;
    static constexpr std::size_t KernelOffsetCount = 32;
    static constexpr std::size_t KernelStorageSize = KernelSize * (KernelOffsetCount + 1);

    SincResampler(double ioSampleRateRatio, std::size_t requestFrames, WebRtcPushSincResampler* callback)
        : ioSampleRateRatio(ioSampleRateRatio), callback(callback), requestFrames(requestFrames), inputBufferSize(requestFrames + KernelSize),
          kernel(KernelStorageSize), input(inputBufferSize), r1(input.data()), r2(input.data() + KernelSize / 2) {
        Flush();
        InitializeKernel();
    }

    void Resample(std::size_t frames, float* destination) {
        std::size_t remainingFrames = frames;
        if (!bufferPrimed && remainingFrames) {
            RunCallback(callback, requestFrames, r0);
            bufferPrimed = true;
        }
        const double currentIoRatio = ioSampleRateRatio;
        const float* const kernelPointer = kernel.data();
        while (remainingFrames) {
            for (int i = static_cast<int>(std::ceil((static_cast<double>(blockSize) - virtualSourceIndex) / currentIoRatio)); i > 0; --i) {
                const int sourceIndex = static_cast<int>(virtualSourceIndex);
                const double subsampleRemainder = virtualSourceIndex - sourceIndex;
                const double virtualOffsetIndex = subsampleRemainder * KernelOffsetCount;
                const int offsetIndex = static_cast<int>(virtualOffsetIndex);
                const float* const k1 = kernelPointer + offsetIndex * KernelSize;
                const float* const k2 = k1 + KernelSize;
                const float* const inputPointer = r1 + sourceIndex;
                const double kernelInterpolationFactor = virtualOffsetIndex - offsetIndex;
                *destination++ = Convolve(inputPointer, k1, k2, kernelInterpolationFactor);
                virtualSourceIndex += currentIoRatio;
                if (!--remainingFrames)
                    return;
            }
            virtualSourceIndex -= static_cast<double>(blockSize);
            std::memcpy(r1, r3, sizeof(float) * KernelSize);
            if (r0 == r2)
                UpdateRegions(true);
            RunCallback(callback, requestFrames, r0);
        }
    }

    std::size_t ChunkSize() const {
        return static_cast<std::size_t>(static_cast<double>(blockSize) / ioSampleRateRatio);
    }

    std::size_t RequestFrames() const {
        return requestFrames;
    }

private:
    static double SincScaleFactor(double ioRatio) {
        return (ioRatio > 1.0 ? 1.0 / ioRatio : 1.0) * 0.9;
    }

    static float Convolve(const float* inputPointer, const float* k1, const float* k2, double kernelInterpolationFactor) {
        float sum1 = 0;
        float sum2 = 0;
        std::size_t n = KernelSize;
        while (n--) {
            sum1 += *inputPointer * *k1++;
            sum2 += *inputPointer++ * *k2++;
        }
        return static_cast<float>((1.0 - kernelInterpolationFactor) * sum1 + kernelInterpolationFactor * sum2);
    }

    void Flush() {
        virtualSourceIndex = 0;
        bufferPrimed = false;
        std::fill(input.begin(), input.end(), 0.0f);
        UpdateRegions(false);
    }

    void UpdateRegions(bool secondLoad) {
        r0 = input.data() + (secondLoad ? KernelSize : KernelSize / 2);
        r3 = r0 + requestFrames - KernelSize;
        r4 = r0 + requestFrames - KernelSize / 2;
        blockSize = static_cast<std::size_t>(r4 - r2);
    }

    void InitializeKernel() {
        constexpr double Alpha = 0.16;
        constexpr double A0 = 0.5 * (1.0 - Alpha);
        constexpr double A1 = 0.5;
        constexpr double A2 = 0.5 * Alpha;
        const double sincScaleFactor = SincScaleFactor(ioSampleRateRatio);
        for (std::size_t offsetIndex = 0; offsetIndex <= KernelOffsetCount; ++offsetIndex) {
            const float subsampleOffset = static_cast<float>(offsetIndex) / KernelOffsetCount;
            for (std::size_t i = 0; i < KernelSize; ++i) {
                const std::size_t index = i + offsetIndex * KernelSize;
                const float preSinc = static_cast<float>(std::numbers::pi * (static_cast<int>(i) - static_cast<int>(KernelSize / 2) - subsampleOffset));
                const float x = (i - subsampleOffset) / KernelSize;
                const float window = static_cast<float>(A0 - A1 * std::cos(2.0 * std::numbers::pi * x) + A2 * std::cos(4.0 * std::numbers::pi * x));
                kernel[index] = static_cast<float>(window * ((preSinc == 0) ? sincScaleFactor : (std::sin(sincScaleFactor * preSinc) / preSinc)));
            }
        }
    }

    double ioSampleRateRatio;
    double virtualSourceIndex = 0;
    bool bufferPrimed = false;
    WebRtcPushSincResampler* callback;
    const std::size_t requestFrames;
    std::size_t blockSize = 0;
    const std::size_t inputBufferSize;
    std::vector<float> kernel;
    std::vector<float> input;
    float* r0 = nullptr;
    float* const r1;
    float* const r2;
    float* r3 = nullptr;
    float* r4 = nullptr;
};

SincResampler* Sinc(const WebRtcPushSincResampler* push) {
    return static_cast<SincResampler*>(push->resampler);
}

std::int16_t FloatS16ToS16(float value) {
    value = std::min(value, 32767.f);
    value = std::max(value, -32768.f);
    return static_cast<std::int16_t>(value + std::copysign(0.5f, value));
}

void RunCallback(WebRtcPushSincResampler* push, std::size_t frames, float* destination) {
    if (push->sourceAvailable != frames)
        throw std::logic_error("PushSincResampler::Run was asked for a different number of frames than it holds");
    if (push->firstPass) {
        std::memset(destination, 0, frames * sizeof(*destination));
        push->firstPass = false;
        return;
    }
    if (push->sourcePtr) {
        std::memcpy(destination, push->sourcePtr, frames * sizeof(*destination));
    } else {
        for (std::size_t i = 0; i < frames; ++i)
            destination[i] = static_cast<float>(push->sourcePtrInt[i]);
    }
    push->sourceAvailable -= frames;
}

void APS5_VABI RunPushSincResampler_nid_no_patch(WebRtcPushSincResampler* push, std::size_t frames, float* destination) {
    RunCallback(push, frames, destination);
}

void APS5_VABI DeletePushSincResampler_nid_no_patch(WebRtcPushSincResampler* push) {
    _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(push);
    _ZdlPv_nid_postfix(push);
}

void* const PushSincResamplerVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc17PushSincResamplerD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeletePushSincResampler_nid_no_patch),
    reinterpret_cast<void*>(&RunPushSincResampler_nid_no_patch),
};

std::size_t ResampleInt16(WebRtcPushSincResampler* push, const std::int16_t* source, std::size_t sourceLength, std::int16_t* destination) {
    if (!push->floatBuffer)
        push->floatBuffer = new float[push->destinationFrames];
    push->sourcePtrInt = source;
    _ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(push, nullptr, sourceLength, push->floatBuffer, push->destinationFrames);
    for (std::size_t i = 0; i < push->destinationFrames; ++i)
        destination[i] = FloatS16ToS16(push->floatBuffer[i]);
    push->sourcePtrInt = nullptr;
    return push->destinationFrames;
}

std::size_t ResamplerCount(const WebRtcPushResampler16* resampler) {
    return static_cast<std::size_t>(resampler->resamplersEnd - resampler->resamplersBegin);
}

void ReleaseResamplers(WebRtcPushResampler16* resampler) {
    for (WebRtcPushSincResampler** entry = resampler->resamplersBegin; entry != resampler->resamplersEnd; ++entry) {
        _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(*entry);
        delete *entry;
    }
    delete[] resampler->resamplersBegin;
    resampler->resamplersBegin = nullptr;
    resampler->resamplersEnd = nullptr;
    resampler->resamplersCapacity = nullptr;
}

struct PushSincResamplerDeleter {
    void operator()(WebRtcPushSincResampler* push) const {
        _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(push);
        delete push;
    }
};

void CheckShape(std::size_t sourceSamplesPerChannel, std::size_t destinationSamplesPerChannel, std::size_t channels) {
    if (channels == 0 || channels > MaxChannels || sourceSamplesPerChannel == 0 || sourceSamplesPerChannel > MaxSamplesPerChannel ||
        destinationSamplesPerChannel == 0 || destinationSamplesPerChannel > MaxSamplesPerChannel)
        throw std::invalid_argument("PushResampler needs 1 to 8 channels and 1 to 3840 samples per channel");
}

void Clear(WebRtcPushResampler16* resampler) {
    resampler->source = nullptr;
    resampler->destination = nullptr;
    resampler->sourceView = {};
    resampler->destinationView = {};
    resampler->resamplersBegin = nullptr;
    resampler->resamplersEnd = nullptr;
    resampler->resamplersCapacity = nullptr;
}

void EnsureInitialized(WebRtcPushResampler16* resampler, std::size_t sourceSamplesPerChannel, std::size_t destinationSamplesPerChannel, std::size_t channels) {
    CheckShape(sourceSamplesPerChannel, destinationSamplesPerChannel, channels);
    if (sourceSamplesPerChannel == resampler->sourceView.samplesPerChannel && destinationSamplesPerChannel == resampler->destinationView.samplesPerChannel &&
        channels == resampler->sourceView.numChannels)
        return;
    std::unique_ptr<std::int16_t[]> source(new std::int16_t[sourceSamplesPerChannel * channels]);
    std::unique_ptr<std::int16_t[]> destination(new std::int16_t[destinationSamplesPerChannel * channels]);
    std::vector<std::unique_ptr<WebRtcPushSincResampler, PushSincResamplerDeleter>> pushes;
    pushes.reserve(channels);
    for (std::size_t i = 0; i < channels; ++i) {
        auto push = std::make_unique<WebRtcPushSincResampler>();
        _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(push.get(), sourceSamplesPerChannel, destinationSamplesPerChannel);
        pushes.emplace_back(push.release());
    }
    std::unique_ptr<WebRtcPushSincResampler*[]> table(new WebRtcPushSincResampler*[channels]);
    ReleaseResamplers(resampler);
    delete[] resampler->source;
    delete[] resampler->destination;
    for (std::size_t i = 0; i < channels; ++i)
        table[i] = pushes[i].release();
    resampler->source = source.release();
    resampler->destination = destination.release();
    resampler->sourceView = {channels, sourceSamplesPerChannel, resampler->source, sourceSamplesPerChannel * channels};
    resampler->destinationView = {channels, destinationSamplesPerChannel, resampler->destination, destinationSamplesPerChannel * channels};
    resampler->resamplersBegin = table.release();
    resampler->resamplersEnd = resampler->resamplersBegin + channels;
    resampler->resamplersCapacity = resampler->resamplersEnd;
}

int ResampleInterleaved(WebRtcPushResampler16* resampler, const WebRtcInterleavedView& source, const WebRtcMutableInterleavedView16& destination) {
    if (destination.numChannels != source.numChannels)
        throw std::invalid_argument("PushResampler::Resample needs the same channel count on both sides");
    CheckShape(source.samplesPerChannel, destination.samplesPerChannel, source.numChannels);
    if (source.size != source.samplesPerChannel * source.numChannels || destination.size != destination.samplesPerChannel * destination.numChannels ||
        !source.data || !destination.data)
        throw std::invalid_argument("PushResampler::Resample views must hold samples per channel times channels samples");
    EnsureInitialized(resampler, source.samplesPerChannel, destination.samplesPerChannel, source.numChannels);
    if (source.samplesPerChannel == destination.samplesPerChannel) {
        if (source.size != 0)
            std::memcpy(destination.data, source.data, source.size * sizeof(std::int16_t));
        return static_cast<int>(source.size);
    }
    const std::size_t channels = source.numChannels;
    const std::size_t sourceFrames = source.samplesPerChannel;
    const std::size_t destinationFrames = destination.samplesPerChannel;
    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (std::size_t frame = 0; frame < sourceFrames; ++frame)
            resampler->source[channel * sourceFrames + frame] = source.data[frame * channels + channel];
    }
    for (std::size_t channel = 0; channel < channels; ++channel)
        ResampleInt16(resampler->resamplersBegin[channel], resampler->source + channel * sourceFrames, sourceFrames, resampler->destination + channel * destinationFrames);
    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (std::size_t frame = 0; frame < destinationFrames; ++frame)
            destination.data[frame * channels + channel] = resampler->destination[channel * destinationFrames + frame];
    }
    return static_cast<int>(destination.size);
}

WebRtcMutableInterleavedView16 MutableData(WebRtcAudioFrame* frame, std::size_t samplesPerChannel, std::size_t channels) {
    const std::size_t total = samplesPerChannel * channels;
    if (total > WebRtcAudioFrameMaxDataSizeSamples)
        throw std::invalid_argument("AudioFrame::mutable_data: more samples than the frame holds");
    if (channels > WebRtcAudioFrameMaxConcurrentChannels)
        throw std::invalid_argument("AudioFrame::mutable_data: more channels than kMaxConcurrentChannels");
    if (frame->muted) {
        std::memset(frame->data, 0, total * sizeof(std::int16_t));
        frame->muted = false;
    }
    frame->samplesPerChannel = samplesPerChannel;
    frame->numChannels = channels;
    return {channels, samplesPerChannel, frame->data, total};
}

void UpmixChannels(std::size_t channels, WebRtcAudioFrame* frame) {
    if (frame->numChannels != 1 || frame->samplesPerChannel * channels > WebRtcAudioFrameMaxDataSizeSamples)
        return;
    if (frame->muted) {
        frame->numChannels = channels;
        return;
    }
    const WebRtcMutableInterleavedView16 data = MutableData(frame, frame->samplesPerChannel, channels);
    for (std::size_t i = frame->samplesPerChannel; i-- > 0;) {
        for (std::size_t j = 0; j < channels; ++j)
            data.data[channels * i + j] = data.data[i];
    }
}

}

extern "C" {

void APS5_VABI _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(WebRtcPushSincResampler* push, std::size_t sourceFrames, std::size_t destinationFrames) {
    if (!push)
        throw std::invalid_argument("PushSincResampler constructor requires an object");
    if (sourceFrames <= SincResampler::KernelSize + SincResampler::KernelSize / 2 || sourceFrames > MaxSamplesPerChannel || destinationFrames == 0 ||
        destinationFrames > MaxSamplesPerChannel)
        throw std::invalid_argument("PushSincResampler needs 49 to 3840 source frames and 1 to 3840 destination frames");
    push->vtable = PushSincResamplerVtable + 2;
    push->resampler = nullptr;
    push->floatBuffer = nullptr;
    push->sourcePtr = nullptr;
    push->sourcePtrInt = nullptr;
    push->destinationFrames = destinationFrames;
    push->firstPass = true;
    push->sourceAvailable = 0;
    push->resampler = new SincResampler(static_cast<double>(sourceFrames) / static_cast<double>(destinationFrames), sourceFrames, push);
}

void APS5_VABI _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(WebRtcPushSincResampler* push) {
    if (!push)
        throw std::invalid_argument("PushSincResampler destructor requires an object");
    delete Sinc(push);
    delete[] push->floatBuffer;
    push->resampler = nullptr;
    push->floatBuffer = nullptr;
}

std::size_t APS5_VABI _ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(WebRtcPushSincResampler* push, const float* source, std::size_t sourceLength, float* destination, std::size_t destinationCapacity) {
    if (!push || !push->resampler || !destination)
        throw std::invalid_argument("PushSincResampler::Resample requires a constructed resampler and a destination");
    if (sourceLength != Sinc(push)->RequestFrames())
        throw std::invalid_argument("PushSincResampler::Resample needs exactly the source frame count it was built for");
    if (destinationCapacity < push->destinationFrames)
        throw std::invalid_argument("PushSincResampler::Resample destination is too small");
    if (!source && !push->sourcePtrInt)
        throw std::invalid_argument("PushSincResampler::Resample requires a source");
    push->sourcePtr = source;
    push->sourceAvailable = sourceLength;
    if (push->firstPass)
        Sinc(push)->Resample(Sinc(push)->ChunkSize(), destination);
    Sinc(push)->Resample(push->destinationFrames, destination);
    push->sourcePtr = nullptr;
    return push->destinationFrames;
}

void APS5_VABI _ZN6webrtc13PushResamplerIsEC1Ev_nid_postfix(WebRtcPushResampler16* resampler) {
    if (!resampler)
        throw std::invalid_argument("PushResampler constructor requires an object");
    Clear(resampler);
}

void APS5_VABI _ZN6webrtc13PushResamplerIsEC1Emmm_nid_postfix(WebRtcPushResampler16* resampler, std::size_t sourceSamplesPerChannel, std::size_t destinationSamplesPerChannel, std::size_t channels) {
    if (!resampler)
        throw std::invalid_argument("PushResampler constructor requires an object");
    Clear(resampler);
    EnsureInitialized(resampler, sourceSamplesPerChannel, destinationSamplesPerChannel, channels);
}

void APS5_VABI _ZN6webrtc13PushResamplerIsED1Ev_nid_postfix(WebRtcPushResampler16* resampler) {
    if (!resampler)
        throw std::invalid_argument("PushResampler destructor requires an object");
    ReleaseResamplers(resampler);
    delete[] resampler->source;
    delete[] resampler->destination;
    Clear(resampler);
}

int APS5_VABI _ZN6webrtc13PushResamplerIsE8ResampleENS_15InterleavedViewIKsEENS2_IsEE_nid_postfix(WebRtcPushResampler16* resampler, WebRtcInterleavedView source, WebRtcMutableInterleavedView16 destination) {
    if (!resampler)
        throw std::invalid_argument("PushResampler::Resample requires a resampler");
    return ResampleInterleaved(resampler, source, destination);
}

int APS5_VABI _ZN6webrtc13PushResamplerIsE8ResampleEN3rtc9ArrayViewIKsLln4711EEENS3_IsLln4711EEE_nid_postfix(WebRtcPushResampler16* resampler, WebRtcConstMonoView16 source, WebRtcMonoView16 destination) {
    if (!resampler)
        throw std::invalid_argument("PushResampler::Resample requires a resampler");
    if (source.size == destination.size) {
        if (source.size != 0)
            std::memcpy(destination.data, source.data, source.size * sizeof(std::int16_t));
        return static_cast<int>(source.size);
    }
    if (ResamplerCount(resampler) != 1)
        throw std::invalid_argument("PushResampler::Resample on a mono view needs a one-channel resampler");
    WebRtcPushSincResampler* push = resampler->resamplersBegin[0];
    if (destination.size < push->destinationFrames)
        throw std::invalid_argument("PushResampler::Resample destination is too small");
    return static_cast<int>(ResampleInt16(push, source.data, source.size, destination.data));
}

void APS5_VABI _ZN6webrtc3voe16RemixAndResampleENS_15InterleavedViewIKsEEiPNS_13PushResamplerIsEEPNS_10AudioFrameE_nid_postfix(WebRtcInterleavedView source, int, WebRtcPushResampler16* resampler, WebRtcAudioFrame* frame) {
    if (!resampler || !frame)
        throw std::invalid_argument("voe::RemixAndResample requires a resampler and a frame");
    if (source.numChannels == 0 || source.numChannels > MaxChannels || source.samplesPerChannel > MaxSamplesPerChannel ||
        source.size != source.samplesPerChannel * source.numChannels || (source.size != 0 && !source.data))
        throw std::invalid_argument("voe::RemixAndResample needs 1 to 8 channels, at most 3840 samples per channel and a matching view");
    std::array<std::int16_t, WebRtcAudioFrameMaxDataSizeSamples> downmixed;
    if (source.numChannels > frame->numChannels) {
        const std::size_t frames = source.samplesPerChannel;
        if (frames * frame->numChannels > downmixed.size())
            throw std::invalid_argument("voe::RemixAndResample: more samples than a frame holds");
        if (source.numChannels > 1 && frame->numChannels == 1) {
            for (std::size_t i = 0; i < frames; ++i) {
                std::int32_t value = 0;
                for (std::size_t channel = 0; channel < source.numChannels; ++channel)
                    value += source.data[i * source.numChannels + channel];
                downmixed[i] = static_cast<std::int16_t>(value / static_cast<std::int32_t>(source.numChannels));
            }
        } else if (source.numChannels == 4 && frame->numChannels == 2) {
            for (std::size_t i = 0; i < frames; ++i) {
                downmixed[2 * i] = static_cast<std::int16_t>((static_cast<std::int32_t>(source.data[4 * i]) + source.data[4 * i + 1]) >> 1);
                downmixed[2 * i + 1] = static_cast<std::int16_t>((static_cast<std::int32_t>(source.data[4 * i + 2]) + source.data[4 * i + 3]) >> 1);
            }
        } else {
            throw std::invalid_argument("voe::RemixAndResample can only downmix to mono or from four channels to stereo");
        }
        source = {frame->numChannels, frames, downmixed.data(), frames * frame->numChannels};
    }
    const std::size_t originalChannels = frame->numChannels;
    ResampleInterleaved(resampler, source, MutableData(frame, frame->samplesPerChannel, source.numChannels));
    if (source.numChannels == 1 && originalChannels == 2)
        UpmixChannels(2, frame);
}

}
