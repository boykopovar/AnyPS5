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

#include <cstdint>
#include <stdexcept>

namespace {

constexpr int BlockLengthSlot = 2;
constexpr int CreateSlot = 3;

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using BlockLengthFunction = std::size_t (APS5_VABI*)(const void* packet);
using CreateFunction = bool (APS5_VABI*)(const void* packet, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, void* function, PacketReady call);

}

extern "C" {

WebRtcBuffer* APS5_VABI _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(WebRtcBuffer* result, const void* packet) {
    if (!result || !packet)
        throw std::invalid_argument("RtcpPacket::Build requires a result and a packet");
    const auto* vtable = *static_cast<void* const* const*>(packet);
    if (!vtable || !vtable[BlockLengthSlot] || !vtable[CreateSlot])
        throw std::invalid_argument("RtcpPacket::Build requires BlockLength and Create virtual functions");
    const std::size_t size = reinterpret_cast<BlockLengthFunction>(vtable[BlockLengthSlot])(packet);
    auto* data = size != 0 ? static_cast<std::uint8_t*>(_Znam_nid_postfix(size)) : nullptr;
    *result = WebRtcBuffer {size, size, data};
    std::size_t length = 0;
    reinterpret_cast<CreateFunction>(vtable[CreateSlot])(packet, data, &length, size, nullptr, nullptr);
    return result;
}

}
