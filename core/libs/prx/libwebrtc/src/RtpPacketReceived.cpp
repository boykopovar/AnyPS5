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
#include "prx/libc/include/GuestArena.hpp"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>

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

void RequireReadable(const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    ErrnoGuard guard;
    if (!source)
        throw std::invalid_argument(message);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
    if (!GuestReadable_nid_no_patch(source, bytes))
        throw std::invalid_argument(message);
}

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    RequireReadable(source, bytes, message);
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

class GuestWriteScope {
public:
    GuestWriteScope(void* pointer, std::size_t bytes, const char* message)
        : guard(pointer ? pointer : nullptr, pointer ? bytes : 0) {
        if (!bytes) return;
        ErrnoGuard guardErrno;
        if (!pointer || !guard.Open() || !GuestWritable_nid_no_patch(pointer, bytes))
            throw std::invalid_argument(message);
    }
    GuestWriteScope(const GuestWriteScope&) = delete;
    GuestWriteScope& operator=(const GuestWriteScope&) = delete;

private:
    GuestArena::HostWrite guard;
};

void RequireWritable(void* pointer, std::size_t bytes, const char* message) {
    GuestWriteScope scope(pointer, bytes, message);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    GuestWriteScope scope(destination, bytes, message);
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

using DeletingDestructor = void(APS5_VABI*)(void*);

std::atomic_ref<std::int32_t> RefCount(void* object) {
    return std::atomic_ref<std::int32_t>(*reinterpret_cast<std::int32_t*>(static_cast<std::uint8_t*>(object) + RefCountOffset));
}

struct BufferGuard {
    WebRtcCopyOnWriteBuffer* buffer = nullptr;
    ~BufferGuard() {
        if (buffer)
            _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(buffer);
    }
    void Release() { buffer = nullptr; }
};

struct EntriesGuard {
    WebRtcRtpPacketExtensionInfo* entries = nullptr;
    ~EntriesGuard() {
        if (entries)
            _ZdlPv_nid_postfix(entries);
    }
    void Release() { entries = nullptr; }
};

}

