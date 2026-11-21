/* Derived from modules/rtp_rtcp/source/rtp_packet_received.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/rtp_rtcp/source/rtp_packet.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::size_t FixedHeaderSize = 12;
constexpr std::size_t DefaultPacketSize = 1500;
constexpr std::uint8_t RtpVersion = 2;
constexpr std::size_t DeletingDestructorSlot = 1;
constexpr std::size_t RefCountOffset = 8;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

using DeletingDestructor = void(APS5_VABI*)(void*);

std::atomic_ref<std::int32_t> RefCount(void* object) {
    return std::atomic_ref<std::int32_t>(*reinterpret_cast<std::int32_t*>(static_cast<std::uint8_t*>(object) + RefCountOffset));
}

}

extern "C" {

void APS5_VABI _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(
    WebRtcRtpPacketReceived* packet, const WebRtcRtpHeaderExtensionMap* extensions, std::int64_t arrivalTimeUs) {
    if (!packet)
        throw std::invalid_argument("RtpPacketReceived constructor requires an object");
    WebRtcRtpHeaderExtensionMap hostExtensions{};
    if (extensions) {
        ErrnoGuard guard;
        if (!GuestCopyIn_nid_no_patch(&hostExtensions, extensions, sizeof(hostExtensions)))
            throw std::invalid_argument("RtpPacketReceived constructor requires a readable guest extensions pointer");
    }
    WebRtcRtpPacketReceived staged{};
    staged.extensions = hostExtensions;
    staged.payloadOffset = FixedHeaderSize;
    staged.arrivalTimeUs = arrivalTimeUs;
    _ZN3rtc17CopyOnWriteBufferC1Emm_nid_postfix(&staged.buffer, FixedHeaderSize, DefaultPacketSize);
    std::memset(staged.buffer.buffer->data, 0, FixedHeaderSize);
    staged.buffer.buffer->data[0] = RtpVersion << 6;
    std::memcpy(packet, &staged, sizeof(staged));
}

void APS5_VABI _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(WebRtcRtpPacketReceived* packet, const WebRtcRtpPacketReceived* other) {
    if (!packet || !other)
        throw std::invalid_argument("RtpPacketReceived copy constructor requires objects");
    WebRtcRtpPacketReceived staged;
    std::memcpy(&staged, other, sizeof(staged));
    const std::size_t entries = other->extensionEntriesEnd - other->extensionEntriesBegin;
    staged.extensionEntriesBegin = nullptr;
    staged.extensionEntriesEnd = nullptr;
    staged.extensionEntriesCapacity = nullptr;
    if (entries > 0) {
        staged.extensionEntriesBegin = static_cast<WebRtcRtpPacketExtensionInfo*>(_Znwm_nid_postfix(entries * sizeof(WebRtcRtpPacketExtensionInfo)));
        std::memcpy(staged.extensionEntriesBegin, other->extensionEntriesBegin, entries * sizeof(WebRtcRtpPacketExtensionInfo));
        staged.extensionEntriesEnd = staged.extensionEntriesBegin + entries;
        staged.extensionEntriesCapacity = staged.extensionEntriesEnd;
    }
    _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(&staged.buffer, &other->buffer);
    if (staged.additionalData)
        RefCount(staged.additionalData).fetch_add(1, std::memory_order_relaxed);
    std::memcpy(packet, &staged, sizeof(staged));
}

void APS5_VABI _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(WebRtcRtpPacketReceived* packet) {
    if (!packet)
        throw std::invalid_argument("RtpPacketReceived destructor requires an object");
    if (void* data = packet->additionalData) {
        packet->additionalData = nullptr;
        if (RefCount(data).fetch_sub(1, std::memory_order_acq_rel) == 1) {
            const void* const* vtable = *static_cast<const void* const* const*>(data);
            reinterpret_cast<DeletingDestructor>(const_cast<void*>(vtable[DeletingDestructorSlot]))(data);
        }
    }
    _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(&packet->buffer);
    if (packet->extensionEntriesBegin)
        _ZdlPv_nid_postfix(packet->extensionEntriesBegin);
}

}
