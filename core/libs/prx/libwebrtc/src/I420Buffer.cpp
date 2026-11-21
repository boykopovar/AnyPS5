/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

using PlaneGetter = const std::uint8_t* (APS5_VABI*)(const WebRtcI420Buffer*);

constexpr std::size_t DataYSlot = 17;
constexpr std::size_t DataUSlot = 18;
constexpr std::size_t DataVSlot = 19;

std::uint8_t* Plane(WebRtcI420Buffer* buffer, std::size_t slot, const char* function) {
    if (!buffer || !buffer->vtable)
        throw std::invalid_argument(function);
    const auto getter = reinterpret_cast<PlaneGetter>(const_cast<void*>(static_cast<const void* const*>(buffer->vtable)[slot]));
    return const_cast<std::uint8_t*>(getter(buffer));
}

constexpr std::size_t BufferAlignment = 64;
constexpr std::int32_t I420Type = 1;
constexpr std::int32_t DroppedLastRef = 0;
constexpr std::int32_t OtherRefsRemained = 1;

std::int32_t I420DataSize(std::int32_t width, std::int32_t height, std::int32_t strideY, std::int32_t strideU, std::int32_t strideV) {
    if (width <= 0 || height <= 0 || strideY < width || strideU <= 0 || strideV <= 0)
        throw std::runtime_error("I420Buffer::Create requires positive dimensions and strides with stride_y >= width");
    const std::int64_t h = height;
    const std::int64_t size = strideY * h + (static_cast<std::int64_t>(strideU) + strideV) * ((h + 1) / 2);
    if (size > std::numeric_limits<std::int32_t>::max())
        throw std::runtime_error("I420Buffer::Create requires a data size that fits in int");
    return static_cast<std::int32_t>(size);
}

std::uint8_t* AlignedMalloc(std::size_t size) {
    auto* memory = static_cast<std::uint8_t*>(malloc_nid_postfix(size + sizeof(std::uintptr_t) + BufferAlignment - 1));
    if (!memory)
        throw std::runtime_error("I420Buffer::Create couldn't allocate memory in AlignedMalloc");
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(memory);
    const std::uintptr_t aligned = (start + sizeof(std::uintptr_t) + BufferAlignment - 1) & ~(BufferAlignment - 1);
    std::memcpy(reinterpret_cast<void*>(aligned - sizeof(std::uintptr_t)), &start, sizeof(start));
    return reinterpret_cast<std::uint8_t*>(aligned);
}

void AlignedFree(std::uint8_t* block) {
    if (!block)
        return;
    std::uintptr_t start;
    std::memcpy(&start, block - sizeof(std::uintptr_t), sizeof(start));
    free_nid_postfix(reinterpret_cast<void*>(start));
}

WebRtcRefCountedI420Buffer* Mutable(const WebRtcRefCountedI420Buffer* buffer) {
    return const_cast<WebRtcRefCountedI420Buffer*>(buffer);
}

void APS5_VABI AddRef_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    Mutable(buffer)->refCount.fetch_add(1, std::memory_order_relaxed);
}

void APS5_VABI Destroy_nid_no_patch(WebRtcRefCountedI420Buffer* buffer) {
    AlignedFree(buffer->buffer.data);
}

void APS5_VABI Delete_nid_no_patch(WebRtcRefCountedI420Buffer* buffer) {
    Destroy_nid_no_patch(buffer);
    _ZdlPv_nid_postfix(buffer);
}

std::int32_t APS5_VABI Release_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    if (Mutable(buffer)->refCount.fetch_sub(1, std::memory_order_acq_rel) - 1 != 0)
        return OtherRefsRemained;
    Delete_nid_no_patch(Mutable(buffer));
    return DroppedLastRef;
}

std::int32_t APS5_VABI Type_nid_no_patch(const WebRtcRefCountedI420Buffer*) {
    return I420Type;
}

std::int32_t APS5_VABI Width_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    return buffer->buffer.width;
}

std::int32_t APS5_VABI Height_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    return buffer->buffer.height;
}

WebRtcGuestString* APS5_VABI StorageRepresentation_nid_no_patch(WebRtcGuestString* result, const WebRtcRefCountedI420Buffer*) {
    result->reserved = 0;
    std::memcpy(result->buffer, "?", 2);
    result->size = 1;
    result->capacity = WebRtcGuestStringInlineCapacity;
    return result;
}

std::int32_t APS5_VABI StrideY_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    return buffer->buffer.strideY;
}

std::int32_t APS5_VABI StrideU_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    return buffer->buffer.strideU;
}

