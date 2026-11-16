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
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::uint32_t AppBaseLength = 8;
constexpr std::size_t HeaderLength = 4;
constexpr std::uint8_t PacketType = 204;
constexpr int BlockLengthSlot = 2;
constexpr std::size_t MaxRtcpPayloadBytes = 65535u * 4u;

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);

std::uint32_t ReadBigEndian(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) | (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
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

void SetData(WebRtcBuffer& buffer, const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        buffer.size = 0;
        return;
    }
    if (size > buffer.capacity) {
        const std::size_t capacity = std::max<std::size_t>(size, buffer.capacity + buffer.capacity / 2);
        auto* grown = static_cast<std::uint8_t*>(_Znam_nid_postfix(capacity));
        if (!GuestWritable_nid_no_patch(grown, size) || !GuestBytes(grown).CopyOut(data, size)) {
            _ZdaPv_nid_postfix(grown);
            throw std::invalid_argument("App::Parse failed to copy payload data");
        }
        if (buffer.data)
            _ZdaPv_nid_postfix(buffer.data);
        buffer.data = grown;
        buffer.capacity = capacity;
    } else {
        if (!GuestWritable_nid_no_patch(buffer.data, size) || !GuestBytes(buffer.data).CopyOut(data, size))
            throw std::invalid_argument("App::Parse requires a writable destination buffer");
    }
    buffer.size = size;
}

void WriteBigEndian(std::uint8_t* bytes, std::uint32_t value) {
    bytes[0] = static_cast<std::uint8_t>(value >> 24);
    bytes[1] = static_cast<std::uint8_t>(value >> 16);
    bytes[2] = static_cast<std::uint8_t>(value >> 8);
    bytes[3] = static_cast<std::uint8_t>(value);
}

std::size_t APS5_VABI AppBlockLength_nid_no_patch(const WebRtcRtcpApp* app) {
    const auto hostApp = ReadGuest(app, "App::BlockLength requires a packet object");
    return HeaderLength + AppBaseLength + hostApp.data.size;
}

std::size_t BlockLength(const WebRtcRtcpApp* app) {
    const auto hostApp = ReadGuest(app, "App::BlockLength requires a packet object");
    if (!hostApp.vtable)
        throw std::invalid_argument("App::BlockLength requires a vtable");
    const auto* vtable = static_cast<void* const*>(const_cast<void*>(hostApp.vtable));
    const auto slot = ReadGuest(vtable + BlockLengthSlot, "App::BlockLength requires a virtual function");
    if (!slot)
        throw std::invalid_argument("App::BlockLength requires a virtual function");
    using Function = std::size_t (APS5_VABI*)(const WebRtcRtcpApp*);
    return reinterpret_cast<Function>(slot)(app);
}

bool APS5_VABI AppCreate_nid_no_patch(const WebRtcRtcpApp* app, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    if (!app || !packet || !index)
        throw std::invalid_argument("App::Create requires a packet object, buffer, and index");
    const auto hostApp = ReadGuest(app, "App::Create requires a readable packet object");
    std::size_t hostIndex = ReadGuest(index, "App::Create requires a readable index");
    if (!GuestWritable_nid_no_patch(index, sizeof(*index)))
        throw std::invalid_argument("App::Create requires a writable index");
    if (hostIndex > maxLength)
        throw std::invalid_argument("App::Create index exceeds maxLength");
    const std::size_t totalLength = BlockLength(app);
    if (totalLength < HeaderLength + AppBaseLength || totalLength != HeaderLength + AppBaseLength + hostApp.data.size)
        throw std::invalid_argument("App::Create reported an inconsistent block length");
    if (hostApp.data.size > MaxRtcpPayloadBytes - AppBaseLength)
        throw std::invalid_argument("App::Create payload exceeds maximum RTCP length");
    while (totalLength > maxLength - hostIndex) {
        if (hostIndex == 0)
            return false;
        if (!call)
            throw std::invalid_argument("App::Create needs a callback to send a full buffer");
        call(function, packet, hostIndex);
        hostIndex = 0;
        WriteGuest(index, hostIndex, "App::Create requires a writable index");
    }
    if (!GuestWritable_nid_no_patch(packet + hostIndex, totalLength))
        throw std::invalid_argument("App::Create requires a writable destination buffer");
    std::vector<std::uint8_t> encoded(totalLength);
    const std::size_t words = (totalLength - HeaderLength) / 4;
    encoded[0] = static_cast<std::uint8_t>((2 << 6) | hostApp.subType);
    encoded[1] = PacketType;
    encoded[2] = static_cast<std::uint8_t>((words >> 8) & 0xff);
    encoded[3] = static_cast<std::uint8_t>(words & 0xff);
    WriteBigEndian(encoded.data() + HeaderLength, hostApp.senderSsrc);
    WriteBigEndian(encoded.data() + HeaderLength + 4, hostApp.name);
    if (hostApp.data.size != 0 && !GuestBytes(hostApp.data.data).CopyIn(encoded.data() + HeaderLength + AppBaseLength, hostApp.data.size))
        throw std::invalid_argument("App::Create requires readable payload data");
    if (!GuestBytes(packet + hostIndex).CopyOut(encoded.data(), encoded.size()))
        throw std::invalid_argument("App::Create requires a writable destination buffer");
    hostIndex += encoded.size();
    WriteGuest(index, hostIndex, "App::Create requires a writable index");
    return true;
}

