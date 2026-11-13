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
#include "prx/libwebrtc/include/ModuleStorage.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr int DeletingDestructorSlot = 1;
constexpr int BlockLengthSlot = 2;
constexpr int CreateSlot = 3;

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using DeletingDestructor = void (APS5_VABI*)(void* packet);
using BlockLengthFunction = std::size_t (APS5_VABI*)(const void* packet);
using CreateFunction = bool (APS5_VABI*)(const void* packet, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, void* function, PacketReady call);

template<typename TFunction>
TFunction Slot(const void* packet, int slot) {
    if (!packet)
        throw std::invalid_argument("CompoundPacket holds a null packet");
    const auto* vtable = *static_cast<void* const* const*>(packet);
    return reinterpret_cast<TFunction>(vtable[slot]);
}

std::size_t APS5_VABI CompoundBlockLength_nid_no_patch(const WebRtcRtcpCompoundPacket* compound) {
    std::size_t length = 0;
    for (void** appended = compound->appendedBegin; appended != compound->appendedEnd; ++appended)
        length += Slot<BlockLengthFunction>(*appended, BlockLengthSlot)(*appended);
    return length;
}

bool APS5_VABI CompoundCreate_nid_no_patch(const WebRtcRtcpCompoundPacket* compound, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    for (void** appended = compound->appendedBegin; appended != compound->appendedEnd; ++appended) {
        if (!Slot<CreateFunction>(*appended, CreateSlot)(*appended, packet, index, maxLength, function, call))
            return false;
    }
    return true;
}

void APS5_VABI DeleteCompound_nid_no_patch(WebRtcRtcpCompoundPacket* compound) {
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(compound);
    _ZdlPv_nid_postfix(compound);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteCompound_nid_no_patch),
    reinterpret_cast<void*>(&CompoundBlockLength_nid_no_patch),
    reinterpret_cast<void*>(&CompoundCreate_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(WebRtcRtcpCompoundPacket* compound) {
    if (!compound)
        throw std::invalid_argument("CompoundPacket constructor requires a packet object");
    compound->vtable = Vtable + 2;
    compound->senderSsrc = 0;
    compound->appendedBegin = nullptr;
    compound->appendedEnd = nullptr;
    compound->appendedCapacity = nullptr;
}

void APS5_VABI _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(WebRtcRtcpCompoundPacket* compound) {
    if (!compound)
        throw std::invalid_argument("CompoundPacket destructor requires a packet object");
    for (void** appended = compound->appendedBegin; appended != compound->appendedEnd; ++appended) {
        if (*appended)
            Slot<DeletingDestructor>(*appended, DeletingDestructorSlot)(*appended);
    }
    const auto capacity = static_cast<std::size_t>(compound->appendedCapacity - compound->appendedBegin) * sizeof(void*);
    WebRtcModuleStorage::Free(compound->appendedBegin, capacity);
    compound->appendedBegin = nullptr;
    compound->appendedEnd = nullptr;
    compound->appendedCapacity = nullptr;
}

}
