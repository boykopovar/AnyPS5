/* Derived from api/video/encoded_image.cc, api/video/encoded_image.h, api/ref_count.h, rtc_base/ref_counted_object.h, rtc_base/ref_counter.h and rtc_base/buffer.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
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

#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::int32_t DroppedLastRef = 0;
constexpr std::int32_t OtherRefsRemained = 1;
constexpr std::size_t ReleaseSlot = 1;

using ReleaseFunction = std::int32_t(APS5_VABI*)(const void*);

void DestroyBuffer(WebRtcEncodedImageBuffer* buffer) {
    if (buffer->buffer.data)
        _ZdaPv_nid_postfix(buffer->buffer.data);
}

void APS5_VABI AddRefBuffer_nid_no_patch(const WebRtcEncodedImageBuffer* buffer) {
    const_cast<WebRtcEncodedImageBuffer*>(buffer)->refCount.fetch_add(1, std::memory_order_relaxed);
}

std::int32_t APS5_VABI ReleaseBuffer_nid_no_patch(const WebRtcEncodedImageBuffer* buffer) {
    auto* mutableBuffer = const_cast<WebRtcEncodedImageBuffer*>(buffer);
    if (mutableBuffer->refCount.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return OtherRefsRemained;
    DestroyBuffer(mutableBuffer);
    _ZdlPv_nid_postfix(mutableBuffer);
    return DroppedLastRef;
}

void APS5_VABI DestroyBuffer_nid_no_patch(WebRtcEncodedImageBuffer* buffer) {
    DestroyBuffer(buffer);
}

void APS5_VABI DeleteBuffer_nid_no_patch(WebRtcEncodedImageBuffer* buffer) {
    DestroyBuffer(buffer);
    _ZdlPv_nid_postfix(buffer);
}

const std::uint8_t* APS5_VABI BufferData_nid_no_patch(const WebRtcEncodedImageBuffer* buffer) {
    return buffer->buffer.data;
}

std::size_t APS5_VABI BufferSize_nid_no_patch(const WebRtcEncodedImageBuffer* buffer) {
    return buffer->buffer.size;
}

bool APS5_VABI BufferHasOneRef_nid_no_patch(const WebRtcEncodedImageBuffer* buffer) {
    return buffer->refCount.load(std::memory_order_acquire) == 1;
}

void* const BufferVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&AddRefBuffer_nid_no_patch), reinterpret_cast<void*>(&ReleaseBuffer_nid_no_patch),
    reinterpret_cast<void*>(&DestroyBuffer_nid_no_patch), reinterpret_cast<void*>(&DeleteBuffer_nid_no_patch),
    reinterpret_cast<void*>(&BufferData_nid_no_patch), reinterpret_cast<void*>(&BufferData_nid_no_patch),
    reinterpret_cast<void*>(&BufferSize_nid_no_patch), reinterpret_cast<void*>(&BufferHasOneRef_nid_no_patch),
};

template<typename TValue>
std::size_t Count(TValue* begin, TValue* end, TValue* capacity, const char* message) {
    const auto first = reinterpret_cast<std::uintptr_t>(begin);
    const auto last = reinterpret_cast<std::uintptr_t>(end);
    const auto limit = reinterpret_cast<std::uintptr_t>(capacity);
    if (last < first || limit < last || (!first && limit) || first % alignof(TValue) ||
        (last - first) % sizeof(TValue) || (limit - first) % sizeof(TValue))
        throw std::invalid_argument(message);
    return (last - first) / sizeof(TValue);
}

void ReleasePacketInfos(WebRtcRtpPacketInfosData* data) {
    if (!data || data->refCount.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
    const auto count = Count(data->begin, data->end, data->capacity, "EncodedImage destructor requires a valid packet info vector");
    for (std::size_t i = 0; i < count; ++i) {
        auto& entry = data->begin[i];
        Count(entry.csrcsBegin, entry.csrcsEnd, entry.csrcsCapacity, "EncodedImage destructor requires a valid CSRC vector");
    }
    for (std::size_t i = count; i > 0; --i)
        if (data->begin[i - 1].csrcsBegin)
            _ZdlPv_nid_postfix(data->begin[i - 1].csrcsBegin);
    if (data->begin)
        _ZdlPv_nid_postfix(data->begin);
    _ZdlPv_nid_postfix(data);
}

WebRtcEncodedImageMapNode* EndNode(WebRtcEncodedImageMap& map) {
    return reinterpret_cast<WebRtcEncodedImageMapNode*>(&map.root);
}

std::vector<WebRtcEncodedImageMapNode*> CollectNodes(WebRtcEncodedImageMap& map) {
    std::vector<WebRtcEncodedImageMapNode*> nodes;
    std::vector<WebRtcEncodedImageMapNode*> pending;
    if (map.root)
        pending.push_back(map.root);
    while (!pending.empty()) {
        auto* node = pending.back();
        pending.pop_back();
        if (nodes.size() >= map.size)
            throw std::invalid_argument("EncodedImage destructor requires a valid spatial layer size map");
        nodes.push_back(node);
        if (node->links.right)
            pending.push_back(node->links.right);
        if (node->links.left)
            pending.push_back(node->links.left);
    }
    if (nodes.size() != map.size || (map.size == 0 && map.beginNode != EndNode(map)))
        throw std::invalid_argument("EncodedImage destructor requires a valid spatial layer size map");
    return nodes;
}

void ReleaseEncodedData(void* buffer) {
    if (!buffer)
        return;
    const void* const* vtable;
    if (!GuestCopyIn_nid_no_patch(&vtable, buffer, sizeof(vtable)) || !vtable || !vtable[ReleaseSlot])
        throw std::invalid_argument("EncodedImage destructor requires a buffer virtual function");
    reinterpret_cast<ReleaseFunction>(const_cast<void*>(vtable[ReleaseSlot]))(buffer);
}

}

extern "C" {

WebRtcEncodedImageBuffer** APS5_VABI _ZN6webrtc18EncodedImageBuffer6CreateEPKhm_nid_postfix(WebRtcEncodedImageBuffer** result, const std::uint8_t* data, std::size_t size) {
    if (!result)
        throw std::invalid_argument("EncodedImageBuffer::Create requires a result");
    if (!data && size > 0)
        throw std::invalid_argument("EncodedImageBuffer::Create requires data");
    auto* storage = _Znwm_nid_postfix(sizeof(WebRtcEncodedImageBuffer));
    std::uint8_t* bytes = nullptr;
    try {
        if (size > 0)
            bytes = static_cast<std::uint8_t*>(_Znam_nid_postfix(size));
    } catch (...) {
        _ZdlPv_nid_postfix(storage);
        throw;
    }
    if (size > 0)
        std::memcpy(bytes, data, size);
    *result = new (storage) WebRtcEncodedImageBuffer{BufferVtable + 2, {size, size, bytes}, 1};
    return result;
}

void APS5_VABI _ZN6webrtc12EncodedImageC1Ev_nid_postfix(WebRtcEncodedImage* image) {
    if (!image)
        throw std::invalid_argument("EncodedImage requires an object");
    std::memset(image, 0, sizeof(WebRtcEncodedImage));
    image->frameType = 4;
    image->qp = -1;
    image->timing.flags = 0xff;
    image->spatialLayerFrameSizeBytes.beginNode = EndNode(image->spatialLayerFrameSizeBytes);
    image->retransmissionAllowed = true;
}

void APS5_VABI _ZN6webrtc12EncodedImageD1Ev_nid_postfix(WebRtcEncodedImage* image) {
    if (!image)
        throw std::invalid_argument("EncodedImage destructor requires an object");
    const auto nodes = CollectNodes(image->spatialLayerFrameSizeBytes);
    ReleasePacketInfos(image->packetInfos);
    for (auto* node : nodes)
        _ZdlPv_nid_postfix(node);
    ReleaseEncodedData(image->encodedData);
}

}
