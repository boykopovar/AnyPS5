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

#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

const std::int16_t ZeroedData[WebRtcAudioFrameMaxDataSizeSamples] = {};

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
    return frame->muted;
}

const std::int16_t* APS5_VABI _ZNK6webrtc10AudioFrame4dataEv_nid_postfix(const WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::data requires a frame");
    return frame->muted ? ZeroedData : frame->data;
}

std::int16_t* APS5_VABI _ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(WebRtcAudioFrame* frame) {
    if (!frame)
        throw std::invalid_argument("AudioFrame::mutable_data requires a frame");
    if (frame->muted) {
        std::memset(frame->data, 0, sizeof(frame->data));
        frame->muted = false;
    }
    return frame->data;
}

}
