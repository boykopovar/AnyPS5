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
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {

std::uint32_t ReadBigEndian(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < size; ++index)
        value = (value << 8) | bytes[index];
    return value;
}

template<typename TValue>
void EnsureCapacity(WebRtcNackVector<TValue>& vector, std::size_t size) {
    const std::size_t capacity = vector.begin ? vector.capacityEnd - vector.begin : 0;
    if (size <= capacity)
        return;
    const std::size_t count = vector.begin ? vector.end - vector.begin : 0;
    const std::size_t grownCapacity = std::max(size, 2 * capacity);
    auto* grown = static_cast<TValue*>(_Znwm_nid_postfix(grownCapacity * sizeof(TValue)));
    if (count != 0)
        std::memcpy(grown, vector.begin, count * sizeof(TValue));
    if (vector.begin)
        _ZdlPv_nid_postfix(vector.begin);
    vector.begin = grown;
    vector.end = grown + count;
    vector.capacityEnd = grown + grownCapacity;
}

void Append(WebRtcNackVector<std::uint16_t>& vector, std::uint16_t value) {
    const std::size_t size = vector.begin ? vector.end - vector.begin : 0;
    EnsureCapacity(vector, size + 1);
    *vector.end++ = value;
}

constexpr std::size_t HeaderLength = 4;
constexpr std::size_t CommonFeedbackLength = 8;
constexpr std::size_t NackItemLength = 4;
constexpr std::uint8_t FeedbackMessageType = 1;
constexpr std::uint8_t PacketType = 205;

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);

void WriteBigEndian(std::uint8_t* bytes, std::uint32_t value, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index)
        bytes[index] = static_cast<std::uint8_t>(value >> (8 * (size - 1 - index)));
}

std::size_t PackedCount(const WebRtcRtcpNack* nack) {
    return nack->packed.begin ? static_cast<std::size_t>(nack->packed.end - nack->packed.begin) : 0;
}

std::size_t APS5_VABI NackBlockLength_nid_no_patch(const WebRtcRtcpNack* nack) {
    return HeaderLength + CommonFeedbackLength + PackedCount(nack) * NackItemLength;
}

bool APS5_VABI NackCreate_nid_no_patch(const WebRtcRtcpNack* nack, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    const std::size_t count = PackedCount(nack);
    for (std::size_t nackIndex = 0; nackIndex < count;) {
        const std::size_t bytesLeft = maxLength - *index;
        if (bytesLeft < HeaderLength + CommonFeedbackLength + NackItemLength) {
            if (*index == 0)
                return false;
            if (!call)
                throw std::invalid_argument("Nack::Create needs a callback to send a full buffer");
            call(function, packet, *index);
            *index = 0;
            continue;
        }
        const std::size_t fields = std::min((bytesLeft - HeaderLength - CommonFeedbackLength) / NackItemLength, count - nackIndex);
        const std::size_t words = (CommonFeedbackLength + fields * NackItemLength) / 4;
        packet[*index] = static_cast<std::uint8_t>((2 << 6) | FeedbackMessageType);
        packet[*index + 1] = PacketType;
        WriteBigEndian(packet + *index + 2, static_cast<std::uint32_t>(words), 2);
        *index += HeaderLength;
        WriteBigEndian(packet + *index, nack->senderSsrc, 4);
        WriteBigEndian(packet + *index + 4, nack->mediaSsrc, 4);
        *index += CommonFeedbackLength;
        for (const std::size_t end = nackIndex + fields; nackIndex < end; ++nackIndex) {
            WriteBigEndian(packet + *index, nack->packed.begin[nackIndex].firstPid, 2);
            WriteBigEndian(packet + *index + 2, nack->packed.begin[nackIndex].bitmask, 2);
            *index += NackItemLength;
        }
    }
    return true;
}

void APS5_VABI DeleteNack_nid_no_patch(WebRtcRtcpNack* nack) {
    _ZN6webrtc4rtcp4NackD1Ev_nid_postfix(nack);
    _ZdlPv_nid_postfix(nack);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc4rtcp4NackD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteNack_nid_no_patch),
    reinterpret_cast<void*>(&NackBlockLength_nid_no_patch),
    reinterpret_cast<void*>(&NackCreate_nid_no_patch),
};

template<typename TValue>
void Release(WebRtcNackVector<TValue>& vector) {
    if (vector.begin)
        _ZdlPv_nid_postfix(vector.begin);
    vector = {};
}

}

extern "C" {

void APS5_VABI _ZN6webrtc4rtcp4NackC1Ev_nid_postfix(WebRtcRtcpNack* nack) {
    if (!nack)
        throw std::invalid_argument("Nack constructor requires a packet object");
    nack->vtable = Vtable + 2;
    nack->senderSsrc = 0;
    nack->mediaSsrc = 0;
    nack->packed = {};
    nack->packetIds = {};
}

void APS5_VABI _ZN6webrtc4rtcp4NackD1Ev_nid_postfix(WebRtcRtcpNack* nack) {
    if (!nack)
        throw std::invalid_argument("Nack destructor requires a packet object");
    Release(nack->packetIds);
    Release(nack->packed);
}

bool APS5_VABI _ZN6webrtc4rtcp4Nack5ParseERKNS0_12CommonHeaderE_nid_postfix(WebRtcRtcpNack* nack, const WebRtcRtcpCommonHeader* packet) {
    if (!nack || !packet)
        throw std::invalid_argument("Nack::Parse requires an object and common header");
    if (packet->payloadSize < 12)
        return false;
    if (!packet->payload)
        throw std::invalid_argument("Nack::Parse requires a payload");
    const std::size_t nackItems = (packet->payloadSize - 8) / 4;
    nack->senderSsrc = ReadBigEndian(packet->payload, 4);
    nack->mediaSsrc = ReadBigEndian(packet->payload + 4, 4);
    const auto* nextNack = packet->payload + 8;
    nack->packetIds.end = nack->packetIds.begin;
    EnsureCapacity(nack->packed, nackItems);
    nack->packed.end = nack->packed.begin + nackItems;
    for (std::size_t index = 0; index < nackItems; ++index) {
        nack->packed.begin[index].firstPid = ReadBigEndian(nextNack, 2);
        nack->packed.begin[index].bitmask = ReadBigEndian(nextNack + 2, 2);
        nextNack += 4;
    }
    for (auto* item = nack->packed.begin; item != nack->packed.end; ++item) {
        Append(nack->packetIds, item->firstPid);
        std::uint16_t pid = item->firstPid + 1;
        for (std::uint16_t bitmask = item->bitmask; bitmask != 0; bitmask >>= 1, ++pid) {
            if (bitmask & 1)
                Append(nack->packetIds, pid);
        }
    }
    return true;
}

}
