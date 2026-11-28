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
#include "prx/libc/include/GuestPtr.hpp"

#include <cerrno>
#include <stdexcept>

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

}

extern "C" {

void APS5_VABI _ZN3rtc10SentPacketC1Ell_nid_postfix(WebRtcSentPacket* packet, std::int64_t packetId, std::int64_t sendTimeMs) {
    if (!packet)
        throw std::invalid_argument("SentPacket::SentPacket requires a packet");
    WebRtcSentPacket host{};
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(packet, sizeof(*packet)) ||
        !GuestPtr<const WebRtcSentPacket>(packet).CopyIn(host))
        throw std::invalid_argument("SentPacket::SentPacket requires a packet");
    host.packetId = packetId;
    host.sendTimeMs = sendTimeMs;
    host.info.includedInFeedback = false;
    host.info.includedInAllocation = false;
    host.info.isMedia = false;
    host.info.packetType = 0;
    host.info.protocol = 0;
    host.info.hasNetworkId = false;
    host.info.packetSizeBytes = 0;
    host.info.turnOverheadBytes = 0;
    host.info.ipOverheadBytes = 0;
    if (!GuestPtr<WebRtcSentPacket>(packet).CopyOut(host))
        throw std::invalid_argument("SentPacket::SentPacket requires a packet");
}

void APS5_VABI _ZN3rtc10SentPacketC1EllRKNS_10PacketInfoE_nid_postfix(WebRtcSentPacket* packet, std::int64_t packetId, std::int64_t sendTimeMs, const WebRtcPacketInfo* info) {
    if (!packet || !info)
        throw std::invalid_argument("SentPacket::SentPacket requires a packet and info");
    WebRtcPacketInfo hostInfo{};
    WebRtcSentPacket host{};
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(packet, sizeof(*packet)) ||
        !GuestPtr<const WebRtcPacketInfo>(info).CopyIn(hostInfo) ||
        !GuestPtr<const WebRtcSentPacket>(packet).CopyIn(host))
        throw std::invalid_argument("SentPacket::SentPacket requires a packet and info");
    host.packetId = packetId;
    host.sendTimeMs = sendTimeMs;
    host.info.includedInFeedback = hostInfo.includedInFeedback;
    host.info.includedInAllocation = hostInfo.includedInAllocation;
    host.info.isMedia = hostInfo.isMedia;
    host.info.packetType = hostInfo.packetType;
    host.info.protocol = hostInfo.protocol;
    host.info.hasNetworkId = hostInfo.hasNetworkId;
    if (hostInfo.hasNetworkId)
        host.info.networkId = hostInfo.networkId;
    host.info.packetSizeBytes = hostInfo.packetSizeBytes;
    host.info.turnOverheadBytes = hostInfo.turnOverheadBytes;
    host.info.ipOverheadBytes = hostInfo.ipOverheadBytes;
    if (!GuestPtr<WebRtcSentPacket>(packet).CopyOut(host))
        throw std::invalid_argument("SentPacket::SentPacket requires a packet and info");
}

}
