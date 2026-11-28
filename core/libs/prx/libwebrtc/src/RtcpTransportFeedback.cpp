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
#include "prx/libc/include/GuestPtr.hpp"

#include <algorithm>
#include <cstring>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::size_t MaxRtcpPayloadBytes = 65535u * 4u;

template <typename T>
T ReadGuest(const T* address, const char* message) {
    T value{};
    if (!GuestPtr<const T>(address).CopyIn(value))
        throw std::invalid_argument(message);
    return value;
}

template <typename T>
void WriteGuest(T* address, const T& value, const char* message) {
    if (!GuestPtr<T>(address).CopyOut(value))
        throw std::invalid_argument(message);
}

template <typename T>
void ValidateVector(const WebRtcFeedbackVector<T>& values) {
    if (!values.begin) {
        if (values.end || values.capacity)
            throw std::invalid_argument("TransportFeedback vector pointers are inconsistent");
        return;
    }
    if (!values.end || !values.capacity || values.end < values.begin || values.capacity < values.end)
        throw std::invalid_argument("TransportFeedback vector pointers are out of order");
}

template <typename T>
struct VectorPlan {
    bool grown = false;
    bool committedInPlace = false;
    T* newBegin = nullptr;
    T* newEnd = nullptr;
    T* newCapacity = nullptr;
    T* oldToFree = nullptr;
    std::size_t size = 0;
    std::vector<T> previousData;

    ~VectorPlan() {
        if (grown && newBegin)
            _ZdlPv_nid_postfix(newBegin);
        else if (committedInPlace && newBegin && !previousData.empty())
            GuestBytes(newBegin).CopyOut(previousData.data(), previousData.size() * sizeof(T));
    }
};

template <typename T>
VectorPlan<T> PrepareVectorWrite(const WebRtcFeedbackVector<T>& values, const T* data, std::size_t size) {
    ValidateVector(values);
    VectorPlan<T> plan;
    plan.size = size;
    if (size == 0) {
        plan.newBegin = values.begin;
        plan.newEnd = values.begin;
        plan.newCapacity = values.capacity;
        return plan;
    }
    const std::size_t currentCapacity = values.begin ? static_cast<std::size_t>(values.capacity - values.begin) : 0;
    if (size > currentCapacity) {
        auto* storage = static_cast<T*>(_Znwm_nid_postfix(size * sizeof(T)));
        plan.grown = true;
        plan.newBegin = storage;
        plan.newEnd = storage + size;
        plan.newCapacity = storage + size;
        plan.oldToFree = values.begin;
        if (!GuestWritable_nid_no_patch(storage, size * sizeof(T)) ||
            !GuestBytes(storage).CopyOut(data, size * sizeof(T)))
            throw std::invalid_argument("TransportFeedback vector destination is not writable");
        return plan;
    }
    if (!GuestWritable_nid_no_patch(values.begin, size * sizeof(T)))
        throw std::invalid_argument("TransportFeedback vector destination is not writable");
    plan.previousData.resize(size);
    if (!GuestBytes(values.begin).CopyIn(plan.previousData.data(), size * sizeof(T)))
        throw std::invalid_argument("TransportFeedback vector destination is not writable");
    plan.newBegin = values.begin;
    plan.newEnd = values.begin + size;
    plan.newCapacity = values.capacity;
    return plan;
}

template <typename T>
void CommitVectorWrite(WebRtcFeedbackVector<T>& values, VectorPlan<T>& plan, const T* data) {
    if (!plan.grown && plan.size != 0) {
        if (!GuestBytes(plan.newBegin).CopyOut(data, plan.size * sizeof(T)))
            throw std::invalid_argument("TransportFeedback vector destination is not writable");
        plan.committedInPlace = true;
    }
    values = {plan.newBegin, plan.newEnd, plan.newCapacity};
}

template <typename T>
void FinalizeVectorWrite(VectorPlan<T>& plan) {
    plan.grown = false;
    plan.committedInPlace = false;
    plan.previousData.clear();
    if (plan.oldToFree) {
        _ZdlPv_nid_postfix(plan.oldToFree);
        plan.oldToFree = nullptr;
    }
}

