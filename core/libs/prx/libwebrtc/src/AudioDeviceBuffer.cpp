/*
 * Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS. All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
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

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

WebRtcAudioDeviceBuffer& RequireBuffer(WebRtcAudioDeviceBuffer* buffer, const char* function) {
    if (!buffer)
        throw std::invalid_argument(std::string(function) + " requires an AudioDeviceBuffer");
    return *buffer;
}

}

extern "C" {

std::int32_t APS5_VABI _ZN6webrtc17AudioDeviceBuffer22SetRecordingSampleRateEj_nid_postfix(WebRtcAudioDeviceBuffer* buffer, std::uint32_t sampleRateHz) {
    std::atomic_ref<std::uint32_t>(RequireBuffer(buffer, "AudioDeviceBuffer::SetRecordingSampleRate").recordingSampleRate).store(sampleRateHz);
    return 0;
}

std::int32_t APS5_VABI _ZN6webrtc17AudioDeviceBuffer20SetPlayoutSampleRateEj_nid_postfix(WebRtcAudioDeviceBuffer* buffer, std::uint32_t sampleRateHz) {
    std::atomic_ref<std::uint32_t>(RequireBuffer(buffer, "AudioDeviceBuffer::SetPlayoutSampleRate").playoutSampleRate).store(sampleRateHz);
    return 0;
}

std::int32_t APS5_VABI _ZN6webrtc17AudioDeviceBuffer20SetRecordingChannelsEm_nid_postfix(WebRtcAudioDeviceBuffer* buffer, std::uint64_t channels) {
    std::atomic_ref<std::uint64_t>(RequireBuffer(buffer, "AudioDeviceBuffer::SetRecordingChannels").recordingChannels).store(channels);
    return 0;
}

std::int32_t APS5_VABI _ZN6webrtc17AudioDeviceBuffer18SetPlayoutChannelsEm_nid_postfix(WebRtcAudioDeviceBuffer* buffer, std::uint64_t channels) {
    std::atomic_ref<std::uint64_t>(RequireBuffer(buffer, "AudioDeviceBuffer::SetPlayoutChannels").playoutChannels).store(channels);
    return 0;
}

std::int32_t APS5_VABI _ZN6webrtc17AudioDeviceBuffer21RegisterAudioCallbackEPNS_14AudioTransportE_nid_postfix(WebRtcAudioDeviceBuffer* buffer, WebRtcAudioTransport* callback) {
    WebRtcAudioDeviceBuffer& device = RequireBuffer(buffer, "AudioDeviceBuffer::RegisterAudioCallback");
    if (device.playing || device.recording)
        return -1;
    device.audioTransportCallback = callback;
    return 0;
}

std::int32_t APS5_VABI _ZN6webrtc17AudioDeviceBuffer12StartPlayoutEv_nid_postfix(WebRtcAudioDeviceBuffer* buffer) {
    WebRtcAudioDeviceBuffer& device = RequireBuffer(buffer, "AudioDeviceBuffer::StartPlayout");
    if (device.playing)
        return 0;
    device.playing = true;
    return 0;
}

std::int32_t APS5_VABI _ZN6webrtc17AudioDeviceBuffer11StopPlayoutEv_nid_postfix(WebRtcAudioDeviceBuffer* buffer) {
    WebRtcAudioDeviceBuffer& device = RequireBuffer(buffer, "AudioDeviceBuffer::StopPlayout");
    if (!device.playing)
        return 0;
    device.playing = false;
    return 0;
}

std::int32_t APS5_VABI _ZN6webrtc17AudioDeviceBuffer14StartRecordingEv_nid_postfix(WebRtcAudioDeviceBuffer* buffer) {
    WebRtcAudioDeviceBuffer& device = RequireBuffer(buffer, "AudioDeviceBuffer::StartRecording");
    if (device.recording)
        return 0;
    device.recording = true;
    return 0;
}

std::int32_t APS5_VABI _ZN6webrtc17AudioDeviceBuffer13StopRecordingEv_nid_postfix(WebRtcAudioDeviceBuffer* buffer) {
    WebRtcAudioDeviceBuffer& device = RequireBuffer(buffer, "AudioDeviceBuffer::StopRecording");
    if (!device.recording)
        return 0;
    device.recording = false;
    return 0;
}

}
