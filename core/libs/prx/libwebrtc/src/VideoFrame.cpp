/* Derived from api/video/video_frame.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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

#include <cerrno>
#include <cstdint>
#include <stdexcept>

namespace {

using BufferReference = void(APS5_VABI*)(const void*);

constexpr std::size_t AddRefSlot = 0;
constexpr std::size_t ReleaseSlot = 1;
constexpr std::int64_t MicrosecondsPerMillisecond = 1000;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void CallBuffer(void* buffer, std::size_t slot, const char* function) {
    const void* const* vtable = nullptr;
    const void* entry = nullptr;
    {
        ErrnoGuard guard;
        if (!GuestCopyIn_nid_no_patch(&vtable, buffer, sizeof(vtable)) || !vtable ||
            !GuestCopyIn_nid_no_patch(&entry, vtable + slot, sizeof(entry)) || !entry)
            throw std::invalid_argument(function);
    }
    reinterpret_cast<BufferReference>(const_cast<void*>(entry))(buffer);
}

}

extern "C" {

void APS5_VABI _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(
    WebRtcVideoFrame* frame, void* const* buffer, std::uint32_t timestampRtp, std::int64_t renderTimeMs, std::int32_t rotation) {
    if (!frame)
        throw std::invalid_argument("VideoFrame constructor requires a frame");
    if (!buffer)
        throw std::invalid_argument("VideoFrame constructor requires a buffer reference");
    void* frameBuffer = nullptr;
    WebRtcVideoFrame hostFrame{};
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(frame, sizeof(*frame)) ||
            !GuestPtr<const WebRtcVideoFrame>(frame).CopyIn(hostFrame))
            throw std::invalid_argument("VideoFrame constructor requires a writable frame");
        if (!GuestPtr<void* const>(buffer).CopyIn(frameBuffer))
            throw std::invalid_argument("VideoFrame constructor requires a readable buffer reference");
    }
    if (frameBuffer)
        CallBuffer(frameBuffer, AddRefSlot, "VideoFrame constructor requires a buffer with AddRef");
    hostFrame.videoFrameBuffer = frameBuffer;
    hostFrame.timestampRtp = timestampRtp;
    hostFrame.ntpTimeMs = 0;
    hostFrame.timestampUs = static_cast<std::int64_t>(static_cast<std::uint64_t>(renderTimeMs) * MicrosecondsPerMillisecond);
    reinterpret_cast<unsigned char*>(&hostFrame.presentationTimestamp.value)[0] = 0;
    hostFrame.presentationTimestamp.engaged = false;
    reinterpret_cast<unsigned char*>(&hostFrame.referenceTime.value)[0] = 0;
    hostFrame.referenceTime.engaged = false;
    hostFrame.rotation = rotation;
    hostFrame.colorSpace[0] = 0;
    hostFrame.colorSpace[68] = 0;
    hostFrame.useLowLatencyRendering = false;
    reinterpret_cast<unsigned char*>(&hostFrame.maxCompositionDelayInFrames)[0] = 0;
    hostFrame.hasMaxCompositionDelayInFrames = false;
    reinterpret_cast<unsigned char*>(hostFrame.updateRect)[0] = 0;
    hostFrame.hasUpdateRect = false;
    hostFrame.packetInfos = nullptr;
    reinterpret_cast<unsigned char*>(hostFrame.processingTime)[0] = 0;
    hostFrame.hasProcessingTime = false;
    ErrnoGuard guard;
    if (!GuestPtr<WebRtcVideoFrame>(frame).CopyOut(hostFrame))
        throw std::invalid_argument("VideoFrame constructor requires a writable frame");
}

void APS5_VABI _ZN6webrtc10VideoFrameD1Ev_nid_postfix(WebRtcVideoFrame* frame) {
    if (!frame)
        throw std::invalid_argument("VideoFrame destructor requires a frame");
    WebRtcVideoFrame hostFrame{};
    {
        ErrnoGuard guard;
        if (!GuestPtr<const WebRtcVideoFrame>(frame).CopyIn(hostFrame))
            throw std::invalid_argument("VideoFrame destructor requires a readable frame");
    }
    if (hostFrame.packetInfos)
        NotImplemented_nid_no_patch("VideoFrame destructor with packet infos: releasing the guest packet info vector");
    if (hostFrame.videoFrameBuffer)
        CallBuffer(hostFrame.videoFrameBuffer, ReleaseSlot, "VideoFrame destructor requires a buffer with Release");
}

}