void Clear(WebRtcRtcpTransportFeedback& feedback) {
    feedback.sequenceNumberCount = 0;
    feedback.lastTimestamp = (std::int64_t{1} << 24) * 64000 + std::int64_t{feedback.baseTimeTicks} * 64000;
    feedback.receivedPackets.end = feedback.receivedPackets.begin;
    feedback.allPackets.end = feedback.allPackets.begin;
    feedback.encodedChunks.end = feedback.encodedChunks.begin;
    feedback.lastChunk.size = 0;
    feedback.lastChunk.allSame = true;
    feedback.lastChunk.hasLargeDelta = false;
    feedback.sizeBytes = 20;
}

void Decode(WebRtcFeedbackLastChunk& state, std::uint16_t chunk, std::size_t remaining) {
    state.allSame = (chunk & 0x8000) == 0;
    if (state.allSame) {
        state.size = std::min<std::size_t>(chunk & 0x1fff, remaining);
        const auto symbol = (chunk >> 13) & 3;
        state.hasLargeDelta = symbol >= 2;
        std::fill_n(state.deltaSizes, std::min<std::size_t>(state.size, 14), symbol);
    } else {
        state.hasLargeDelta = (chunk & 0x4000) != 0;
        const std::size_t capacity = state.hasLargeDelta ? 7 : 14;
        state.size = std::min(capacity, remaining);
        for (std::size_t i = 0; i < state.size; ++i)
            state.deltaSizes[i] = (chunk >> ((capacity - 1 - i) * (state.hasLargeDelta ? 2 : 1))) & (state.hasLargeDelta ? 3 : 1);
    }
}

void APS5_VABI DeleteFeedback_nid_no_patch(WebRtcRtcpTransportFeedback* feedback) {
    _ZN6webrtc4rtcp17TransportFeedbackD1Ev_nid_postfix(feedback);
    _ZdlPv_nid_postfix(feedback);
}

