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
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestPtr.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t HeaderLength = 4;
constexpr std::size_t CommonFeedbackLength = 8;
constexpr std::size_t NackItemLength = 4;
constexpr std::uint8_t FeedbackMessageType = 1;
constexpr std::uint8_t PacketType = 205;

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);

std::uint32_t ReadBigEndian(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < size; ++index)
        value = (value << 8) | bytes[index];
    return value;
}

template<typename T>
T ReadGuest(const T* address, const char* message) {
    T value{};
    if (!GuestPtr<const T>(address).CopyIn(value))
        throw std::invalid_argument(message);
    return value;
}

template<typename T>
void WriteGuest(T* address, const T& value, const char* message) {
    if (!GuestPtr<T>(address).CopyOut(value))
        throw std::invalid_argument(message);
}

template<typename TValue>
void ValidateVector(const WebRtcNackVector<TValue>& vector) {
    if (!vector.begin) {
        if (vector.end || vector.capacityEnd)
            throw std::invalid_argument("Nack vector pointers are inconsistent");
        return;
    }
    if (!vector.end || !vector.capacityEnd || vector.end < vector.begin || vector.capacityEnd < vector.end)
        throw std::invalid_argument("Nack vector pointers are out of order");
}

template<typename TValue>
void WriteVector(WebRtcNackVector<TValue>& vector, const TValue* values, std::size_t size) {
    ValidateVector(vector);
    if (size == 0) {
        vector.end = vector.begin;
        return;
    }
    const std::size_t capacity = vector.begin ? static_cast<std::size_t>(vector.capacityEnd - vector.begin) : 0;
    if (size > capacity) {
        const std::size_t grownCapacity = std::max(size, 2 * capacity);
        auto* grown = static_cast<TValue*>(_Znwm_nid_postfix(grownCapacity * sizeof(TValue)));
        if (!GuestBytes(grown).CopyOut(values, size * sizeof(TValue))) {
            _ZdlPv_nid_postfix(grown);
            throw std::invalid_argument("Nack vector destination is not writable");
        }
        if (vector.begin)
            _ZdlPv_nid_postfix(vector.begin);
        vector.begin = grown;
        vector.end = grown + size;
        vector.capacityEnd = grown + grownCapacity;
        return;
    }
    if (!GuestBytes(vector.begin).CopyOut(values, size * sizeof(TValue)))
        throw std::invalid_argument("Nack vector destination is not writable");
    vector.end = vector.begin + size;
}

void WriteBigEndian(std::uint8_t* bytes, std::uint32_t value, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index)
        bytes[index] = static_cast<std::uint8_t>(value >> (8 * (size - 1 - index)));
}

std::size_t PackedCount(const WebRtcRtcpNack& nack) {
    ValidateVector(nack.packed);
    return nack.packed.begin ? static_cast<std::size_t>(nack.packed.end - nack.packed.begin) : 0;
}

bool OnBufferFull(std::uint8_t* packet, std::size_t* index, std::size_t& hostIndex, void* function, PacketReady call) {
    if (hostIndex == 0)
        return false;
    if (!call)
        throw std::invalid_argument("Nack::Create needs a callback to send a full buffer");
    call(function, packet, hostIndex);
    hostIndex = 0;
    WriteGuest(index, hostIndex, "Nack::Create requires a writable index");
    return true;
}

std::size_t APS5_VABI NackBlockLength_nid_no_patch(const WebRtcRtcpNack* nack) {
    const auto hostNack = ReadGuest(nack, "Nack::BlockLength requires a packet object");
    return HeaderLength + CommonFeedbackLength + PackedCount(hostNack) * NackItemLength;
}

