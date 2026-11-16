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
#include "prx/libc/include/GuestPtr.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::int32_t DroppedLastRef = 0;
constexpr std::int32_t OtherRefsRemained = 1;
constexpr std::size_t ReleaseSlot = 1;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

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

bool ProbeReadableBytes(const std::uint8_t* source, std::size_t size) {
    if (size == 0)
        return true;
    if (!source)
        return false;
    std::array<std::uint8_t, 4096> scratch{};
    std::size_t offset = 0;
    ErrnoGuard guard;
    while (offset < size) {
        const std::size_t chunk = std::min(size - offset, scratch.size());
        if (!GuestBytes(source + offset).CopyIn(scratch.data(), chunk))
            return false;
        offset += chunk;
    }
    return true;
}

void ReleasePacketInfos(WebRtcRtpPacketInfosData* data) {
    if (!data)
        return;
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(data, sizeof(*data)))
            throw std::invalid_argument("EncodedImage destructor requires writable packet infos");
    }
    struct PacketInfosRange {
        WebRtcRtpPacketInfoEntry* begin;
        WebRtcRtpPacketInfoEntry* end;
        WebRtcRtpPacketInfoEntry* capacity;
    } range{};
    {
        ErrnoGuard guard;
        if (!GuestCopyIn_nid_no_patch(&range, reinterpret_cast<const std::uint8_t*>(data) + offsetof(WebRtcRtpPacketInfosData, begin), sizeof(range)))
            throw std::invalid_argument("EncodedImage destructor requires readable packet infos");
    }
    const auto count = Count(range.begin, range.end, range.capacity, "EncodedImage destructor requires a valid packet info vector");
    std::vector<WebRtcRtpPacketInfoEntry> entries(count);
    if (count > 0) {
        ErrnoGuard guard;
        if (!GuestPtr<const WebRtcRtpPacketInfoEntry>(range.begin).CopyIn(entries.data(), count))
            throw std::invalid_argument("EncodedImage destructor requires readable packet info entries");
    }
    for (std::size_t i = 0; i < count; ++i)
        Count(entries[i].csrcsBegin, entries[i].csrcsEnd, entries[i].csrcsCapacity, "EncodedImage destructor requires a valid CSRC vector");
    if (data->refCount.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
    for (std::size_t i = count; i > 0; --i)
        if (entries[i - 1].csrcsBegin)
            _ZdlPv_nid_postfix(entries[i - 1].csrcsBegin);
    if (range.begin)
        _ZdlPv_nid_postfix(range.begin);
    _ZdlPv_nid_postfix(data);
}

const WebRtcEncodedImageMapNode* EndNode(const WebRtcEncodedImage* image) {
    return reinterpret_cast<const WebRtcEncodedImageMapNode*>(
        reinterpret_cast<std::uintptr_t>(image) + offsetof(WebRtcEncodedImage, spatialLayerFrameSizeBytes) + offsetof(WebRtcEncodedImageMap, root));
}

std::vector<WebRtcEncodedImageMapNode*> CollectNodes(const WebRtcEncodedImageMap& map, const WebRtcEncodedImageMapNode* expectedEnd) {
    std::vector<WebRtcEncodedImageMapNode*> nodes;
    std::vector<WebRtcEncodedImageMapNode*> pending;
    if (map.root)
        pending.push_back(map.root);
    while (!pending.empty()) {
        auto* node = pending.back();
        pending.pop_back();
        if (nodes.size() >= map.size)
            throw std::invalid_argument("EncodedImage destructor requires a valid spatial layer size map");
        WebRtcEncodedImageMapNode nodeCopy{};
        {
            ErrnoGuard guard;
            if (!node || !GuestWritable_nid_no_patch(node, sizeof(*node)) ||
                !GuestPtr<const WebRtcEncodedImageMapNode>(node).CopyIn(nodeCopy))
                throw std::invalid_argument("EncodedImage destructor requires a valid spatial layer size map");
        }
        nodes.push_back(node);
        if (nodeCopy.links.right)
            pending.push_back(nodeCopy.links.right);
        if (nodeCopy.links.left)
            pending.push_back(nodeCopy.links.left);
    }
    if (nodes.size() != map.size || (map.size == 0 && map.beginNode != expectedEnd))
        throw std::invalid_argument("EncodedImage destructor requires a valid spatial layer size map");
    return nodes;
}

