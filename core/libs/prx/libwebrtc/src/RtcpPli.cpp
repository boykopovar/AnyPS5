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

#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::size_t HeaderLength = 4;
constexpr std::size_t BlockLength = HeaderLength + WebRtcRtcpCommonFeedbackSize;
constexpr std::uint8_t PacketType = 206;
constexpr std::uint8_t FeedbackMessageType = 1;

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

std::size_t APS5_VABI PliBlockLength_nid_no_patch(const WebRtcRtcpPli*) {
    return BlockLength;
}

bool APS5_VABI PliCreate_nid_no_patch(const WebRtcRtcpPli* pli, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    while (*index + BlockLength > maxLength) {
        if (*index == 0)
            return false;
        if (!call)
            throw std::invalid_argument("Pli::Create needs a callback to send a full buffer");
        call(function, packet, *index);
        *index = 0;
    }
    packet[*index] = static_cast<std::uint8_t>((2 << 6) | FeedbackMessageType);
    packet[*index + 1] = PacketType;
    packet[*index + 2] = 0;
    packet[*index + 3] = static_cast<std::uint8_t>(WebRtcRtcpCommonFeedbackSize / 4);
    *index += HeaderLength;
    WriteBigEndian(packet + *index, pli->senderSsrc);
    WriteBigEndian(packet + *index + 4, pli->mediaSsrc);
    *index += WebRtcRtcpCommonFeedbackSize;
    return true;
}

void APS5_VABI DeletePli_nid_no_patch(WebRtcRtcpPli* pli) {
    _ZN6webrtc4rtcp3PliD1Ev_nid_postfix(pli);
    _ZdlPv_nid_postfix(pli);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc4rtcp3PliD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeletePli_nid_no_patch),
    reinterpret_cast<void*>(&PliBlockLength_nid_no_patch),
    reinterpret_cast<void*>(&PliCreate_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(WebRtcRtcpPli* pli) {
    if (!pli)
        throw std::invalid_argument("Pli constructor requires a packet object");
    pli->vtable = Vtable + 2;
    pli->senderSsrc = 0;
    pli->mediaSsrc = 0;
}

bool APS5_VABI _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(WebRtcRtcpPli* pli, const WebRtcRtcpCommonHeader* packet) {
    if (!pli)
        throw std::invalid_argument("Pli::Parse requires a packet object");
    if (!packet)
        throw std::invalid_argument("Pli::Parse requires a common header");
    if (packet->payloadSize < WebRtcRtcpCommonFeedbackSize)
        return false;
    if (!packet->payload)
        throw std::invalid_argument("Pli::Parse requires a payload");
    pli->senderSsrc = ReadBigEndian(packet->payload);
    pli->mediaSsrc = ReadBigEndian(packet->payload + 4);
    return true;
}

}
