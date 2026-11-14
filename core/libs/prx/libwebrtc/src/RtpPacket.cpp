/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t FixedHeaderSize = 12;
constexpr std::uint8_t RtpVersion = 2;
constexpr std::uint16_t OneByteExtensionProfileId = 0xBEDE;
constexpr std::uint16_t TwoByteExtensionProfileId = 0x1000;
constexpr std::uint16_t TwoByteExtensionProfileIdAppBitsFilter = 0xfff0;
constexpr std::size_t OneByteExtensionHeaderLength = 1;
constexpr std::size_t TwoByteExtensionHeaderLength = 2;

std::uint32_t ReadBigEndian(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < size; ++index)
        value = (value << 8) | bytes[index];
    return value;
}

std::size_t Capacity(const WebRtcCopyOnWriteBuffer& buffer) {
    return buffer.buffer ? buffer.buffer->capacity - buffer.offset : 0;
}

bool HasOneRef(const WebRtcRefCountedBuffer* backing) {
    return backing->refCount.load(std::memory_order_acquire) == 1;
}

void ReplaceBuffer(WebRtcCopyOnWriteBuffer& buffer, const std::uint8_t* data, std::size_t size, std::size_t capacity) {
    WebRtcCopyOnWriteBuffer replacement;
    _ZN3rtc17CopyOnWriteBufferC1Emm_nid_postfix(&replacement, size, capacity);
    std::memcpy(replacement.buffer->data, data, size);
    _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(&buffer);
    buffer.buffer = replacement.buffer;
}

void SetBackingData(WebRtcRefCountedBuffer* backing, const std::uint8_t* data, std::size_t size) {
    if (size > backing->capacity) {
        const std::size_t capacity = std::max(size, backing->capacity + backing->capacity / 2);
        auto* grown = static_cast<std::uint8_t*>(_Znam_nid_postfix(capacity));
        std::memcpy(grown, data, size);
        _ZdaPv_nid_postfix(backing->data);
        backing->data = grown;
        backing->capacity = capacity;
    } else {
        std::memmove(backing->data, data, size);
    }
    backing->size = size;
}

void SetData(WebRtcCopyOnWriteBuffer& buffer, const std::uint8_t* data, std::size_t size) {
    if (!buffer.buffer)
        ReplaceBuffer(buffer, data, size, size);
    else if (!HasOneRef(buffer.buffer))
        ReplaceBuffer(buffer, data, size, Capacity(buffer));
    else
        SetBackingData(buffer.buffer, data, size);
    buffer.offset = 0;
    buffer.size = size;
}

WebRtcRtpExtensionInfo& FindOrCreateExtensionInfo(WebRtcRtpExtensionInfoVector& entries, int id) {
    for (auto* entry = entries.begin; entry != entries.end; ++entry) {
        if (entry->id == id)
            return *entry;
    }
    const std::size_t count = entries.end - entries.begin;
    if (entries.end == entries.capacityEnd) {
        const std::size_t capacity = std::max<std::size_t>(count + 1, 2 * count);
        auto* grown = static_cast<WebRtcRtpExtensionInfo*>(_Znwm_nid_postfix(capacity * sizeof(WebRtcRtpExtensionInfo)));
        if (count != 0)
            std::memcpy(grown, entries.begin, count * sizeof(WebRtcRtpExtensionInfo));
        if (entries.begin)
            _ZdlPv_nid_postfix(entries.begin);
        entries.begin = grown;
        entries.end = grown + count;
        entries.capacityEnd = grown + capacity;
    }
    *entries.end = {static_cast<std::uint8_t>(id), 0, 0};
    return *entries.end++;
}

void Clear(WebRtcRtpPacket* packet) {
    if (!packet->buffer.buffer)
        throw std::invalid_argument("RtpPacket::Parse cannot clear a packet without a buffer");
    if (Capacity(packet->buffer) < FixedHeaderSize)
        throw std::invalid_argument("RtpPacket::Parse cannot clear a buffer smaller than the fixed header");
    packet->marker = false;
    packet->payloadType = 0;
    packet->sequenceNumber = 0;
    packet->timestamp = 0;
    packet->ssrc = 0;
    packet->payloadOffset = FixedHeaderSize;
    packet->payloadSize = 0;
    packet->paddingSize = 0;
    packet->extensionsSize = 0;
    packet->extensionEntries.end = packet->extensionEntries.begin;
    auto& buffer = packet->buffer;
    _ZN3rtc17CopyOnWriteBuffer24UnshareAndEnsureCapacityEm_nid_postfix(&buffer, Capacity(buffer));
    std::uint8_t* data = buffer.buffer->data + buffer.offset;
    std::memset(data, 0, FixedHeaderSize);
    if (FixedHeaderSize > buffer.size)
        buffer.buffer->size = FixedHeaderSize + buffer.offset;
    buffer.size = FixedHeaderSize;
    data[0] = RtpVersion << 6;
}

