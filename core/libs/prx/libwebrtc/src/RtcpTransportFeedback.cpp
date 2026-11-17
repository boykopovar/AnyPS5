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

#include <algorithm>
#include <cstring>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace {

template <typename T>
void Append(WebRtcFeedbackVector<T>& values, T value) {
    if (values.end == values.capacity) {
        const auto size = values.begin ? static_cast<std::size_t>(values.end - values.begin) : 0;
        const auto capacity = std::max<std::size_t>(1, size * 2);
        auto* storage = static_cast<T*>(_Znwm_nid_postfix(capacity * sizeof(T)));
        if (size) std::memcpy(storage, values.begin, size * sizeof(T));
        if (values.begin) _ZdlPv_nid_postfix(values.begin);
        values = {storage, storage + size, storage + capacity};
    }
    *values.end++ = value;
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
    if (!feedback) throw std::invalid_argument("TransportFeedback constructor requires an object");
    *feedback = {};
    feedback->vtable = Vtable + 2;
    feedback->includeTimestamps = true;
    feedback->lastChunk.allSame = true;
    feedback->sizeBytes = 20;
}

void APS5_VABI _ZN6webrtc4rtcp17TransportFeedbackD1Ev_nid_postfix(WebRtcRtcpTransportFeedback* feedback) {
    if (!feedback) throw std::invalid_argument("TransportFeedback destructor requires an object");
    if (feedback->receivedPackets.begin) _ZdlPv_nid_postfix(feedback->receivedPackets.begin);
    if (feedback->allPackets.begin) _ZdlPv_nid_postfix(feedback->allPackets.begin);
    if (feedback->encodedChunks.begin) _ZdlPv_nid_postfix(feedback->encodedChunks.begin);
}

bool APS5_VABI _ZN6webrtc4rtcp17TransportFeedback5ParseERKNS0_12CommonHeaderE_nid_postfix(WebRtcRtcpTransportFeedback* feedback, const WebRtcRtcpCommonHeader* packet) {
    if (!feedback || !packet) throw std::invalid_argument("TransportFeedback::Parse requires an object and header");
    if (packet->payloadSize < 18) return false;
    if (!packet->payload) throw std::invalid_argument("TransportFeedback::Parse requires a payload");
    const auto* payload = packet->payload;
    auto read = [payload](std::size_t offset, std::size_t count) {
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < count; ++i) value = (value << 8) | payload[offset + i];
        return value;
    };
    feedback->senderSsrc = read(0, 4);
    feedback->mediaSsrc = read(4, 4);
    feedback->baseSequenceNumber = read(8, 2);
    const auto statusCount = read(10, 2);
    feedback->baseTimeTicks = read(12, 3);
    feedback->feedbackSequence = payload[15];
    Clear(*feedback);
    if (!statusCount) return false;
    std::size_t index = 16;
    std::vector<std::uint8_t> deltaSizes;
    deltaSizes.reserve(statusCount);
    while (deltaSizes.size() < statusCount) {
        if (index + 2 > packet->payloadSize) {
            Clear(*feedback);
            return false;
        }
        const auto chunk = static_cast<std::uint16_t>(read(index, 2));
        index += 2;
        Append(feedback->encodedChunks, chunk);
        Decode(feedback->lastChunk, chunk, statusCount - deltaSizes.size());
        const auto& state = feedback->lastChunk;
        for (std::size_t i = 0; i < state.size; ++i)
            deltaSizes.push_back(state.deltaSizes[state.allSame ? 0 : i]);
    }
    --feedback->encodedChunks.end;
    feedback->sequenceNumberCount = statusCount;
    std::size_t deltaBytes = 0;
    for (auto size : deltaSizes) deltaBytes += size;
    const bool hasDeltas = packet->payloadSize >= index + deltaBytes;
    if (!hasDeltas) feedback->includeTimestamps = false;
    auto sequence = feedback->baseSequenceNumber;
    for (auto size : deltaSizes) {
        if (size) {
            std::int16_t delta = 0;
            if (hasDeltas) {
                if (size == 3) {
                    Clear(*feedback);
                    return false;
                }
                delta = static_cast<std::int16_t>(read(index, size));
                index += size;
                feedback->lastTimestamp += std::int64_t{delta} * 250;
            }
            Append(feedback->receivedPackets, WebRtcFeedbackReceivedPacket{sequence, delta});
        }
        ++sequence;
    }
    feedback->sizeBytes = 4 + index;
    return true;
}

std::uint16_t APS5_VABI _ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(const WebRtcRtcpTransportFeedback* feedback) {
    if (!feedback)
        throw std::invalid_argument("TransportFeedback::GetBaseSequence requires a packet");
    return feedback->baseSequenceNumber;
}

}