void APS5_VABI DeleteApp_nid_no_patch(WebRtcRtcpApp* app) {
    _ZN6webrtc4rtcp3AppD1Ev_nid_postfix(app);
    _ZdlPv_nid_postfix(app);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc4rtcp3AppD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteApp_nid_no_patch),
    reinterpret_cast<void*>(&AppBlockLength_nid_no_patch),
    reinterpret_cast<void*>(&AppCreate_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc4rtcp3AppC1Ev_nid_postfix(WebRtcRtcpApp* app) {
    if (!app || !GuestWritable_nid_no_patch(app, sizeof(*app)))
        throw std::invalid_argument("App constructor requires a writable packet object");
    WebRtcRtcpApp host{};
    host.vtable = Vtable + 2;
    WriteGuest(app, host, "App constructor requires a writable packet object");
}

bool APS5_VABI _ZN6webrtc4rtcp3App5ParseERKNS0_12CommonHeaderE_nid_postfix(WebRtcRtcpApp* app, const WebRtcRtcpCommonHeader* packet) {
    if (!app || !GuestWritable_nid_no_patch(app, sizeof(*app)))
        throw std::invalid_argument("App::Parse requires a writable packet object");
    if (!packet)
        throw std::invalid_argument("App::Parse requires a common header");
    auto hostApp = ReadGuest(app, "App::Parse requires a packet object");
    const auto hostPacket = ReadGuest(packet, "App::Parse requires a readable common header");
    if (hostPacket.payloadSize < AppBaseLength || hostPacket.payloadSize % 4 != 0)
        return false;
    if (hostPacket.payloadSize > MaxRtcpPayloadBytes)
        throw std::invalid_argument("App::Parse payload exceeds maximum RTCP length");
    if (!hostPacket.payload)
        throw std::invalid_argument("App::Parse requires a payload");
    std::vector<std::uint8_t> payload(hostPacket.payloadSize);
    if (!GuestBytes(hostPacket.payload).CopyIn(payload.data(), payload.size()))
        throw std::invalid_argument("App::Parse requires a readable payload");
    hostApp.subType = hostPacket.countOrFormat;
    hostApp.senderSsrc = ReadBigEndian(payload.data());
    hostApp.name = ReadBigEndian(payload.data() + 4);
    SetData(hostApp.data, payload.data() + AppBaseLength, payload.size() - AppBaseLength);
    WriteGuest(app, hostApp, "App::Parse requires a writable packet object");
    return true;
}

void APS5_VABI _ZN6webrtc4rtcp3AppD1Ev_nid_postfix(WebRtcRtcpApp* app) {
    const auto hostApp = ReadGuest(app, "App destructor requires a packet object");
    if (hostApp.data.data)
        _ZdaPv_nid_postfix(hostApp.data.data);
}

}