extern "C" {

void APS5_VABI _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(
    WebRtcRtpPacketReceived* packet, const WebRtcRtpHeaderExtensionMap* extensions, std::int64_t arrivalTimeUs) {
    if (!packet)
        throw std::invalid_argument("RtpPacketReceived constructor requires an object");
    RequireWritable(packet, sizeof(*packet), "RtpPacketReceived constructor requires a writable guest packet pointer");
    WebRtcRtpHeaderExtensionMap hostExtensions{};
    if (extensions)
        CopyIn(&hostExtensions, extensions, sizeof(hostExtensions), "RtpPacketReceived constructor requires a readable guest extensions pointer");
    WebRtcRtpPacketReceived staged{};
    staged.extensions = hostExtensions;
    staged.payloadOffset = FixedHeaderSize;
    staged.arrivalTimeUs = arrivalTimeUs;
    _ZN3rtc17CopyOnWriteBufferC1Emm_nid_postfix(&staged.buffer, FixedHeaderSize, DefaultPacketSize);
    BufferGuard bufferGuard{&staged.buffer};
    std::uint8_t header[FixedHeaderSize]{};
    header[0] = RtpVersion << 6;
    CopyOut(staged.buffer.buffer->data, header, FixedHeaderSize, "RtpPacketReceived constructor requires a writable buffer");
    CopyOut(packet, &staged, sizeof(staged), "RtpPacketReceived constructor requires a writable guest packet pointer");
    bufferGuard.Release();
}

void APS5_VABI _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(WebRtcRtpPacketReceived* packet, const WebRtcRtpPacketReceived* other) {
    if (!packet || !other)
        throw std::invalid_argument("RtpPacketReceived copy constructor requires objects");
    const char* unreadable = "RtpPacketReceived copy constructor requires readable objects";
    const char* unwritable = "RtpPacketReceived copy constructor requires a writable object";
    WebRtcRtpPacketReceived otherCopy{};
    CopyIn(&otherCopy, other, sizeof(otherCopy), unreadable);

    std::vector<WebRtcRtpPacketExtensionInfo> entriesSnapshot;
    if (otherCopy.extensionEntriesBegin != otherCopy.extensionEntriesEnd) {
        const auto beginAddr = reinterpret_cast<std::uintptr_t>(otherCopy.extensionEntriesBegin);
        const auto endAddr = reinterpret_cast<std::uintptr_t>(otherCopy.extensionEntriesEnd);
        if (!otherCopy.extensionEntriesBegin || !otherCopy.extensionEntriesEnd || endAddr < beginAddr
            || (endAddr - beginAddr) % sizeof(WebRtcRtpPacketExtensionInfo) != 0)
            throw std::invalid_argument(unreadable);
        const std::size_t bytes = endAddr - beginAddr;
        RequireReadable(otherCopy.extensionEntriesBegin, bytes, unreadable);
        entriesSnapshot.resize(bytes / sizeof(WebRtcRtpPacketExtensionInfo));
        CopyIn(entriesSnapshot.data(), otherCopy.extensionEntriesBegin, bytes, unreadable);
    }

    RequireWritable(packet, sizeof(*packet), unwritable);
    std::optional<GuestWriteScope> bufferWrite;
    if (otherCopy.buffer.buffer)
        bufferWrite.emplace(otherCopy.buffer.buffer, sizeof(*otherCopy.buffer.buffer), unwritable);
    std::optional<GuestWriteScope> additionalDataWrite;
    if (otherCopy.additionalData)
        additionalDataWrite.emplace(static_cast<std::uint8_t*>(otherCopy.additionalData) + RefCountOffset, sizeof(std::int32_t), unwritable);

    WebRtcRtpPacketReceived staged = otherCopy;
    staged.extensionEntriesBegin = nullptr;
    staged.extensionEntriesEnd = nullptr;
    staged.extensionEntriesCapacity = nullptr;
    EntriesGuard entriesGuard;
    if (!entriesSnapshot.empty()) {
        const std::size_t bytes = entriesSnapshot.size() * sizeof(WebRtcRtpPacketExtensionInfo);
        staged.extensionEntriesBegin = static_cast<WebRtcRtpPacketExtensionInfo*>(_Znwm_nid_postfix(bytes));
        entriesGuard.entries = staged.extensionEntriesBegin;
        CopyOut(staged.extensionEntriesBegin, entriesSnapshot.data(), bytes, unwritable);
        staged.extensionEntriesEnd = staged.extensionEntriesBegin + entriesSnapshot.size();
        staged.extensionEntriesCapacity = staged.extensionEntriesEnd;
    }
    _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(&staged.buffer, &otherCopy.buffer);
    BufferGuard bufferGuard{&staged.buffer};
    if (staged.additionalData)
        RefCount(staged.additionalData).fetch_add(1, std::memory_order_relaxed);
    try {
        CopyOut(packet, &staged, sizeof(staged), unwritable);
    } catch (...) {
        if (staged.additionalData)
            RefCount(staged.additionalData).fetch_sub(1, std::memory_order_relaxed);
        throw;
    }
    bufferGuard.Release();
    entriesGuard.Release();
}

void APS5_VABI _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(WebRtcRtpPacketReceived* packet) {
    if (!packet)
        throw std::invalid_argument("RtpPacketReceived destructor requires an object");
    const char* badPacket = "RtpPacketReceived destructor requires a readable and writable object";
    WebRtcRtpPacketReceived packetCopy{};
    CopyIn(&packetCopy, packet, sizeof(packetCopy), badPacket);
    RequireWritable(packet, sizeof(*packet), badPacket);
    if (void* data = packetCopy.additionalData) {
        GuestWriteScope refWrite(static_cast<std::uint8_t*>(data) + RefCountOffset, sizeof(std::int32_t), badPacket);
        const void* const* vtable = nullptr;
        CopyIn(&vtable, data, sizeof(vtable), badPacket);
        const void* destructorEntry = nullptr;
        if (vtable)
            CopyIn(&destructorEntry, vtable + DeletingDestructorSlot, sizeof(destructorEntry), badPacket);
        void* cleared = nullptr;
        CopyOut(reinterpret_cast<unsigned char*>(packet) + offsetof(WebRtcRtpPacketReceived, additionalData), &cleared, sizeof(cleared), badPacket);
        if (RefCount(data).fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (!destructorEntry)
                throw std::invalid_argument(badPacket);
            reinterpret_cast<DeletingDestructor>(const_cast<void*>(destructorEntry))(data);
        }
    }
    {
        std::optional<GuestWriteScope> bufferWrite;
        if (packetCopy.buffer.buffer)
            bufferWrite.emplace(packetCopy.buffer.buffer, sizeof(*packetCopy.buffer.buffer), badPacket);
        _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(&packetCopy.buffer);
    }
    CopyOut(reinterpret_cast<unsigned char*>(packet) + offsetof(WebRtcRtpPacketReceived, buffer), &packetCopy.buffer, sizeof(packetCopy.buffer), badPacket);
    if (packetCopy.extensionEntriesBegin)
        _ZdlPv_nid_postfix(packetCopy.extensionEntriesBegin);
}

}
