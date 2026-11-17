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
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::size_t AddRefSlot = 0;
constexpr std::size_t ReleaseSlot = 1;

void CallReference(void* buffer, std::size_t slot, const char* function) {
    const void* const* vtable;
    std::memcpy(&vtable, buffer, sizeof(vtable));
    if (!vtable || !vtable[slot])
        throw std::invalid_argument(function);
    using Reference = void(APS5_VABI*)(const void*);
    reinterpret_cast<Reference>(const_cast<void*>(vtable[slot]))(buffer);
}

WebRtcVideoFrameBuilder& Checked(WebRtcVideoFrameBuilder* builder, const char* function) {
    if (!builder)
        throw std::invalid_argument(function);
    return *builder;
}

using BufferRelease = int(APS5_VABI*)(const void*);

void ReleaseVideoFrameBuffer(void* buffer) {
    if (!buffer)
        return;
    const void* const* vtable;
    if (!GuestCopyIn_nid_no_patch(&vtable, buffer, sizeof(vtable)) || !vtable || !vtable[ReleaseSlot])
        throw std::invalid_argument("VideoFrame::Builder destructor requires a buffer virtual function");
    reinterpret_cast<BufferRelease>(const_cast<void*>(vtable[ReleaseSlot]))(buffer);
}

}

extern "C" {

void APS5_VABI _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(WebRtcVideoFrameBuilder* builder) {
    std::memset(&Checked(builder, "VideoFrame::Builder constructor requires a builder"), 0, sizeof(WebRtcVideoFrameBuilder));
}

void APS5_VABI _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(WebRtcVideoFrameBuilder* builder) {
    const WebRtcVideoFrameBuilder& checked = Checked(builder, "VideoFrame::Builder destructor requires a builder");
    if (checked.packetInfos)
        NotImplemented_nid_no_patch("VideoFrame::Builder destructor with packet infos: releasing the guest packet info vector");
    ReleaseVideoFrameBuffer(checked.videoFrameBuffer);
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(WebRtcVideoFrameBuilder* builder, std::int64_t timestampMs) {
    Checked(builder, "VideoFrame::Builder::set_timestamp_ms requires a builder").timestampUs =
        static_cast<std::int64_t>(static_cast<std::uint64_t>(timestampMs) * 1000u);
    return builder;
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder17set_timestamp_rtpEj_nid_postfix(WebRtcVideoFrameBuilder* builder, std::uint32_t timestampRtp) {
    Checked(builder, "VideoFrame::Builder::set_timestamp_rtp requires a builder").timestampRtp = timestampRtp;
    return builder;
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder15set_ntp_time_msEl_nid_postfix(WebRtcVideoFrameBuilder* builder, std::int64_t ntpTimeMs) {
    Checked(builder, "VideoFrame::Builder::set_ntp_time_ms requires a builder").ntpTimeMs = ntpTimeMs;
    return builder;
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(WebRtcVideoFrameBuilder* builder, std::uint16_t id) {
    Checked(builder, "VideoFrame::Builder::set_id requires a builder").id = id;
    return builder;
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(WebRtcVideoFrameBuilder* builder, void* const* buffer) {
    WebRtcVideoFrameBuilder& checked = Checked(builder, "VideoFrame::Builder::set_video_frame_buffer requires a builder");
    if (!buffer)
        throw std::invalid_argument("VideoFrame::Builder::set_video_frame_buffer requires a buffer reference");
    void* const replacement = *buffer;
    if (replacement)
        CallReference(replacement, AddRefSlot, "VideoFrame::Builder::set_video_frame_buffer requires a frame buffer with a virtual AddRef");
    if (checked.videoFrameBuffer)
        CallReference(checked.videoFrameBuffer, ReleaseSlot, "VideoFrame::Builder::set_video_frame_buffer requires a frame buffer with a virtual Release");
    checked.videoFrameBuffer = replacement;
    return builder;
}

void** APS5_VABI _ZNK6webrtc10VideoFrame18video_frame_bufferEv_nid_postfix(void** result, const WebRtcVideoFrame* frame) {
    if (!result)
        throw std::invalid_argument("VideoFrame::video_frame_buffer requires a result");
    if (!frame)
        throw std::invalid_argument("VideoFrame::video_frame_buffer requires a frame");
    if (frame->videoFrameBuffer)
        CallReference(frame->videoFrameBuffer, AddRefSlot, "VideoFrame::video_frame_buffer requires a frame buffer with a virtual AddRef");
    *result = frame->videoFrameBuffer;
    return result;
}

std::int64_t APS5_VABI _ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(const WebRtcVideoFrame* frame) {
    if (!frame)
        throw std::invalid_argument("VideoFrame::render_time_ms requires a frame");
    return frame->timestampUs / 1000;
}

}
