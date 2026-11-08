/* Derived from api/video/video_frame_buffer.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstddef>
#include <stdexcept>

namespace {

constexpr std::size_t AddRefSlot = 0;
constexpr std::size_t TypeSlot = 4;
constexpr std::int32_t NativeType = 0;

const void* Slot(const WebRtcVideoFrameBuffer* buffer, std::size_t slot, const char* function) {
    if (!buffer || !buffer->vtable)
        throw std::invalid_argument(function);
    const void* const entry = static_cast<const void* const*>(buffer->vtable)[slot];
    if (!entry)
        throw std::invalid_argument(function);
    return entry;
}

}

extern "C" {

void** APS5_VABI _ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix(void** result, WebRtcVideoFrameBuffer* buffer) {
    if (!result)
        throw std::invalid_argument("I420BufferInterface::ToI420 requires a result");
    if (buffer) {
        using AddRef = void(APS5_VABI*)(const WebRtcVideoFrameBuffer*);
        reinterpret_cast<AddRef>(const_cast<void*>(Slot(buffer, AddRefSlot, "I420BufferInterface::ToI420 requires a buffer with AddRef")))(buffer);
    }
    *result = buffer;
    return result;
}

const void* APS5_VABI _ZNK6webrtc19I420BufferInterface7GetI420Ev_nid_postfix(const WebRtcVideoFrameBuffer* buffer) {
    return buffer;
}

void** APS5_VABI _ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix(void** result, WebRtcVideoFrameBuffer* buffer, WebRtcVideoFrameBufferTypes) {
    if (!result)
        throw std::invalid_argument("VideoFrameBuffer::GetMappedFrameBuffer requires a result");
    using GetType = std::int32_t(APS5_VABI*)(const WebRtcVideoFrameBuffer*);
    const std::int32_t type = reinterpret_cast<GetType>(const_cast<void*>(Slot(buffer, TypeSlot, "VideoFrameBuffer::GetMappedFrameBuffer requires a buffer with type")))(buffer);
    if (type != NativeType)
        throw std::runtime_error("VideoFrameBuffer::GetMappedFrameBuffer requires a native buffer");
    *result = nullptr;
    return result;
}

}
