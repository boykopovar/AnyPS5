/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
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

#include <stdexcept>

extern "C" {

void APS5_VABI _ZN3rtc10SentPacketC1Ell_nid_postfix(WebRtcSentPacket* packet, std::int64_t packetId, std::int64_t sendTimeMs) {
    if (!packet)
        throw std::invalid_argument("SentPacket::SentPacket requires a packet");
    packet->packetId = packetId;
    packet->sendTimeMs = sendTimeMs;
    packet->info.includedInFeedback = false;
    packet->info.includedInAllocation = false;
    packet->info.isMedia = false;
    packet->info.packetType = 0;
    packet->info.protocol = 0;
    packet->info.hasNetworkId = false;
    packet->info.packetSizeBytes = 0;
    packet->info.turnOverheadBytes = 0;
    packet->info.ipOverheadBytes = 0;
}

void APS5_VABI _ZN3rtc10SentPacketC1EllRKNS_10PacketInfoE_nid_postfix(WebRtcSentPacket* packet, std::int64_t packetId, std::int64_t sendTimeMs, const WebRtcPacketInfo* info) {
    if (!packet || !info)
        throw std::invalid_argument("SentPacket::SentPacket requires a packet and info");
    packet->packetId = packetId;
    packet->sendTimeMs = sendTimeMs;
    packet->info.includedInFeedback = info->includedInFeedback;
    packet->info.includedInAllocation = info->includedInAllocation;
    packet->info.isMedia = info->isMedia;
    packet->info.packetType = info->packetType;
    packet->info.protocol = info->protocol;
    packet->info.hasNetworkId = info->hasNetworkId;
    if (info->hasNetworkId)
        packet->info.networkId = info->networkId;
    packet->info.packetSizeBytes = info->packetSizeBytes;
    packet->info.turnOverheadBytes = info->turnOverheadBytes;
    packet->info.ipOverheadBytes = info->ipOverheadBytes;
}

}
