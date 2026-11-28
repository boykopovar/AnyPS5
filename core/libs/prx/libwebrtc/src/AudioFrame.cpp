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
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

const std::int16_t ZeroedData[WebRtcAudioFrameMaxDataSizeSamples] = {};

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    ErrnoGuard guard;
    if (!source)
        throw std::invalid_argument(message);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
    if (!GuestReadable_nid_no_patch(source, bytes) || !GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

class GuestWriteScope {
public:
    GuestWriteScope(void* pointer, std::size_t bytes, const char* message)
        : guard(pointer ? pointer : nullptr, pointer ? bytes : 0) {
        if (!bytes) return;
        ErrnoGuard saved;
        if (!pointer || !guard.Open() || !GuestWritable_nid_no_patch(pointer, bytes))
            throw std::invalid_argument(message);
    }
    GuestWriteScope(const GuestWriteScope&) = delete;
    GuestWriteScope& operator=(const GuestWriteScope&) = delete;

private:
    GuestArena::HostWrite guard;
};

void RequireWritable(void* pointer, std::size_t bytes, const char* message) {
    GuestWriteScope scope(pointer, bytes, message);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    GuestWriteScope writeScope(destination, bytes, message);
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

void* FieldAddress(WebRtcAudioFrame* frame, std::size_t offset) {
    return reinterpret_cast<unsigned char*>(frame) + offset;
}

const void* FieldAddress(const WebRtcAudioFrame* frame, std::size_t offset) {
    return reinterpret_cast<const unsigned char*>(frame) + offset;
}

std::int16_t* DataAddress(WebRtcAudioFrame* frame) {
    return reinterpret_cast<std::int16_t*>(FieldAddress(frame, offsetof(WebRtcAudioFrame, data)));
}

const std::int16_t* DataAddress(const WebRtcAudioFrame* frame) {
    return reinterpret_cast<const std::int16_t*>(FieldAddress(frame, offsetof(WebRtcAudioFrame, data)));
}

template<typename T> T LoadField(const WebRtcAudioFrame* frame, std::size_t offset, const char* message) {
    T value{};
    CopyIn(&value, FieldAddress(frame, offset), sizeof(value), message);
    return value;
}

template<typename T> void StoreField(WebRtcAudioFrame* frame, std::size_t offset, T value, const char* message) {
    CopyOut(FieldAddress(frame, offset), &value, sizeof(value), message);
}

std::int32_t GuessChannelLayout(int channels) {
    switch (channels) {
    case 1:
        return WebRtcChannelLayoutMono;
    case 2:
        return WebRtcChannelLayoutStereo;
    case 3:
        return WebRtcChannelLayoutSurround;
    case 4:
        return WebRtcChannelLayoutQuad;
    case 5:
        return WebRtcChannelLayout5_0;
    case 6:
        return WebRtcChannelLayout5_1;
    case 7:
        return WebRtcChannelLayout6_1;
    case 8:
        return WebRtcChannelLayout7_1;
    default:
        return WebRtcChannelLayoutUnsupported;
    }
}

}

extern "C" {

void APS5_VABI _ZN6webrtc10AudioFrameC1Ev_nid_postfix(WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::AudioFrame requires a frame");
    const char* unwritable = "AudioFrame::AudioFrame requires a writable frame";
    RequireWritable(FieldAddress(frame, 0), offsetof(WebRtcAudioFrame, data), unwritable);
    RequireWritable(FieldAddress(frame, offsetof(WebRtcAudioFrame, muted)),
        sizeof(WebRtcAudioFrame) - offsetof(WebRtcAudioFrame, muted), unwritable);
    StoreField<std::uint32_t>(frame, offsetof(WebRtcAudioFrame, timestamp), 0, unwritable);
    StoreField<std::int64_t>(frame, offsetof(WebRtcAudioFrame, elapsedTimeMs), -1, unwritable);
    StoreField<std::int64_t>(frame, offsetof(WebRtcAudioFrame, ntpTimeMs), -1, unwritable);
    StoreField<std::size_t>(frame, offsetof(WebRtcAudioFrame, samplesPerChannel), 0, unwritable);
    StoreField<std::int32_t>(frame, offsetof(WebRtcAudioFrame, sampleRateHz), 0, unwritable);
    StoreField<std::size_t>(frame, offsetof(WebRtcAudioFrame, numChannels), 0, unwritable);
    StoreField<std::int32_t>(frame, offsetof(WebRtcAudioFrame, speechType), WebRtcAudioFrameSpeechUndefined, unwritable);
    StoreField<std::int32_t>(frame, offsetof(WebRtcAudioFrame, vadActivity), WebRtcAudioFrameVadUnknown, unwritable);
    StoreField<std::int64_t>(frame, offsetof(WebRtcAudioFrame, profileTimestampMs), 0, unwritable);
    StoreField<void*>(frame, offsetof(WebRtcAudioFrame, packetInfos), nullptr, unwritable);
    StoreField<bool>(frame, offsetof(WebRtcAudioFrame, muted), true, unwritable);
    StoreField<std::int32_t>(frame, offsetof(WebRtcAudioFrame, channelLayout), WebRtcChannelLayoutNone, unwritable);
    StoreField<char>(frame, offsetof(WebRtcAudioFrame, absoluteCaptureTimestampMs), 0, unwritable);
    StoreField<bool>(frame, offsetof(WebRtcAudioFrame, absoluteCaptureTimestampMsEngaged), false, unwritable);
}

void APS5_VABI _ZN6webrtc10AudioFrame4MuteEv_nid_postfix(WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::Mute requires a frame");
    StoreField<bool>(frame, offsetof(WebRtcAudioFrame, muted), true, "AudioFrame::Mute requires a writable frame");
}

bool APS5_VABI _ZNK6webrtc10AudioFrame5mutedEv_nid_postfix(const WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::muted requires a frame");
    return LoadField<bool>(frame, offsetof(WebRtcAudioFrame, muted), "AudioFrame::muted requires a readable frame");
}

const std::int16_t* APS5_VABI _ZNK6webrtc10AudioFrame4dataEv_nid_postfix(const WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::data requires a frame");
    return LoadField<bool>(frame, offsetof(WebRtcAudioFrame, muted), "AudioFrame::data requires a readable frame") ? ZeroedData : DataAddress(frame);
}

std::int16_t* APS5_VABI _ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::mutable_data requires a frame");
    if (LoadField<bool>(frame, offsetof(WebRtcAudioFrame, muted), "AudioFrame::mutable_data requires a readable frame")) {
        const char* unwritable = "AudioFrame::mutable_data requires a writable frame";
        RequireWritable(DataAddress(frame), sizeof(WebRtcAudioFrame::data), unwritable);
        RequireWritable(FieldAddress(frame, offsetof(WebRtcAudioFrame, muted)), sizeof(bool), unwritable);
        CopyOut(DataAddress(frame), ZeroedData, sizeof(WebRtcAudioFrame::data), unwritable);
        StoreField<bool>(frame, offsetof(WebRtcAudioFrame, muted), false, unwritable);
    }
    return DataAddress(frame);
}

void APS5_VABI _ZN6webrtc10AudioFrame11UpdateFrameEjPKsmiNS0_10SpeechTypeENS0_11VADActivityEm_nid_postfix(WebRtcAudioFrame* frame, std::uint32_t timestamp, const std::int16_t* data, std::size_t samplesPerChannel, std::int32_t sampleRateHz, std::int32_t speechType, std::int32_t vadActivity, std::size_t numChannels) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::UpdateFrame requires a frame");
    if (numChannels > WebRtcAudioFrameMaxConcurrentChannels)
        throw std::invalid_argument("AudioFrame::UpdateFrame: more channels than kMaxConcurrentChannels");
    const char* unwritable = "AudioFrame::UpdateFrame requires a writable frame";
    RequireWritable(FieldAddress(frame, offsetof(WebRtcAudioFrame, timestamp)), sizeof(timestamp), unwritable);
    RequireWritable(FieldAddress(frame, offsetof(WebRtcAudioFrame, samplesPerChannel)),
        offsetof(WebRtcAudioFrame, profileTimestampMs) - offsetof(WebRtcAudioFrame, samplesPerChannel), unwritable);
    RequireWritable(FieldAddress(frame, offsetof(WebRtcAudioFrame, muted)),
        offsetof(WebRtcAudioFrame, absoluteCaptureTimestampMs) - offsetof(WebRtcAudioFrame, muted), unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, timestamp), timestamp, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, samplesPerChannel), samplesPerChannel, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, sampleRateHz), sampleRateHz, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, speechType), speechType, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, vadActivity), vadActivity, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, numChannels), numChannels, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, channelLayout), GuessChannelLayout(static_cast<int>(numChannels)), unwritable);
    if (numChannels != 0 && samplesPerChannel > WebRtcAudioFrameMaxDataSizeSamples / numChannels)
        throw std::invalid_argument("AudioFrame::UpdateFrame: more samples than the frame holds");
    const std::size_t length = samplesPerChannel * numChannels;
    if (data) {
        std::vector<std::int16_t> samples(length);
        CopyIn(samples.data(), data, sizeof(std::int16_t) * length, "AudioFrame::UpdateFrame requires readable samples");
        RequireWritable(DataAddress(frame), sizeof(std::int16_t) * length, unwritable);
        CopyOut(DataAddress(frame), samples.data(), sizeof(std::int16_t) * length, unwritable);
        StoreField<bool>(frame, offsetof(WebRtcAudioFrame, muted), false, unwritable);
    } else {
        StoreField<bool>(frame, offsetof(WebRtcAudioFrame, muted), true, unwritable);
    }
}