bool ParseExtensions(WebRtcRtpPacket* packet, const std::uint8_t* buffer, std::size_t size) {
    const std::size_t extensionOffset = packet->payloadOffset + 4;
    if (extensionOffset > size)
        return false;
    const auto profile = static_cast<std::uint16_t>(ReadBigEndian(buffer + packet->payloadOffset, 2));
    const std::size_t extensionsCapacity = ReadBigEndian(buffer + packet->payloadOffset + 2, 2) * std::size_t {4};
    if (extensionOffset + extensionsCapacity > size)
        return false;
    if (profile == OneByteExtensionProfileId || (profile & TwoByteExtensionProfileIdAppBitsFilter) == TwoByteExtensionProfileId) {
        const std::size_t headerLength = profile == OneByteExtensionProfileId ? OneByteExtensionHeaderLength : TwoByteExtensionHeaderLength;
        constexpr std::uint8_t PaddingByte = 0;
        constexpr int PaddingId = 0;
        constexpr int OneByteHeaderExtensionReservedId = 15;
        auto& extensionsSize = packet->extensionsSize;
        while (extensionsSize + headerLength < extensionsCapacity) {
            const std::uint8_t* header = buffer + extensionOffset + extensionsSize;
            if (header[0] == PaddingByte) {
                ++extensionsSize;
                continue;
            }
            int id;
            std::uint8_t length;
            if (profile == OneByteExtensionProfileId) {
                id = header[0] >> 4;
                length = 1 + (header[0] & 0xf);
                if (id == OneByteHeaderExtensionReservedId || (id == PaddingId && length != 1))
                    break;
            } else {
                id = header[0];
                length = header[1];
            }
            if (extensionsSize + headerLength + length > extensionsCapacity)
                break;
            auto& info = FindOrCreateExtensionInfo(packet->extensionEntries, id);
            const std::size_t offset = extensionOffset + extensionsSize + headerLength;
            if (offset > std::numeric_limits<std::uint16_t>::max())
                break;
            info.offset = static_cast<std::uint16_t>(offset);
            info.length = length;
            extensionsSize += headerLength + length;
        }
    }
    packet->payloadOffset = extensionOffset + extensionsCapacity;
    return true;
}

bool ParseBuffer(WebRtcRtpPacket* packet, const std::uint8_t* buffer, std::size_t size) {
    if (size < FixedHeaderSize)
        return false;
    const std::uint8_t version = buffer[0] >> 6;
    if (version != RtpVersion)
        return false;
    const bool hasPadding = (buffer[0] & 0x20) != 0;
    const bool hasExtension = (buffer[0] & 0x10) != 0;
    const std::uint8_t numberOfCsrcs = buffer[0] & 0x0f;
    packet->marker = (buffer[1] & 0x80) != 0;
    packet->payloadType = buffer[1] & 0x7f;
    packet->sequenceNumber = static_cast<std::uint16_t>(ReadBigEndian(buffer + 2, 2));
    packet->timestamp = ReadBigEndian(buffer + 4, 4);
    packet->ssrc = ReadBigEndian(buffer + 8, 4);
    if (size < FixedHeaderSize + numberOfCsrcs * 4)
        return false;
    packet->payloadOffset = FixedHeaderSize + numberOfCsrcs * 4;
    packet->extensionsSize = 0;
    packet->extensionEntries.end = packet->extensionEntries.begin;
    if (hasExtension && !ParseExtensions(packet, buffer, size))
        return false;
    if (hasPadding && packet->payloadOffset < size) {
        packet->paddingSize = buffer[size - 1];
        if (packet->paddingSize == 0)
            return false;
    } else {
        packet->paddingSize = 0;
    }
    if (packet->payloadOffset + packet->paddingSize > size)
        return false;
    packet->payloadSize = size - packet->payloadOffset - packet->paddingSize;
    return true;
}

}

extern "C" {

bool APS5_VABI _ZN6webrtc9RtpPacket5ParseEPKhm_nid_postfix(WebRtcRtpPacket* packet, const std::uint8_t* buffer, std::size_t size) {
    if (!packet)
        throw std::invalid_argument("RtpPacket::Parse requires a packet object");
    if (!buffer && size >= FixedHeaderSize)
        throw std::invalid_argument("RtpPacket::Parse requires a buffer");
    if (!ParseBuffer(packet, buffer, size)) {
        Clear(packet);
        return false;
    }
    SetData(packet->buffer, buffer, size);
    return true;
}

}
