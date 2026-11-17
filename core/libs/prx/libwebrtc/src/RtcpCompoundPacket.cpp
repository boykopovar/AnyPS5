/* Derived from modules/rtp_rtcp/source/rtcp_packet/compound_packet.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
#include "prx/libc/include/GuestPtr.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr int DeletingDestructorSlot = 1;
constexpr int BlockLengthSlot = 2;
constexpr int CreateSlot = 3;
constexpr std::size_t PacketPointerChunk = 4096 / sizeof(void*);

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using DeletingDestructor = void (APS5_VABI*)(void* packet);
using BlockLengthFunction = std::size_t (APS5_VABI*)(const void* packet);
using CreateFunction = bool (APS5_VABI*)(const void* packet, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, void* function, PacketReady call);

template<typename TFunction>
TFunction Slot(const void* packet, int slot) {
    if (!packet)
        throw std::invalid_argument("CompoundPacket holds a null packet");
    const void* const* vtable = nullptr;
    const void* function = nullptr;
    if (!GuestPtr<const void* const*>(packet).CopyIn(vtable) || !vtable || !GuestPtr<const void*>(vtable + slot).CopyIn(function) || !function)
        throw std::invalid_argument("CompoundPacket holds a packet without a readable vtable");
    return reinterpret_cast<TFunction>(const_cast<void*>(function));
}

WebRtcRtcpCompoundPacket ReadCompound(const WebRtcRtcpCompoundPacket* compound, const char* message) {
    WebRtcRtcpCompoundPacket host{};
    if (!GuestPtr<WebRtcRtcpCompoundPacket>(compound).CopyIn(host))
        throw std::invalid_argument(message);
    const auto begin = reinterpret_cast<std::uintptr_t>(host.appendedBegin);
    const auto end = reinterpret_cast<std::uintptr_t>(host.appendedEnd);
    const auto capacity = reinterpret_cast<std::uintptr_t>(host.appendedCapacity);
    if (end < begin || capacity < end || (!begin && capacity) || (end - begin) % sizeof(void*) || (capacity - begin) % sizeof(void*))
        throw std::invalid_argument(message);
    return host;
}

template<typename TVisit> bool ForEachAppended(const WebRtcRtcpCompoundPacket& compound, TVisit visit) {
    const std::size_t count = static_cast<std::size_t>(compound.appendedEnd - compound.appendedBegin);
    std::array<void*, PacketPointerChunk> chunk{};
    for (std::size_t base = 0; base < count; base += chunk.size()) {
        const std::size_t size = std::min(chunk.size(), count - base);
        if (!GuestPtr<void*>(compound.appendedBegin + base).CopyIn(chunk.data(), size))
            throw std::invalid_argument("CompoundPacket holds an unreadable packet vector");
        for (std::size_t i = 0; i < size; ++i) {
            if (!visit(chunk[i]))
                return false;
        }
    }
    return true;
}

std::size_t APS5_VABI CompoundBlockLength_nid_no_patch(const WebRtcRtcpCompoundPacket* compound) {
    std::size_t length = 0;
    ForEachAppended(ReadCompound(compound, "CompoundPacket::BlockLength requires a valid packet object"), [&](void* appended) {
        length += Slot<BlockLengthFunction>(appended, BlockLengthSlot)(appended);
        return true;
    });
    return length;
}

bool APS5_VABI CompoundCreate_nid_no_patch(const WebRtcRtcpCompoundPacket* compound, std::uint8_t* packet, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    return ForEachAppended(ReadCompound(compound, "CompoundPacket::Create requires a valid packet object"), [&](void* appended) {
        return Slot<CreateFunction>(appended, CreateSlot)(appended, packet, index, maxLength, function, call);
    });
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
    constexpr const char* message = "CompoundPacket constructor requires a packet object";
    WebRtcRtcpCompoundPacket host{};
    if (!compound || !GuestWritable_nid_no_patch(compound, sizeof(*compound)) || !GuestPtr<WebRtcRtcpCompoundPacket>(compound).CopyIn(host))
        throw std::invalid_argument(message);
    host.vtable = Vtable + 2;
    host.senderSsrc = 0;
    host.appendedBegin = nullptr;
    host.appendedEnd = nullptr;
    host.appendedCapacity = nullptr;
    if (!GuestPtr<WebRtcRtcpCompoundPacket>(compound).CopyOut(host))
        throw std::invalid_argument(message);
}

void APS5_VABI _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(WebRtcRtcpCompoundPacket* compound) {
    constexpr const char* message = "CompoundPacket destructor requires a packet object";
    if (!compound || !GuestWritable_nid_no_patch(compound, sizeof(*compound)))
        throw std::invalid_argument(message);
    WebRtcRtcpCompoundPacket host = ReadCompound(compound, message);
    ForEachAppended(host, [](void* appended) {
        if (appended)
            Slot<DeletingDestructor>(appended, DeletingDestructorSlot);
        return true;
    });
    ForEachAppended(host, [](void* appended) {
        if (appended)
            Slot<DeletingDestructor>(appended, DeletingDestructorSlot)(appended);
        return true;
    });
    const auto capacity = static_cast<std::size_t>(host.appendedCapacity - host.appendedBegin) * sizeof(void*);
    WebRtcModuleStorage::Free(host.appendedBegin, capacity);
    host.appendedBegin = nullptr;
    host.appendedEnd = nullptr;
    host.appendedCapacity = nullptr;
    if (!GuestPtr<WebRtcRtcpCompoundPacket>(compound).CopyOut(host))
        throw std::invalid_argument(message);
}

}
