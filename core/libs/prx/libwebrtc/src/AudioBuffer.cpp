/* Derived from modules/audio_processing/audio_buffer.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from common_audio/channel_buffer.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/splitting_filter.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/audio_processing/three_band_filter_bank.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t MaxSamplesPerChannel10ms = 3840;
constexpr std::size_t MaxConcurrentChannels = 8;
constexpr std::size_t SamplesPer32kHzChannel = 320;
constexpr std::size_t SamplesPer48kHzChannel = 480;

template<typename T> T* AllocateZeroed(std::size_t count) {
    if (count == 0)
        return nullptr;
    auto* storage = static_cast<T*>(_Znwm_nid_postfix(count * sizeof(T)));
    std::memset(static_cast<void*>(storage), 0, count * sizeof(T));
    return storage;
}

void Release(void* storage) {
    if (storage)
        _ZdlPv_nid_postfix(storage);
}

std::size_t FramesFromRate(std::size_t rate) {
    return static_cast<std::size_t>(static_cast<int>(rate) / 100);
}

std::size_t NumBandsFromFramesPerChannel(std::size_t frames) {
    if (frames == SamplesPer32kHzChannel)
        return 2;
    if (frames == SamplesPer48kHzChannel)
        return 3;
    return 1;
}

std::int16_t FloatS16ToS16(float value) {
    value = std::min(value, 32767.f);
    value = std::max(value, -32768.f);
    return static_cast<std::int16_t>(value + std::copysign(0.5f, value));
}

WebRtcFloatViewTable AllocateViewTable(std::size_t rows, std::size_t columns) {
    WebRtcFloatViewTable table{};
    table.begin = AllocateZeroed<WebRtcFloatViewVector>(rows);
    table.end = table.begin + rows;
    table.capacity = table.end;
    for (std::size_t row = 0; row < rows; ++row) {
        table.begin[row].begin = AllocateZeroed<WebRtcFloatView>(columns);
        table.begin[row].end = table.begin[row].begin + columns;
        table.begin[row].capacity = table.begin[row].end;
    }
    return table;
}

void ReleaseViewTable(WebRtcFloatViewTable& table) {
    for (auto* row = table.begin; row != table.end; ++row)
        Release(row->begin);
    Release(table.begin);
    table = {};
}

void ReleaseChannelBuffer(WebRtcChannelBufferFloat* buffer) {
    if (!buffer)
        return;
    ReleaseViewTable(buffer->bandsView);
    ReleaseViewTable(buffer->channelsView);
    Release(buffer->bands);
    Release(buffer->channels);
    Release(buffer->data);
    Release(buffer);
}

WebRtcChannelBufferFloat* CreateChannelBuffer(std::size_t frames, std::size_t channels, std::size_t bands) {
    auto* buffer = AllocateZeroed<WebRtcChannelBufferFloat>(1);
    try {
        buffer->data = AllocateZeroed<float>(frames * channels);
        buffer->channels = AllocateZeroed<float*>(channels * bands);
        buffer->bands = AllocateZeroed<float*>(channels * bands);
        buffer->numFrames = frames;
        buffer->numFramesPerBand = frames / bands;
        buffer->numAllocatedChannels = channels;
        buffer->numChannels = channels;
        buffer->numBands = bands;
        buffer->bandsView = AllocateViewTable(channels, bands);
        buffer->channelsView = AllocateViewTable(bands, channels);
    } catch (...) {
        ReleaseChannelBuffer(buffer);
        throw;
    }
    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (std::size_t band = 0; band < bands; ++band) {
            const WebRtcFloatView view{&buffer->data[channel * frames + band * buffer->numFramesPerBand], buffer->numFramesPerBand};
            buffer->channelsView.begin[band].begin[channel] = view;
            buffer->bandsView.begin[channel].begin[band] = view;
            buffer->channels[band * channels + channel] = view.data;
            buffer->bands[channel * bands + band] = view.data;
        }
    }
    return buffer;
}

void ReleaseSplittingFilter(WebRtcSplittingFilter* filter) {
    if (!filter)
        return;
    Release(filter->twoBandsBegin);
    Release(filter->threeBandsBegin);
    Release(filter);
}

WebRtcSplittingFilter* CreateSplittingFilter(std::size_t channels, std::size_t bands) {
    auto* filter = AllocateZeroed<WebRtcSplittingFilter>(1);
    try {
        filter->numBands = bands;
        const std::size_t twoBands = bands == 2 ? channels : 0;
        const std::size_t threeBands = bands == 3 ? channels : 0;
        filter->twoBandsBegin = AllocateZeroed<WebRtcTwoBandsStates>(twoBands);
        filter->twoBandsEnd = filter->twoBandsBegin + twoBands;
        filter->twoBandsCapacity = filter->twoBandsEnd;
        filter->threeBandsBegin = AllocateZeroed<WebRtcThreeBandFilterBank>(threeBands);
        filter->threeBandsEnd = filter->threeBandsBegin + threeBands;
        filter->threeBandsCapacity = filter->threeBandsEnd;
    } catch (...) {
        ReleaseSplittingFilter(filter);
        throw;
    }
    return filter;
}

void ReleaseResamplers(WebRtcResamplerVector& resamplers) {
    for (auto** resampler = resamplers.begin; resampler != resamplers.end; ++resampler) {
        if (*resampler) {
            _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(*resampler);
            Release(*resampler);
        }
    }
    Release(resamplers.begin);
    resamplers = {};
}

void CreateResamplers(WebRtcResamplerVector& resamplers, std::size_t count, std::size_t sourceFrames, std::size_t destinationFrames) {
    resamplers.begin = AllocateZeroed<WebRtcPushSincResampler*>(count);
    resamplers.end = resamplers.begin;
    resamplers.capacity = resamplers.begin + count;
    for (std::size_t i = 0; i < count; ++i) {
        auto* resampler = AllocateZeroed<WebRtcPushSincResampler>(1);
        try {
            _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(resampler, sourceFrames, destinationFrames);
        } catch (...) {
            Release(resampler);
            throw;
        }
        *resamplers.end++ = resampler;
    }
}

void ReleaseParts(WebRtcAudioBuffer* buffer) {
    ReleaseResamplers(buffer->outputResamplers);
    ReleaseResamplers(buffer->inputResamplers);
    ReleaseSplittingFilter(buffer->splittingFilter);
    ReleaseChannelBuffer(buffer->splitData);
    ReleaseChannelBuffer(buffer->data);
    buffer->splittingFilter = nullptr;
    buffer->splitData = nullptr;
    buffer->data = nullptr;
}

void Resample(WebRtcPushSincResampler* resampler, const float* source, std::size_t sourceFrames, float* destination, std::size_t destinationFrames) {
    _ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(resampler, source, sourceFrames, destination, destinationFrames);
}

void RestoreNumChannels(WebRtcAudioBuffer* buffer) {
    buffer->numChannels = buffer->bufferNumChannels;
    buffer->data->numChannels = buffer->bufferNumChannels;
    if (buffer->splitData)
        buffer->splitData->numChannels = buffer->bufferNumChannels;
}

void RequireConstructed(const WebRtcAudioBuffer* buffer, const char* function) {
    if (!buffer || !buffer->data)
        throw std::invalid_argument(std::string(function) + " requires a constructed AudioBuffer");
}

void APS5_VABI DestroyAudioBuffer_nid_no_patch(WebRtcAudioBuffer* buffer) {
    if (!buffer)
        throw std::invalid_argument("AudioBuffer destructor requires an object");
    ReleaseParts(buffer);
}

void APS5_VABI DeleteAudioBuffer_nid_no_patch(WebRtcAudioBuffer* buffer) {
    DestroyAudioBuffer_nid_no_patch(buffer);
    _ZdlPv_nid_postfix(buffer);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&DestroyAudioBuffer_nid_no_patch),
    reinterpret_cast<void*>(&DeleteAudioBuffer_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc11AudioBufferC1Emmmmmm_nid_postfix(WebRtcAudioBuffer* buffer, std::size_t inputRate, std::size_t inputChannels, std::size_t bufferRate,
                                                            std::size_t bufferChannels, std::size_t outputRate, std::size_t) {
    if (!buffer)
        throw std::invalid_argument("AudioBuffer constructor requires an object");
    {
        const int savedErrno = errno;
        const bool writable = GuestWritable_nid_no_patch(buffer, sizeof(*buffer));
        errno = savedErrno;
        if (!writable)
            throw std::invalid_argument("AudioBuffer constructor requires a writable object");
    }
    const std::size_t inputFrames = FramesFromRate(inputRate);
    const std::size_t bufferFrames = FramesFromRate(bufferRate);
    const std::size_t outputFrames = FramesFromRate(outputRate);
    for (const std::size_t frames : {inputFrames, bufferFrames, outputFrames}) {
        if (frames == 0 || frames > MaxSamplesPerChannel10ms)
            throw std::invalid_argument("AudioBuffer needs sample rates of 100 to 384000 Hz");
    }
    if (inputChannels == 0 || inputChannels > MaxConcurrentChannels || bufferChannels == 0 || bufferChannels > inputChannels)
        throw std::invalid_argument("AudioBuffer needs 1 to 8 input channels and 1 to input_num_channels buffer channels");
    WebRtcAudioBuffer built{};
    built.vtable = Vtable + 2;
    built.inputNumFrames = inputFrames;
    built.inputNumChannels = inputChannels;
    built.bufferNumFrames = bufferFrames;
    built.bufferNumChannels = bufferChannels;
    built.outputNumFrames = outputFrames;
    built.outputNumChannels = 0;
    built.numChannels = bufferChannels;
    built.numBands = NumBandsFromFramesPerChannel(bufferFrames);
    built.numSplitFrames = bufferFrames / built.numBands;
    built.downmixByAveraging = true;
    built.channelForDownmixing = 0;
    try {
        built.data = CreateChannelBuffer(bufferFrames, bufferChannels, 1);
        if (inputFrames != bufferFrames)
            CreateResamplers(built.inputResamplers, bufferChannels, inputFrames, bufferFrames);
        if (outputFrames != bufferFrames)
            CreateResamplers(built.outputResamplers, bufferChannels, bufferFrames, outputFrames);
        if (built.numBands > 1) {
            built.splitData = CreateChannelBuffer(bufferFrames, bufferChannels, built.numBands);
            built.splittingFilter = CreateSplittingFilter(bufferChannels, built.numBands);
        }
    } catch (...) {
        ReleaseParts(&built);
        throw;
    }
    const int savedErrno = errno;
    const bool copiedOut = GuestPtr<WebRtcAudioBuffer>(buffer).CopyOut(built);
    errno = savedErrno;
    if (!copiedOut) {
        ReleaseParts(&built);
        throw std::invalid_argument("AudioBuffer constructor requires a writable object");
    }
}

void APS5_VABI _ZN6webrtc11AudioBuffer8CopyFromEPKsRKNS_12StreamConfigE_nid_postfix(WebRtcAudioBuffer* buffer, const std::int16_t* interleaved,
                                                                                     const WebRtcStreamConfig* config) {
    RequireConstructed(buffer, "AudioBuffer::CopyFrom");
    if (!interleaved || !config)
        throw std::invalid_argument("AudioBuffer::CopyFrom requires interleaved data and a stream config");
    if (config->numChannels != buffer->inputNumChannels || config->numFrames != buffer->inputNumFrames)
        throw std::invalid_argument("AudioBuffer::CopyFrom stream config does not match the input format");
    RestoreNumChannels(buffer);
    const std::size_t inputFrames = buffer->inputNumFrames;
    const std::size_t inputChannels = buffer->inputNumChannels;
    const std::size_t bufferFrames = buffer->bufferNumFrames;
    const bool resampling = inputFrames != bufferFrames;
    float* const* channels = buffer->data->channels;
    std::array<float, MaxSamplesPerChannel10ms> floats;
    if (buffer->numChannels == 1) {
        if (inputChannels == 1) {
            float* destination = resampling ? floats.data() : channels[0];
            for (std::size_t i = 0; i < inputFrames; ++i)
                destination[i] = interleaved[i];
            if (resampling)
                Resample(buffer->inputResamplers.begin[0], floats.data(), inputFrames, channels[0], bufferFrames);
            return;
        }
        float* downmixed = resampling ? floats.data() : channels[0];
        if (buffer->downmixByAveraging) {
            for (std::size_t j = 0, k = 0; j < inputFrames; ++j) {
                std::int32_t sum = 0;
                for (std::size_t i = 0; i < inputChannels; ++i, ++k)
                    sum += interleaved[k];
                downmixed[j] = static_cast<float>(sum / static_cast<std::int16_t>(inputChannels));
            }
        } else {
            for (std::size_t j = 0, k = buffer->channelForDownmixing; j < inputFrames; ++j, k += inputChannels)
                downmixed[j] = interleaved[k];
        }
        if (resampling)
            Resample(buffer->inputResamplers.begin[0], downmixed, inputFrames, channels[0], bufferFrames);
        return;
    }
    const std::size_t numChannels = buffer->numChannels;
    for (std::size_t channel = 0; channel < numChannels; ++channel) {
        float* destination = resampling ? floats.data() : channels[channel];
        for (std::size_t j = 0, k = channel; j < inputFrames; ++j, k += numChannels)
            destination[j] = interleaved[k];
        if (resampling)
            Resample(buffer->inputResamplers.begin[channel], floats.data(), inputFrames, channels[channel], bufferFrames);
    }
}

void APS5_VABI _ZN6webrtc11AudioBuffer6CopyToERKNS_12StreamConfigEPs_nid_postfix(WebRtcAudioBuffer* buffer, const WebRtcStreamConfig* config,
                                                                                  std::int16_t* interleaved) {
    RequireConstructed(buffer, "AudioBuffer::CopyTo");
    if (!interleaved || !config)
        throw std::invalid_argument("AudioBuffer::CopyTo requires a stream config and interleaved data");
    const std::size_t configChannels = config->numChannels;
    const std::size_t numChannels = buffer->numChannels;
    if ((configChannels != numChannels && numChannels != 1) || configChannels > MaxConcurrentChannels || config->numFrames != buffer->outputNumFrames)
        throw std::invalid_argument("AudioBuffer::CopyTo stream config does not match the output format");
    const std::size_t bufferFrames = buffer->bufferNumFrames;
    const std::size_t outputFrames = buffer->outputNumFrames;
    const bool resampling = bufferFrames != outputFrames;
    float* const* channels = buffer->data->channels;
    std::array<float, MaxSamplesPerChannel10ms> floats;
    if (numChannels == 1) {
        if (resampling)
            Resample(buffer->outputResamplers.begin[0], channels[0], bufferFrames, floats.data(), outputFrames);
        const float* deinterleaved = resampling ? floats.data() : channels[0];
        for (std::size_t i = 0, k = 0; i < outputFrames; ++i) {
            const std::int16_t sample = FloatS16ToS16(deinterleaved[i]);
            for (std::size_t j = 0; j < configChannels; ++j, ++k)
                interleaved[k] = sample;
        }
        return;
    }
    for (std::size_t channel = 0; channel < numChannels; ++channel) {
        const float* source = channels[channel];
        if (resampling) {
            Resample(buffer->outputResamplers.begin[channel], channels[channel], bufferFrames, floats.data(), outputFrames);
            source = floats.data();
        }
        for (std::size_t k = 0, j = channel; k < outputFrames; ++k, j += configChannels)
            interleaved[j] = FloatS16ToS16(source[k]);
    }
}

}
