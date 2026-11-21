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
#include "prx/libc/include/GuestPtr.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::size_t AddRefSlot = 0;
constexpr std::size_t ReleaseSlot = 1;

const void* VirtualFunction(const void* object, std::size_t slot, const char* function) {
    const void* const* vtable = nullptr;
    const void* entry = nullptr;
    if (!GuestPtr<const void* const*>(object).CopyIn(vtable) || !vtable || !GuestPtr<const void*>(vtable + slot).CopyIn(entry) || !entry)
        throw std::invalid_argument(function);
    return entry;
}

void CallReference(void* buffer, std::size_t slot, const char* function) {
    using Reference = void(APS5_VABI*)(const void*);
    reinterpret_cast<Reference>(const_cast<void*>(VirtualFunction(buffer, slot, function)))(buffer);
}

template<typename TObject> void* Field(TObject* object, std::size_t offset) {
    return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(object) + offset);
}

template<typename TValue> TValue ReadField(const void* object, std::size_t offset, const char* function) {
    TValue value{};
    if (!object || !GuestPtr<TValue>(Field(object, offset)).CopyIn(value))
        throw std::invalid_argument(function);
    return value;
}

template<typename TValue> WebRtcVideoFrameBuilder* WriteField(WebRtcVideoFrameBuilder* builder, std::size_t offset, TValue value, const char* function) {
    if (!builder || !GuestPtr<TValue>(Field(builder, offset)).CopyOut(value))
        throw std::invalid_argument(function);
    return builder;
}

using BufferRelease = int(APS5_VABI*)(const void*);

void ReleaseVideoFrameBuffer(void* buffer) {
    if (!buffer)
        return;
    const void* release = VirtualFunction(buffer, ReleaseSlot, "VideoFrame::Builder destructor requires a buffer virtual function");
    reinterpret_cast<BufferRelease>(const_cast<void*>(release))(buffer);
}

}

extern "C" {

void APS5_VABI _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(WebRtcVideoFrameBuilder* builder) {
    WebRtcVideoFrameBuilder host;
    std::memset(static_cast<void*>(&host), 0, sizeof(host));
    if (!builder || !GuestPtr<WebRtcVideoFrameBuilder>(builder).CopyOut(host))
        throw std::invalid_argument("VideoFrame::Builder constructor requires a builder");
}

void APS5_VABI _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(WebRtcVideoFrameBuilder* builder) {
    WebRtcVideoFrameBuilder checked{};
    if (!GuestPtr<WebRtcVideoFrameBuilder>(builder).CopyIn(checked))
        throw std::invalid_argument("VideoFrame::Builder destructor requires a builder");
    if (checked.packetInfos)
        NotImplemented_nid_no_patch("VideoFrame::Builder destructor with packet infos: releasing the guest packet info vector");
    ReleaseVideoFrameBuffer(checked.videoFrameBuffer);
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(WebRtcVideoFrameBuilder* builder, std::int64_t timestampMs) {
    return WriteField(builder, offsetof(WebRtcVideoFrameBuilder, timestampUs), static_cast<std::int64_t>(static_cast<std::uint64_t>(timestampMs) * 1000u),
                      "VideoFrame::Builder::set_timestamp_ms requires a builder");
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder17set_timestamp_rtpEj_nid_postfix(WebRtcVideoFrameBuilder* builder, std::uint32_t timestampRtp) {
    return WriteField(builder, offsetof(WebRtcVideoFrameBuilder, timestampRtp), timestampRtp, "VideoFrame::Builder::set_timestamp_rtp requires a builder");
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder15set_ntp_time_msEl_nid_postfix(WebRtcVideoFrameBuilder* builder, std::int64_t ntpTimeMs) {
    return WriteField(builder, offsetof(WebRtcVideoFrameBuilder, ntpTimeMs), ntpTimeMs, "VideoFrame::Builder::set_ntp_time_ms requires a builder");
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(WebRtcVideoFrameBuilder* builder, std::uint16_t id) {
    return WriteField(builder, offsetof(WebRtcVideoFrameBuilder, id), id, "VideoFrame::Builder::set_id requires a builder");
}

WebRtcVideoFrameBuilder* APS5_VABI _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(WebRtcVideoFrameBuilder* builder, void* const* buffer) {
    constexpr const char* message = "VideoFrame::Builder::set_video_frame_buffer requires a builder";
    void* const current = ReadField<void*>(builder, offsetof(WebRtcVideoFrameBuilder, videoFrameBuffer), message);
    if (!GuestWritable_nid_no_patch(Field(builder, offsetof(WebRtcVideoFrameBuilder, videoFrameBuffer)), sizeof(void*)))
        throw std::invalid_argument(message);
    if (!buffer)
        throw std::invalid_argument("VideoFrame::Builder::set_video_frame_buffer requires a buffer reference");
    void* replacement = nullptr;
    if (!GuestPtr<void*>(buffer).CopyIn(replacement))
        throw std::invalid_argument("VideoFrame::Builder::set_video_frame_buffer requires a readable buffer reference");
    if (replacement)
        CallReference(replacement, AddRefSlot, "VideoFrame::Builder::set_video_frame_buffer requires a frame buffer with a virtual AddRef");
    if (current)
        CallReference(current, ReleaseSlot, "VideoFrame::Builder::set_video_frame_buffer requires a frame buffer with a virtual Release");
    return WriteField(builder, offsetof(WebRtcVideoFrameBuilder, videoFrameBuffer), replacement, message);
}

void** APS5_VABI _ZNK6webrtc10VideoFrame18video_frame_bufferEv_nid_postfix(void** result, const WebRtcVideoFrame* frame) {
    if (!result)
        throw std::invalid_argument("VideoFrame::video_frame_buffer requires a result");
    if (!GuestWritable_nid_no_patch(result, sizeof(*result)))
        throw std::invalid_argument("VideoFrame::video_frame_buffer requires a writable result");
    void* const buffer = ReadField<void*>(frame, offsetof(WebRtcVideoFrame, videoFrameBuffer), "VideoFrame::video_frame_buffer requires a frame");
    if (buffer)
        CallReference(buffer, AddRefSlot, "VideoFrame::video_frame_buffer requires a frame buffer with a virtual AddRef");
    if (!GuestPtr<void*>(result).CopyOut(buffer))
        throw std::invalid_argument("VideoFrame::video_frame_buffer requires a writable result");
    return result;
}

std::int64_t APS5_VABI _ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(const WebRtcVideoFrame* frame) {
    return ReadField<std::int64_t>(frame, offsetof(WebRtcVideoFrame, timestampUs), "VideoFrame::render_time_ms requires a frame") / 1000;
}

}