std::int32_t APS5_VABI StrideV_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    return buffer->buffer.strideV;
}

const std::uint8_t* APS5_VABI DataY_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    return buffer->buffer.data;
}

const std::uint8_t* APS5_VABI DataU_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    return buffer->buffer.data + buffer->buffer.strideY * buffer->buffer.height;
}

const std::uint8_t* APS5_VABI DataV_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    const auto& fields = buffer->buffer;
    return fields.data + fields.strideY * fields.height + fields.strideU * ((fields.height + 1) / 2);
}

bool APS5_VABI HasOneRef_nid_no_patch(const WebRtcRefCountedI420Buffer* buffer) {
    return buffer->refCount.load(std::memory_order_acquire) == 1;
}

void* const RefCountedI420BufferVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&AddRef_nid_no_patch), reinterpret_cast<void*>(&Release_nid_no_patch),
    reinterpret_cast<void*>(&Destroy_nid_no_patch), reinterpret_cast<void*>(&Delete_nid_no_patch),
    reinterpret_cast<void*>(&Type_nid_no_patch),
    reinterpret_cast<void*>(&Width_nid_no_patch), reinterpret_cast<void*>(&Height_nid_no_patch),
    reinterpret_cast<void*>(&_ZN6webrtc19I420BufferInterface6ToI420Ev_nid_postfix),
    reinterpret_cast<void*>(&_ZNK6webrtc19I420BufferInterface7GetI420Ev_nid_postfix),
    reinterpret_cast<void*>(&_ZN6webrtc16VideoFrameBuffer12CropAndScaleEiiiiii_nid_postfix),
    reinterpret_cast<void*>(&_ZN6webrtc16VideoFrameBuffer20GetMappedFrameBufferEN3rtc9ArrayViewINS0_4TypeELln4711EEE_nid_postfix),
    reinterpret_cast<void*>(&StorageRepresentation_nid_no_patch),
    reinterpret_cast<void*>(&_ZNK6webrtc19I420BufferInterface11ChromaWidthEv_nid_postfix),
    reinterpret_cast<void*>(&_ZNK6webrtc19I420BufferInterface12ChromaHeightEv_nid_postfix),
    reinterpret_cast<void*>(&StrideY_nid_no_patch), reinterpret_cast<void*>(&StrideU_nid_no_patch), reinterpret_cast<void*>(&StrideV_nid_no_patch),
    reinterpret_cast<void*>(&DataY_nid_no_patch), reinterpret_cast<void*>(&DataU_nid_no_patch), reinterpret_cast<void*>(&DataV_nid_no_patch),
    reinterpret_cast<void*>(&HasOneRef_nid_no_patch),
};

static_assert(sizeof(RefCountedI420BufferVtable) / sizeof(void*) == 23);

}

extern "C" {

WebRtcRefCountedI420Buffer** APS5_VABI _ZN6webrtc10I420Buffer6CreateEiiiii_nid_postfix(WebRtcRefCountedI420Buffer** result, int width, int height, int strideY, int strideU, int strideV) {
    if (!result)
        throw std::invalid_argument("I420Buffer::Create requires a result");
    auto* storage = _Znwm_nid_postfix(sizeof(WebRtcRefCountedI420Buffer));
    std::uint8_t* data;
    try {
        data = AlignedMalloc(static_cast<std::size_t>(I420DataSize(width, height, strideY, strideU, strideV)));
    } catch (...) {
        _ZdlPv_nid_postfix(storage);
        throw;
    }
    auto* buffer = new (storage) WebRtcRefCountedI420Buffer{{RefCountedI420BufferVtable + 2, width, height, strideY, strideU, strideV, data}, 1};
    *result = buffer;
    return result;
}

std::uint8_t* APS5_VABI _ZN6webrtc10I420Buffer12MutableDataYEv_nid_postfix(WebRtcI420Buffer* buffer) {
    return Plane(buffer, DataYSlot, "I420Buffer::MutableDataY requires a buffer with a vtable");
}

std::uint8_t* APS5_VABI _ZN6webrtc10I420Buffer12MutableDataUEv_nid_postfix(WebRtcI420Buffer* buffer) {
    return Plane(buffer, DataUSlot, "I420Buffer::MutableDataU requires a buffer with a vtable");
}

std::uint8_t* APS5_VABI _ZN6webrtc10I420Buffer12MutableDataVEv_nid_postfix(WebRtcI420Buffer* buffer) {
    return Plane(buffer, DataVSlot, "I420Buffer::MutableDataV requires a buffer with a vtable");
}

}
