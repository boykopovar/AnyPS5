/* Derived from modules/rtp_rtcp/source/rtcp_packet/remb.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/rtp_rtcp/source/rtcp_packet/psfb.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from modules/rtp_rtcp/source/rtcp_packet.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
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

#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::size_t HeaderLength = 4;
constexpr std::size_t CommonFeedbackLength = 8;
constexpr std::uint8_t PacketType = 206;
constexpr std::uint8_t AfbMessageType = 15;
constexpr std::uint32_t UniqueIdentifier = 0x52454d42;
constexpr std::uint64_t MaxMantissa = 0x3ffff;
constexpr int BlockLengthSlot = 2;

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);

std::uint32_t ReadBigEndian(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) | (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

void WriteBigEndian(std::uint8_t* bytes, std::uint32_t value) {
    bytes[0] = static_cast<std::uint8_t>(value >> 24);
    bytes[1] = static_cast<std::uint8_t>(value >> 16);
    bytes[2] = static_cast<std::uint8_t>(value >> 8);
    bytes[3] = static_cast<std::uint8_t>(value);
}

std::size_t SsrcCount(const WebRtcRtcpRemb* remb) {
    return remb->ssrcBegin ? remb->ssrcEnd - remb->ssrcBegin : 0;
}

std::size_t APS5_VABI RembBlockLength_nid_no_patch(const WebRtcRtcpRemb* remb) {
    if (!remb)
        throw std::invalid_argument("Remb::BlockLength requires a packet object");
    return HeaderLength + CommonFeedbackLength + (2 + SsrcCount(remb)) * 4;
}

std::size_t BlockLength(const WebRtcRtcpRemb* remb) {
    using Function = std::size_t (APS5_VABI*)(const WebRtcRtcpRemb*);
    return reinterpret_cast<Function>(static_cast<void* const*>(const_cast<void*>(remb->vtable))[BlockLengthSlot])(remb);
}

bool APS5_VABI RembCreate_nid_no_patch(const WebRtcRtcpRemb* remb, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    if (!remb || !packet || !index)
        throw std::invalid_argument("Remb::Create requires a packet object, a buffer and an index");
    while (*index + BlockLength(remb) > maxLength) {
        if (*index == 0)
            return false;
        if (!call)
            throw std::invalid_argument("Remb::Create needs a callback to send a full buffer");
        call(function, packet, *index);
        *index = 0;
    }
    const std::size_t words = (BlockLength(remb) - HeaderLength) / 4;
    packet[*index] = static_cast<std::uint8_t>((2 << 6) | AfbMessageType);
    packet[*index + 1] = PacketType;
    packet[*index + 2] = static_cast<std::uint8_t>((words >> 8) & 0xff);
    packet[*index + 3] = static_cast<std::uint8_t>(words & 0xff);
    *index += HeaderLength;
    WriteBigEndian(packet + *index, remb->senderSsrc);
    WriteBigEndian(packet + *index + 4, remb->mediaSsrc);
    *index += CommonFeedbackLength;
    WriteBigEndian(packet + *index, UniqueIdentifier);
    *index += 4;
    std::uint64_t mantissa = std::bit_cast<std::uint64_t>(remb->bitrateBps);
    std::uint8_t exponent = 0;
    while (mantissa > MaxMantissa) {
        mantissa >>= 1;
        ++exponent;
    }
    const std::size_t count = SsrcCount(remb);
    packet[(*index)++] = static_cast<std::uint8_t>(count);
    packet[(*index)++] = static_cast<std::uint8_t>((exponent << 2) | (mantissa >> 16));
    packet[(*index)++] = static_cast<std::uint8_t>(mantissa >> 8);
    packet[(*index)++] = static_cast<std::uint8_t>(mantissa);
    for (std::size_t i = 0; i < count; ++i) {
        WriteBigEndian(packet + *index, remb->ssrcBegin[i]);
        *index += 4;
    }
    return true;
}

void APS5_VABI DeleteRemb_nid_no_patch(WebRtcRtcpRemb* remb) {
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(remb);
    _ZdlPv_nid_postfix(remb);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc4rtcp4RembD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteRemb_nid_no_patch),
    reinterpret_cast<void*>(&RembBlockLength_nid_no_patch),
    reinterpret_cast<void*>(&RembCreate_nid_no_patch),
};

}

extern "C" {

bool APS5_VABI _ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(WebRtcRtcpRemb* remb, const WebRtcRtcpCommonHeader* packet) {
    if (!remb || !packet)
        throw std::invalid_argument("Remb::Parse requires an object and common header");
    if (packet->payloadSize < 16)
        return false;
    const auto* payload = packet->payload;
    if (!payload)
        throw std::invalid_argument("Remb::Parse requires a payload");
    if (ReadBigEndian(payload + 8) != 0x52454d42)
        return false;
    const std::uint8_t count = payload[12];
    if (packet->payloadSize != 16u + count * 4u)
        return false;
    remb->senderSsrc = ReadBigEndian(payload);
    remb->mediaSsrc = ReadBigEndian(payload + 4);
    const std::uint8_t exponent = payload[13] >> 2;
    const std::uint64_t mantissa = (static_cast<std::uint32_t>(payload[13] & 3) << 16) | (static_cast<std::uint32_t>(payload[14]) << 8) | payload[15];
    remb->bitrateBps = std::bit_cast<std::int64_t>(mantissa << exponent);
    const bool shiftOverflow = (static_cast<std::uint64_t>(remb->bitrateBps) >> exponent) != mantissa;
    if (remb->bitrateBps < 0 || shiftOverflow)
        return false;
    remb->ssrcEnd = remb->ssrcBegin;
    const std::size_t capacity = remb->ssrcBegin ? remb->ssrcCapacityEnd - remb->ssrcBegin : 0;
    if (count > capacity) {
        auto* storage = static_cast<std::uint32_t*>(_Znwm_nid_postfix(count * sizeof(std::uint32_t)));
        _ZdlPv_nid_postfix(remb->ssrcBegin);
        remb->ssrcBegin = storage;
        remb->ssrcEnd = storage;
        remb->ssrcCapacityEnd = storage + count;
    }
    for (std::uint8_t i = 0; i < count; ++i) {
        *remb->ssrcEnd++ = ReadBigEndian(payload + 16 + i * 4);
    }
    return true;
}

void APS5_VABI _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(WebRtcRtcpRemb* remb) {
    if (!remb)
        throw std::invalid_argument("Remb constructor requires a packet object");
    remb->vtable = Vtable + 2;
    remb->senderSsrc = 0;
    remb->mediaSsrc = 0;
    remb->bitrateBps = 0;
    remb->ssrcBegin = nullptr;
    remb->ssrcEnd = nullptr;
    remb->ssrcCapacityEnd = nullptr;
}

void APS5_VABI _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(WebRtcRtcpRemb* remb) {
    if (!remb)
        throw std::invalid_argument("Remb destructor requires a packet object");
    if (remb->ssrcBegin)
        _ZdlPv_nid_postfix(remb->ssrcBegin);
}

}
