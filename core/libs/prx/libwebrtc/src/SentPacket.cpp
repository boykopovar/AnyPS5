/* Derived from rtc_base/network/sent_packet.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstring>
#include <stdexcept>

extern "C" {

void APS5_VABI _ZN3rtc10SentPacketC1Ell_nid_postfix(WebRtcSentPacket* packet, std::int64_t packetId, std::int64_t sendTimeMs) {
    if (!packet)
        throw std::invalid_argument("SentPacket constructor requires a packet");
    packet->packetId = packetId;
    packet->sendTimeMs = sendTimeMs;
    packet->info.includedInFeedback = false;
    packet->info.includedInAllocation = false;
    packet->info.isMedia = false;
    packet->info.packetType = WebRtcPacketType::kUnknown;
    packet->info.protocol = WebRtcPacketInfoProtocolType::kUnknown;
    packet->info.networkId.engaged = false;
    packet->info.packetSizeBytes = 0;
    packet->info.turnOverheadBytes = 0;
    packet->info.ipOverheadBytes = 0;
}

void APS5_VABI _ZN3rtc10SentPacketC1EllRKNS_10PacketInfoE_nid_postfix(WebRtcSentPacket* packet, std::int64_t packetId, std::int64_t sendTimeMs, const WebRtcPacketInfo* info) {
    if (!packet)
        throw std::invalid_argument("SentPacket constructor requires a packet");
    if (!info)
        throw std::invalid_argument("SentPacket constructor requires packet information");
    packet->packetId = packetId;
    packet->sendTimeMs = sendTimeMs;
    std::memcpy(&packet->info, info, sizeof(packet->info));
}

}