void APS5_VABI _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(WebRtcAudioFrame* frame, const WebRtcAudioFrame* source) {
    if (!frame || !source)
        throw std::invalid_argument("AudioFrame::CopyFrom requires two frames");
    if (frame == source)
        return;
    const char* unreadable = "AudioFrame::CopyFrom requires readable frames";
    const char* unwritable = "AudioFrame::CopyFrom requires a writable frame";

    const auto sourceTimestamp = LoadField<std::uint32_t>(source, offsetof(WebRtcAudioFrame, timestamp), unreadable);
    const auto sourceElapsedTimeMs = LoadField<std::int64_t>(source, offsetof(WebRtcAudioFrame, elapsedTimeMs), unreadable);
    const auto sourceNtpTimeMs = LoadField<std::int64_t>(source, offsetof(WebRtcAudioFrame, ntpTimeMs), unreadable);
    const auto sourceSamplesPerChannel = LoadField<std::size_t>(source, offsetof(WebRtcAudioFrame, samplesPerChannel), unreadable);
    const auto sourceSampleRateHz = LoadField<std::int32_t>(source, offsetof(WebRtcAudioFrame, sampleRateHz), unreadable);
    const auto sourceNumChannels = LoadField<std::size_t>(source, offsetof(WebRtcAudioFrame, numChannels), unreadable);
    const auto sourceSpeechType = LoadField<std::int32_t>(source, offsetof(WebRtcAudioFrame, speechType), unreadable);
    const auto sourceVadActivity = LoadField<std::int32_t>(source, offsetof(WebRtcAudioFrame, vadActivity), unreadable);
    auto* sourcePacketInfos = static_cast<WebRtcRtpPacketInfosData*>(LoadField<void*>(source, offsetof(WebRtcAudioFrame, packetInfos), unreadable));
    const auto sourceMuted = LoadField<bool>(source, offsetof(WebRtcAudioFrame, muted), unreadable);
    const auto sourceChannelLayout = LoadField<std::int32_t>(source, offsetof(WebRtcAudioFrame, channelLayout), unreadable);
    decltype(WebRtcAudioFrame::absoluteCaptureTimestampMs) sourceCaptureTimestamp{};
    CopyIn(&sourceCaptureTimestamp, FieldAddress(source, offsetof(WebRtcAudioFrame, absoluteCaptureTimestampMs)), sizeof(sourceCaptureTimestamp), unreadable);
    const auto sourceCaptureTimestampEngaged = LoadField<bool>(source, offsetof(WebRtcAudioFrame, absoluteCaptureTimestampMsEngaged), unreadable);

    if (sourceNumChannels != 0 && sourceSamplesPerChannel > WebRtcAudioFrameMaxDataSizeSamples / sourceNumChannels)
        throw std::invalid_argument("AudioFrame::CopyFrom: more samples than the frame holds");
    const std::size_t size = sourceNumChannels * sourceSamplesPerChannel;

    const auto frameMuted = LoadField<bool>(frame, offsetof(WebRtcAudioFrame, muted), unreadable);
    auto* oldPacketInfos = static_cast<WebRtcRtpPacketInfosData*>(LoadField<void*>(frame, offsetof(WebRtcAudioFrame, packetInfos), unreadable));

    std::vector<std::int16_t> samples;
    if (!sourceMuted && size != 0) {
        samples.resize(size);
        CopyIn(samples.data(), DataAddress(source), sizeof(std::int16_t) * size, unreadable);
    }

    RequireWritable(FieldAddress(frame, 0), offsetof(WebRtcAudioFrame, profileTimestampMs), unwritable);
    RequireWritable(FieldAddress(frame, offsetof(WebRtcAudioFrame, packetInfos)), sizeof(void*), unwritable);
    RequireWritable(FieldAddress(frame, offsetof(WebRtcAudioFrame, muted)),
        sizeof(WebRtcAudioFrame) - offsetof(WebRtcAudioFrame, muted), unwritable);
    if (frameMuted && !sourceMuted)
        RequireWritable(DataAddress(frame), sizeof(WebRtcAudioFrame::data), unwritable);
    else if (!sourceMuted && size != 0)
        RequireWritable(DataAddress(frame), sizeof(std::int16_t) * size, unwritable);

    std::optional<GuestWriteScope> sourceInfosWrite;
    if (sourcePacketInfos)
        sourceInfosWrite.emplace(sourcePacketInfos, sizeof(*sourcePacketInfos), unwritable);
    std::optional<GuestWriteScope> oldInfosWrite;
    if (oldPacketInfos && oldPacketInfos != sourcePacketInfos)
        oldInfosWrite.emplace(oldPacketInfos, sizeof(*oldPacketInfos), unwritable);

    if (frameMuted && !sourceMuted)
        CopyOut(DataAddress(frame), ZeroedData, sizeof(WebRtcAudioFrame::data), unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, timestamp), sourceTimestamp, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, elapsedTimeMs), sourceElapsedTimeMs, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, ntpTimeMs), sourceNtpTimeMs, unwritable);
    if (sourcePacketInfos)
        sourcePacketInfos->refCount.fetch_add(1, std::memory_order_relaxed);
    ReleaseRtpPacketInfos(oldPacketInfos);
    StoreField<void*>(frame, offsetof(WebRtcAudioFrame, packetInfos), sourcePacketInfos, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, muted), sourceMuted, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, samplesPerChannel), sourceSamplesPerChannel, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, sampleRateHz), sourceSampleRateHz, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, speechType), sourceSpeechType, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, vadActivity), sourceVadActivity, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, numChannels), sourceNumChannels, unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, channelLayout), sourceChannelLayout, unwritable);
    CopyOut(FieldAddress(frame, offsetof(WebRtcAudioFrame, absoluteCaptureTimestampMs)), &sourceCaptureTimestamp, sizeof(sourceCaptureTimestamp), unwritable);
    StoreField(frame, offsetof(WebRtcAudioFrame, absoluteCaptureTimestampMsEngaged), sourceCaptureTimestampEngaged, unwritable);
    if (!sourceMuted && size != 0)
        CopyOut(DataAddress(frame), samples.data(), sizeof(std::int16_t) * size, unwritable);
}

WebRtcInterleavedView APS5_VABI _ZNK6webrtc10AudioFrame9data_viewEv_nid_postfix(const WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::data_view requires a frame");
    const char* unreadable = "AudioFrame::data_view requires a readable frame";
    const std::size_t numChannels = LoadField<std::size_t>(frame, offsetof(WebRtcAudioFrame, numChannels), unreadable);
    const std::size_t samplesPerChannel = LoadField<std::size_t>(frame, offsetof(WebRtcAudioFrame, samplesPerChannel), unreadable);
    if (numChannels != 0 && samplesPerChannel > std::numeric_limits<std::size_t>::max() / numChannels)
        throw std::invalid_argument("AudioFrame::data_view: sample count overflow");
    const std::size_t size = numChannels * samplesPerChannel;
    const std::int16_t* data = LoadField<bool>(frame, offsetof(WebRtcAudioFrame, muted), unreadable) ? ZeroedData : DataAddress(frame);
    return {numChannels, samplesPerChannel, size == 0 ? nullptr : data, size};
}

}
