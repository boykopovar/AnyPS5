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

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"

#include <bit>
#include <stdexcept>

namespace {

std::uint32_t ReadBigEndian(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) | (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

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

}
