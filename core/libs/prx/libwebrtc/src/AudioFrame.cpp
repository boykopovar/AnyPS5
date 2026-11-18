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

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

const std::int16_t ZeroedData[WebRtcAudioFrameMaxDataSizeSamples] = {};

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* message) {
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* message) {
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

template<typename T> T Load(const T* field, const char* message) {
    T value{};
    CopyIn(&value, field, sizeof(value), message);
    return value;
}

template<typename T> void Store(T* field, T value, const char* message) {
    CopyOut(field, &value, sizeof(value), message);
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
    frame->timestamp = 0;
    frame->elapsedTimeMs = -1;
    frame->ntpTimeMs = -1;
    frame->samplesPerChannel = 0;
    frame->sampleRateHz = 0;
    frame->numChannels = 0;
    frame->speechType = WebRtcAudioFrameSpeechUndefined;
    frame->vadActivity = WebRtcAudioFrameVadUnknown;
    frame->profileTimestampMs = 0;
    frame->packetInfos = nullptr;
    frame->muted = true;
    frame->channelLayout = WebRtcChannelLayoutNone;
    frame->absoluteCaptureTimestampMs.nullState = 0;
    frame->absoluteCaptureTimestampMsEngaged = false;
}

void APS5_VABI _ZN6webrtc10AudioFrame4MuteEv_nid_postfix(WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::Mute requires a frame");
    frame->muted = true;
}

bool APS5_VABI _ZNK6webrtc10AudioFrame5mutedEv_nid_postfix(const WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::muted requires a frame");
    return Load(&frame->muted, "AudioFrame::muted requires a readable frame");
}

const std::int16_t* APS5_VABI _ZNK6webrtc10AudioFrame4dataEv_nid_postfix(const WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::data requires a frame");
    return Load(&frame->muted, "AudioFrame::data requires a readable frame") ? ZeroedData : frame->data;
}

std::int16_t* APS5_VABI _ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::mutable_data requires a frame");
    if (Load(&frame->muted, "AudioFrame::mutable_data requires a readable frame")) {
        CopyOut(frame->data, ZeroedData, sizeof(frame->data), "AudioFrame::mutable_data requires a writable frame");
        Store(&frame->muted, false, "AudioFrame::mutable_data requires a writable frame");
    }
    return frame->data;
}

void APS5_VABI _ZN6webrtc10AudioFrame11UpdateFrameEjPKsmiNS0_10SpeechTypeENS0_11VADActivityEm_nid_postfix(WebRtcAudioFrame* frame, std::uint32_t timestamp, const std::int16_t* data, std::size_t samplesPerChannel, std::int32_t sampleRateHz, std::int32_t speechType, std::int32_t vadActivity, std::size_t numChannels) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::UpdateFrame requires a frame");
    if (numChannels > WebRtcAudioFrameMaxConcurrentChannels)
        throw std::invalid_argument("AudioFrame::UpdateFrame: more channels than kMaxConcurrentChannels");
    const char* unwritable = "AudioFrame::UpdateFrame requires a writable frame";
    Store(&frame->timestamp, timestamp, unwritable);
    Store(&frame->samplesPerChannel, samplesPerChannel, unwritable);
    Store(&frame->sampleRateHz, sampleRateHz, unwritable);
    Store(&frame->speechType, speechType, unwritable);
    Store(&frame->vadActivity, vadActivity, unwritable);
    Store(&frame->numChannels, numChannels, unwritable);
    Store(&frame->channelLayout, GuessChannelLayout(static_cast<int>(numChannels)), unwritable);
    const std::size_t length = samplesPerChannel * numChannels;
    if (length > WebRtcAudioFrameMaxDataSizeSamples)
        throw std::invalid_argument("AudioFrame::UpdateFrame: more samples than the frame holds");
    if (data) {
        std::vector<std::int16_t> samples(length);
        CopyIn(samples.data(), data, sizeof(std::int16_t) * length, "AudioFrame::UpdateFrame requires readable samples");
        CopyOut(frame->data, samples.data(), sizeof(std::int16_t) * length, unwritable);
        Store(&frame->muted, false, unwritable);
    } else {
        Store(&frame->muted, true, unwritable);
    }
}

WebRtcInterleavedView APS5_VABI _ZNK6webrtc10AudioFrame9data_viewEv_nid_postfix(const WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::data_view requires a frame");
    const char* unreadable = "AudioFrame::data_view requires a readable frame";
    const std::size_t numChannels = Load(&frame->numChannels, unreadable);
    const std::size_t samplesPerChannel = Load(&frame->samplesPerChannel, unreadable);
    const std::size_t size = numChannels * samplesPerChannel;
    const std::int16_t* data = Load(&frame->muted, unreadable) ? ZeroedData : frame->data;
    return {numChannels, samplesPerChannel, size == 0 ? nullptr : data, size};
}

}