void ReleaseEncodedData(void* buffer) {
    if (!buffer)
        return;
    const void* const* vtable = nullptr;
    const void* releaseFn = nullptr;
    {
        ErrnoGuard guard;
        if (!GuestCopyIn_nid_no_patch(&vtable, buffer, sizeof(vtable)) || !vtable ||
            !GuestCopyIn_nid_no_patch(&releaseFn, vtable + ReleaseSlot, sizeof(releaseFn)) || !releaseFn)
            throw std::invalid_argument("EncodedImage destructor requires a buffer virtual function");
    }
    reinterpret_cast<ReleaseFunction>(const_cast<void*>(releaseFn))(buffer);
}

}

extern "C" {

WebRtcEncodedImageBuffer** APS5_VABI _ZN6webrtc18EncodedImageBuffer6CreateEPKhm_nid_postfix(WebRtcEncodedImageBuffer** result, const std::uint8_t* data, std::size_t size) {
    if (!result)
        throw std::invalid_argument("EncodedImageBuffer::Create requires a result");
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(result, sizeof(*result)))
            throw std::invalid_argument("EncodedImageBuffer::Create requires a writable result");
    }
    if (!ProbeReadableBytes(data, size))
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
    if (size > 0) {
        bool copied = false;
        {
            ErrnoGuard guard;
            copied = GuestBytes(data).CopyIn(bytes, size);
        }
        if (!copied) {
            _ZdaPv_nid_postfix(bytes);
            _ZdlPv_nid_postfix(storage);
            throw std::invalid_argument("EncodedImageBuffer::Create requires readable data");
        }
    }
    auto* buffer = new (storage) WebRtcEncodedImageBuffer{BufferVtable + 2, {size, size, bytes}, 1};
    bool written = false;
    {
        ErrnoGuard guard;
        written = GuestPtr<WebRtcEncodedImageBuffer*>(result).CopyOut(buffer);
    }
    if (!written) {
        if (bytes)
            _ZdaPv_nid_postfix(bytes);
        _ZdlPv_nid_postfix(storage);
        throw std::invalid_argument("EncodedImageBuffer::Create requires a writable result");
    }
    return result;
}

void APS5_VABI _ZN6webrtc12EncodedImageC1Ev_nid_postfix(WebRtcEncodedImage* image) {
    if (!image)
        throw std::invalid_argument("EncodedImage requires an object");
    WebRtcEncodedImage host{};
    host.frameType = 4;
    host.qp = -1;
    host.timing.flags = 0xff;
    host.spatialLayerFrameSizeBytes.beginNode = const_cast<WebRtcEncodedImageMapNode*>(EndNode(image));
    host.retransmissionAllowed = true;
    ErrnoGuard guard;
    if (!GuestPtr<WebRtcEncodedImage>(image).CopyOut(host))
        throw std::invalid_argument("EncodedImage requires a writable object");
}

void APS5_VABI _ZN6webrtc12EncodedImageD1Ev_nid_postfix(WebRtcEncodedImage* image) {
    if (!image)
        throw std::invalid_argument("EncodedImage destructor requires an object");
    WebRtcEncodedImage host{};
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(image, sizeof(*image)) ||
            !GuestPtr<const WebRtcEncodedImage>(image).CopyIn(host))
            throw std::invalid_argument("EncodedImage destructor requires a writable object");
    }
    const auto nodes = CollectNodes(host.spatialLayerFrameSizeBytes, EndNode(image));
    ReleasePacketInfos(host.packetInfos);
    for (auto* node : nodes)
        _ZdlPv_nid_postfix(node);
    ReleaseEncodedData(host.encodedData);
}

}