bool APS5_VABI NackCreate_nid_no_patch(const WebRtcRtcpNack* nack, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    if (!nack || !packet || !index)
        throw std::invalid_argument("Nack::Create requires a packet object, a buffer and an index");
    const auto hostNack = ReadGuest(nack, "Nack::Create requires a readable packet object");
    std::size_t hostIndex = ReadGuest(index, "Nack::Create requires a readable index");
    if (!GuestWritable_nid_no_patch(index, sizeof(*index)))
        throw std::invalid_argument("Nack::Create requires a writable index");
    if (hostIndex > maxLength)
        throw std::invalid_argument("Nack::Create index is past the end of the buffer");
    constexpr std::size_t NackHeaderLength = HeaderLength + CommonFeedbackLength;
    const std::size_t count = PackedCount(hostNack);
    std::vector<WebRtcPackedNack> items(count);
    if (count != 0 && !GuestBytes(hostNack.packed.begin).CopyIn(items.data(), count * sizeof(WebRtcPackedNack)))
        throw std::invalid_argument("Nack::Create requires readable packed items");
    for (std::size_t nackIndex = 0; nackIndex < count;) {
        const std::size_t bytesLeft = maxLength - hostIndex;
        if (bytesLeft < NackHeaderLength + NackItemLength) {
            if (!OnBufferFull(packet, index, hostIndex, function, call))
                return false;
            continue;
        }
        const std::size_t fields = std::min((bytesLeft - NackHeaderLength) / NackItemLength, count - nackIndex);
        const std::size_t words = (CommonFeedbackLength + fields * NackItemLength) / 4;
        std::vector<std::uint8_t> encoded(NackHeaderLength + fields * NackItemLength);
        encoded[0] = static_cast<std::uint8_t>((2 << 6) | FeedbackMessageType);
        encoded[1] = PacketType;
        encoded[2] = static_cast<std::uint8_t>((words >> 8) & 0xff);
        encoded[3] = static_cast<std::uint8_t>(words & 0xff);
        WriteBigEndian(encoded.data() + HeaderLength, hostNack.senderSsrc, 4);
        WriteBigEndian(encoded.data() + HeaderLength + 4, hostNack.mediaSsrc, 4);
        std::size_t offset = NackHeaderLength;
        for (const std::size_t end = nackIndex + fields; nackIndex < end; ++nackIndex) {
            const WebRtcPackedNack& item = items[nackIndex];
            WriteBigEndian(encoded.data() + offset, item.firstPid, 2);
            WriteBigEndian(encoded.data() + offset + 2, item.bitmask, 2);
            offset += NackItemLength;
        }
        if (!GuestBytes(packet + hostIndex).CopyOut(encoded.data(), encoded.size()))
            throw std::invalid_argument("Nack::Create requires a writable destination buffer");
        hostIndex += encoded.size();
        WriteGuest(index, hostIndex, "Nack::Create requires a writable index");
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

}

extern "C" {

bool APS5_VABI _ZN6webrtc4rtcp4Nack5ParseERKNS0_12CommonHeaderE_nid_postfix(WebRtcRtcpNack* nack, const WebRtcRtcpCommonHeader* packet) {
    if (!nack || !GuestWritable_nid_no_patch(nack, sizeof(*nack)))
        throw std::invalid_argument("Nack::Parse requires a writable packet object");
    if (!packet)
        throw std::invalid_argument("Nack::Parse requires an object and common header");
    auto hostNack = ReadGuest(nack, "Nack::Parse requires an object and common header");
    const auto hostPacket = ReadGuest(packet, "Nack::Parse requires an object and common header");
    if (hostPacket.payloadSize < 12)
        return false;
    if (!hostPacket.payload)
        throw std::invalid_argument("Nack::Parse requires a payload");
    std::vector<std::uint8_t> payload(hostPacket.payloadSize);
    if (!GuestBytes(hostPacket.payload).CopyIn(payload.data(), payload.size()))
        throw std::invalid_argument("Nack::Parse requires a readable payload");
    const std::size_t nackItems = (payload.size() - 8) / 4;
    std::vector<WebRtcPackedNack> packedItems(nackItems);
    std::vector<std::uint16_t> packetIds;
    packetIds.reserve(nackItems);
    const auto* nextNack = payload.data() + 8;
    for (std::size_t index = 0; index < nackItems; ++index) {
        packedItems[index].firstPid = static_cast<std::uint16_t>(ReadBigEndian(nextNack, 2));
        packedItems[index].bitmask = static_cast<std::uint16_t>(ReadBigEndian(nextNack + 2, 2));
        nextNack += 4;
        packetIds.push_back(packedItems[index].firstPid);
        std::uint16_t pid = static_cast<std::uint16_t>(packedItems[index].firstPid + 1);
        for (std::uint16_t bitmask = packedItems[index].bitmask; bitmask != 0; bitmask >>= 1, ++pid) {
            if (bitmask & 1)
                packetIds.push_back(pid);
        }
    }
    hostNack.senderSsrc = ReadBigEndian(payload.data(), 4);
    hostNack.mediaSsrc = ReadBigEndian(payload.data() + 4, 4);
    WriteVector(hostNack.packed, packedItems.data(), packedItems.size());
    WriteVector(hostNack.packetIds, packetIds.data(), packetIds.size());
    WriteGuest(nack, hostNack, "Nack::Parse requires a writable packet object");
    return true;
}

void APS5_VABI _ZN6webrtc4rtcp4NackC1Ev_nid_postfix(WebRtcRtcpNack* nack) {
    if (!nack || !GuestWritable_nid_no_patch(nack, sizeof(*nack)))
        throw std::invalid_argument("Nack constructor requires a writable packet object");
    WebRtcRtcpNack host{};
    host.vtable = Vtable + 2;
    WriteGuest(nack, host, "Nack constructor requires a writable packet object");
}

void APS5_VABI _ZN6webrtc4rtcp4NackD1Ev_nid_postfix(WebRtcRtcpNack* nack) {
    const auto hostNack = ReadGuest(nack, "Nack destructor requires a packet object");
    if (hostNack.packetIds.begin)
        _ZdlPv_nid_postfix(hostNack.packetIds.begin);
    if (hostNack.packed.begin)
        _ZdlPv_nid_postfix(hostNack.packed.begin);
}

}
