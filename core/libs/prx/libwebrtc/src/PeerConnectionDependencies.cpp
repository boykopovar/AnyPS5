/* Derived from api/peer_connection_interface.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/include/General.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::size_t DeletingDestructorSlot = 1;

using DeletingDestructor = void(APS5_VABI*)(void*);

void CopyInChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!source)
        throw std::invalid_argument(message);
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

void CopyOutChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!destination)
        throw std::invalid_argument(message);
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

void RequireWritable(void* address, std::size_t bytes, const char* message) {
    if (!address)
        throw std::invalid_argument(message);
    if (!GuestWritable_nid_no_patch(address, bytes))
        throw std::invalid_argument(message);
}

bool RangesOverlap(const void* first, const void* second, std::size_t size) {
    const auto a = reinterpret_cast<std::uintptr_t>(first);
    const auto b = reinterpret_cast<std::uintptr_t>(second);
    return a <= b ? (b - a) < size : (a - b) < size;
}

DeletingDestructor ResolveDeletingDestructor(const void* object, const char* message) {
    const void* const* vtable = nullptr;
    CopyInChecked(&vtable, object, sizeof(vtable), message);
    if (!vtable)
        throw std::invalid_argument(message);
    void* slot = nullptr;
    CopyInChecked(&slot, vtable + DeletingDestructorSlot, sizeof(slot), message);
    if (!slot)
        throw std::invalid_argument(message);
    return reinterpret_cast<DeletingDestructor>(slot);
}

}

extern "C" {

void APS5_VABI _ZN6webrtc26PeerConnectionDependenciesC1EPNS_22PeerConnectionObserverE_nid_postfix(
    WebRtcPeerConnectionDependencies* dependencies, void* observer) {
    constexpr const char* message = "PeerConnectionDependencies constructor requires a writable object";
    WebRtcPeerConnectionDependencies staged{};
    staged.observer = observer;
    CopyOutChecked(dependencies, &staged, sizeof(staged), message);
}

void APS5_VABI _ZN6webrtc26PeerConnectionDependenciesC1EOS0__nid_postfix(
    WebRtcPeerConnectionDependencies* dependencies, WebRtcPeerConnectionDependencies* other) {
    constexpr const char* message = "PeerConnectionDependencies move constructor requires valid writable objects";
    if (!dependencies || !other || RangesOverlap(dependencies, other, sizeof(*dependencies)))
        throw std::invalid_argument(message);
    WebRtcPeerConnectionDependencies moved{};
    CopyInChecked(&moved, other, sizeof(moved), message);
    RequireWritable(dependencies, sizeof(*dependencies), message);
    RequireWritable(other, sizeof(*other), message);
    WebRtcPeerConnectionDependencies cleared{};
    cleared.observer = moved.observer;
    CopyOutChecked(dependencies, &moved, sizeof(moved), message);
    CopyOutChecked(other, &cleared, sizeof(cleared), message);
}

void APS5_VABI _ZN6webrtc26PeerConnectionDependenciesD1Ev_nid_postfix(
    WebRtcPeerConnectionDependencies* dependencies) {
    constexpr const char* message = "PeerConnectionDependencies destructor requires a valid object";
    WebRtcPeerConnectionDependencies local{};
    CopyInChecked(&local, dependencies, sizeof(local), message);
    RequireWritable(dependencies, sizeof(*dependencies), message);
    struct OwnedField {
        void* WebRtcPeerConnectionDependencies::* member;
        std::size_t offset;
    };
    constexpr std::array<OwnedField, 8> ownedFields = {{
        {&WebRtcPeerConnectionDependencies::trials, offsetof(WebRtcPeerConnectionDependencies, trials)},
        {&WebRtcPeerConnectionDependencies::networkControllerFactory, offsetof(WebRtcPeerConnectionDependencies, networkControllerFactory)},
        {&WebRtcPeerConnectionDependencies::videoBitrateAllocatorFactory, offsetof(WebRtcPeerConnectionDependencies, videoBitrateAllocatorFactory)},
        {&WebRtcPeerConnectionDependencies::tlsCertVerifier, offsetof(WebRtcPeerConnectionDependencies, tlsCertVerifier)},
        {&WebRtcPeerConnectionDependencies::certGenerator, offsetof(WebRtcPeerConnectionDependencies, certGenerator)},
        {&WebRtcPeerConnectionDependencies::iceTransportFactory, offsetof(WebRtcPeerConnectionDependencies, iceTransportFactory)},
        {&WebRtcPeerConnectionDependencies::asyncDnsResolverFactory, offsetof(WebRtcPeerConnectionDependencies, asyncDnsResolverFactory)},
        {&WebRtcPeerConnectionDependencies::allocator, offsetof(WebRtcPeerConnectionDependencies, allocator)},
    }};
    std::array<DeletingDestructor, 8> destructors{};
    for (std::size_t i = 0; i < ownedFields.size(); ++i) {
        void* const object = local.*(ownedFields[i].member);
        if (object)
            destructors[i] = ResolveDeletingDestructor(object, message);
    }
    auto* const base = reinterpret_cast<std::uint8_t*>(dependencies);
    for (std::size_t i = 0; i < ownedFields.size(); ++i) {
        void* const object = local.*(ownedFields[i].member);
        if (!object)
            continue;
        void* const nullPointer = nullptr;
        CopyOutChecked(base + ownedFields[i].offset, &nullPointer, sizeof(nullPointer), message);
        destructors[i](object);
    }
}

}