void APS5_VABI UnsupportedFeedback_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc4rtcp17TransportFeedbackD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteFeedback_nid_no_patch),
    reinterpret_cast<void*>(&UnsupportedFeedback_nid_no_patch),
    reinterpret_cast<void*>(&UnsupportedFeedback_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc4rtcp17TransportFeedbackC1Ev_nid_postfix(WebRtcRtcpTransportFeedback* feedback) {
    if (!feedback || !GuestWritable_nid_no_patch(feedback, sizeof(*feedback)))
        throw std::invalid_argument("TransportFeedback constructor requires an object");
    WebRtcRtcpTransportFeedback host{};
    host.vtable = Vtable + 2;
    host.includeTimestamps = true;
    host.lastChunk.allSame = true;
    host.sizeBytes = 20;
    WriteGuest(feedback, host, "TransportFeedback constructor requires an object");
}

void APS5_VABI _ZN6webrtc4rtcp17TransportFeedbackD1Ev_nid_postfix(WebRtcRtcpTransportFeedback* feedback) {
    const auto host = ReadGuest(feedback, "TransportFeedback destructor requires an object");
    if (host.receivedPackets.begin) _ZdlPv_nid_postfix(host.receivedPackets.begin);
    if (host.allPackets.begin) _ZdlPv_nid_postfix(host.allPackets.begin);
    if (host.encodedChunks.begin) _ZdlPv_nid_postfix(host.encodedChunks.begin);
}

bool APS5_VABI _ZN6webrtc4rtcp17TransportFeedback5ParseERKNS0_12CommonHeaderE_nid_postfix(WebRtcRtcpTransportFeedback* feedback, const WebRtcRtcpCommonHeader* packet) {
    if (!feedback || !GuestWritable_nid_no_patch(feedback, sizeof(*feedback)))
        throw std::invalid_argument("TransportFeedback::Parse requires an object and header");
    if (!packet) throw std::invalid_argument("TransportFeedback::Parse requires an object and header");
    auto hostFeedback = ReadGuest(feedback, "TransportFeedback::Parse requires an object and header");
    const auto hostPacket = ReadGuest(packet, "TransportFeedback::Parse requires an object and header");
    if (hostPacket.payloadSize < 18) return false;
    if (hostPacket.payloadSize > MaxRtcpPayloadBytes)
        throw std::invalid_argument("TransportFeedback::Parse payload exceeds maximum RTCP length");
    if (!hostPacket.payload) throw std::invalid_argument("TransportFeedback::Parse requires a payload");
    ValidateVector(hostFeedback.receivedPackets);
    ValidateVector(hostFeedback.allPackets);
    ValidateVector(hostFeedback.encodedChunks);
    std::vector<std::uint8_t> payload(hostPacket.payloadSize);
    if (!GuestBytes(hostPacket.payload).CopyIn(payload.data(), payload.size()))
        throw std::invalid_argument("TransportFeedback::Parse requires a readable payload");
    auto read = [&payload](std::size_t offset, std::size_t count) {
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < count; ++i) value = (value << 8) | payload[offset + i];
        return value;
    };
    hostFeedback.senderSsrc = read(0, 4);
    hostFeedback.mediaSsrc = read(4, 4);
    hostFeedback.baseSequenceNumber = static_cast<std::uint16_t>(read(8, 2));
    const auto statusCount = static_cast<std::uint16_t>(read(10, 2));
    hostFeedback.baseTimeTicks = read(12, 3);
    hostFeedback.feedbackSequence = payload[15];
    Clear(hostFeedback);
    if (!statusCount) {
        WriteGuest(feedback, hostFeedback, "TransportFeedback::Parse requires a writable object");
        return false;
    }
    std::size_t index = 16;
    std::vector<std::uint8_t> deltaSizes;
    deltaSizes.reserve(statusCount);
    std::vector<std::uint16_t> encodedChunks;
    while (deltaSizes.size() < statusCount) {
        if (index + 2 > payload.size()) {
            Clear(hostFeedback);
            WriteGuest(feedback, hostFeedback, "TransportFeedback::Parse requires a writable object");
            return false;
        }
        const auto chunk = static_cast<std::uint16_t>(read(index, 2));
        index += 2;
        encodedChunks.push_back(chunk);
        Decode(hostFeedback.lastChunk, chunk, statusCount - deltaSizes.size());
        const auto& state = hostFeedback.lastChunk;
        for (std::size_t i = 0; i < state.size; ++i)
            deltaSizes.push_back(state.deltaSizes[state.allSame ? 0 : i]);
    }
    encodedChunks.pop_back();
    hostFeedback.sequenceNumberCount = statusCount;
    std::size_t deltaBytes = 0;
    for (auto size : deltaSizes) deltaBytes += size;
    const bool hasDeltas = payload.size() >= index + deltaBytes;
    if (!hasDeltas) hostFeedback.includeTimestamps = false;
    std::vector<WebRtcFeedbackReceivedPacket> receivedPackets;
    auto sequence = hostFeedback.baseSequenceNumber;
    for (auto size : deltaSizes) {
        if (size) {
            std::int16_t delta = 0;
            if (hasDeltas) {
                if (size == 3) {
                    Clear(hostFeedback);
                    WriteGuest(feedback, hostFeedback, "TransportFeedback::Parse requires a writable object");
                    return false;
                }
                delta = static_cast<std::int16_t>(read(index, size));
                index += size;
                hostFeedback.lastTimestamp += std::int64_t{delta} * 250;
            }
            receivedPackets.push_back(WebRtcFeedbackReceivedPacket{sequence, delta});
        }
        ++sequence;
    }
    auto chunksPlan = PrepareVectorWrite(hostFeedback.encodedChunks, encodedChunks.data(), encodedChunks.size());
    auto packetsPlan = PrepareVectorWrite(hostFeedback.receivedPackets, receivedPackets.data(), receivedPackets.size());
    CommitVectorWrite(hostFeedback.encodedChunks, chunksPlan, encodedChunks.data());
    CommitVectorWrite(hostFeedback.receivedPackets, packetsPlan, receivedPackets.data());
    hostFeedback.sizeBytes = 4 + index;
    WriteGuest(feedback, hostFeedback, "TransportFeedback::Parse requires a writable object");
    FinalizeVectorWrite(chunksPlan);
    FinalizeVectorWrite(packetsPlan);
    return true;
}

std::uint16_t APS5_VABI _ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(const WebRtcRtcpTransportFeedback* feedback) {
    return ReadGuest(feedback, "TransportFeedback::GetBaseSequence requires a packet").baseSequenceNumber;
}

}
