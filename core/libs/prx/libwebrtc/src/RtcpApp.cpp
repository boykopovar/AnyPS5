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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::uint32_t AppBaseLength = 8;
constexpr std::size_t HeaderLength = 4;
constexpr std::uint8_t PacketType = 204;
constexpr int BlockLengthSlot = 2;

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);

std::uint32_t ReadBigEndian(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) | (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

void SetData(WebRtcBuffer& buffer, const std::uint8_t* data, std::size_t size) {
    buffer.size = 0;
    if (size == 0)
        return;
    if (size > buffer.capacity) {
        const std::size_t capacity = std::max<std::size_t>(size, buffer.capacity + buffer.capacity / 2);
        auto* grown = static_cast<std::uint8_t*>(_Znam_nid_postfix(capacity));
        if (buffer.data)
            _ZdaPv_nid_postfix(buffer.data);
        buffer.data = grown;
        buffer.capacity = capacity;
    }
    std::memcpy(buffer.data, data, size);
    buffer.size = size;
}

void WriteBigEndian(std::uint8_t* bytes, std::uint32_t value) {
    bytes[0] = static_cast<std::uint8_t>(value >> 24);
    bytes[1] = static_cast<std::uint8_t>(value >> 16);
    bytes[2] = static_cast<std::uint8_t>(value >> 8);
    bytes[3] = static_cast<std::uint8_t>(value);
}

std::size_t APS5_VABI AppBlockLength_nid_no_patch(const WebRtcRtcpApp* app) {
    return HeaderLength + AppBaseLength + app->data.size;
}

std::size_t BlockLength(const WebRtcRtcpApp* app) {
    using Function = std::size_t (APS5_VABI*)(const WebRtcRtcpApp*);
    return reinterpret_cast<Function>(static_cast<void* const*>(const_cast<void*>(app->vtable))[BlockLengthSlot])(app);
}

bool APS5_VABI AppCreate_nid_no_patch(const WebRtcRtcpApp* app, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    while (*index + BlockLength(app) > maxLength) {
        if (*index == 0)
            return false;
        if (!call)
            throw std::invalid_argument("App::Create needs a callback to send a full buffer");
        call(function, packet, *index);
        *index = 0;
    }
    const std::size_t words = (BlockLength(app) - HeaderLength) / 4;
    packet[*index] = static_cast<std::uint8_t>((2 << 6) | app->subType);
    packet[*index + 1] = PacketType;
    packet[*index + 2] = static_cast<std::uint8_t>((words >> 8) & 0xff);
    packet[*index + 3] = static_cast<std::uint8_t>(words & 0xff);
    *index += HeaderLength;
    WriteBigEndian(packet + *index, app->senderSsrc);
    WriteBigEndian(packet + *index + 4, app->name);
    if (app->data.size != 0)
        std::memcpy(packet + *index + AppBaseLength, app->data.data, app->data.size);
    *index += AppBaseLength + app->data.size;
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
    if (!app)
        throw std::invalid_argument("App constructor requires a packet object");
    app->vtable = Vtable + 2;
    app->senderSsrc = 0;
    app->subType = 0;
    app->name = 0;
    app->data = WebRtcBuffer {};
}

bool APS5_VABI _ZN6webrtc4rtcp3App5ParseERKNS0_12CommonHeaderE_nid_postfix(WebRtcRtcpApp* app, const WebRtcRtcpCommonHeader* packet) {
    if (!app)
        throw std::invalid_argument("App::Parse requires a packet object");
    if (!packet)
        throw std::invalid_argument("App::Parse requires a common header");
    if (packet->payloadSize < AppBaseLength || packet->payloadSize % 4 != 0)
        return false;
    if (!packet->payload)
        throw std::invalid_argument("App::Parse requires a payload");
    app->subType = packet->countOrFormat;
    app->senderSsrc = ReadBigEndian(packet->payload);
    app->name = ReadBigEndian(packet->payload + 4);
    SetData(app->data, packet->payload + AppBaseLength, packet->payloadSize - AppBaseLength);
    return true;
}

void APS5_VABI _ZN6webrtc4rtcp3AppD1Ev_nid_postfix(WebRtcRtcpApp* app) {
    if (!app)
        throw std::invalid_argument("App destructor requires a packet object");
    if (app->data.data)
        _ZdaPv_nid_postfix(app->data.data);
}

}
