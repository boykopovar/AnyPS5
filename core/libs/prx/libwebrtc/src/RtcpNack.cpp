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

}

extern "C" {

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
