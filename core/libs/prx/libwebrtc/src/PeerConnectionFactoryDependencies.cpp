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

constexpr std::size_t DefaultDeletingDestructorSlot = 1;
constexpr std::size_t SingleMethodDeletingDestructorSlot = 2;
constexpr std::size_t VideoEncoderFactoryDeletingDestructorSlot = 6;
constexpr std::size_t ReleaseSlot = 1;

using DeletingDestructor = void(APS5_VABI*)(void*);
using ReleaseFunction = std::int32_t(APS5_VABI*)(const void*);

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void CopyInChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!source)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

void CopyOutChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!destination)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

void RequireWritable(void* address, std::size_t bytes, const char* message) {
    if (!address)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(address, bytes))
        throw std::invalid_argument(message);
}

bool RangesOverlap(const void* first, const void* second, std::size_t size) {
    const auto a = reinterpret_cast<std::uintptr_t>(first);
    const auto b = reinterpret_cast<std::uintptr_t>(second);
    return a <= b ? (b - a) < size : (a - b) < size;
}

void* ResolveVTableSlot(const void* object, std::size_t slotIndex, const char* message) {
    const void* const* vtable = nullptr;
    CopyInChecked(&vtable, object, sizeof(vtable), message);
    if (!vtable)
        throw std::invalid_argument(message);
    void* slot = nullptr;
    CopyInChecked(&slot, vtable + slotIndex, sizeof(slot), message);
    if (!slot)
        throw std::invalid_argument(message);
    return slot;
}

enum class CleanupKind {
    DeletingDestructor,
    Release,
};

struct ManagedField {
    void* WebRtcPeerConnectionFactoryDependencies::* member;
    std::size_t offset;
    std::size_t slotIndex;
    CleanupKind kind;
};

constexpr std::array<ManagedField, 24> ManagedFieldsReverseOrder = {{
    {&WebRtcPeerConnectionFactoryDependencies::mediaFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, mediaFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::videoDecoderFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, videoDecoderFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::videoEncoderFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, videoEncoderFactory), VideoEncoderFactoryDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::audioFrameProcessor, offsetof(WebRtcPeerConnectionFactoryDependencies, audioFrameProcessor), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::audioProcessingBuilder, offsetof(WebRtcPeerConnectionFactoryDependencies, audioProcessingBuilder), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::audioProcessing, offsetof(WebRtcPeerConnectionFactoryDependencies, audioProcessing), ReleaseSlot, CleanupKind::Release},
    {&WebRtcPeerConnectionFactoryDependencies::audioMixer, offsetof(WebRtcPeerConnectionFactoryDependencies, audioMixer), ReleaseSlot, CleanupKind::Release},
    {&WebRtcPeerConnectionFactoryDependencies::audioDecoderFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, audioDecoderFactory), ReleaseSlot, CleanupKind::Release},
    {&WebRtcPeerConnectionFactoryDependencies::audioEncoderFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, audioEncoderFactory), ReleaseSlot, CleanupKind::Release},
    {&WebRtcPeerConnectionFactoryDependencies::adm, offsetof(WebRtcPeerConnectionFactoryDependencies, adm), ReleaseSlot, CleanupKind::Release},
    {&WebRtcPeerConnectionFactoryDependencies::encodeMetronome, offsetof(WebRtcPeerConnectionFactoryDependencies, encodeMetronome), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::decodeMetronome, offsetof(WebRtcPeerConnectionFactoryDependencies, decodeMetronome), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::transportControllerSendFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, transportControllerSendFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::trials, offsetof(WebRtcPeerConnectionFactoryDependencies, trials), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::sctpFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, sctpFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::netEqFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, netEqFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::networkMonitorFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, networkMonitorFactory), SingleMethodDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::networkManager, offsetof(WebRtcPeerConnectionFactoryDependencies, networkManager), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::networkControllerFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, networkControllerFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::networkStatePredictorFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, networkStatePredictorFactory), SingleMethodDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::fecControllerFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, fecControllerFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::eventLogFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, eventLogFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::taskQueueFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, taskQueueFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
    {&WebRtcPeerConnectionFactoryDependencies::packetSocketFactory, offsetof(WebRtcPeerConnectionFactoryDependencies, packetSocketFactory), DefaultDeletingDestructorSlot, CleanupKind::DeletingDestructor},
}};

}

extern "C" {

void APS5_VABI _ZN6webrtc33PeerConnectionFactoryDependenciesC1Ev_nid_postfix(
    WebRtcPeerConnectionFactoryDependencies* dependencies) {
    constexpr const char* message = "PeerConnectionFactoryDependencies constructor requires a writable object";
    WebRtcPeerConnectionFactoryDependencies staged{};
    CopyOutChecked(dependencies, &staged, sizeof(staged), message);
}

void APS5_VABI _ZN6webrtc33PeerConnectionFactoryDependenciesC1EOS0__nid_postfix(
    WebRtcPeerConnectionFactoryDependencies* dependencies, WebRtcPeerConnectionFactoryDependencies* other) {
    constexpr const char* message = "PeerConnectionFactoryDependencies move constructor requires valid writable objects";
    if (!dependencies || !other || RangesOverlap(dependencies, other, sizeof(*dependencies)))
        throw std::invalid_argument(message);
    WebRtcPeerConnectionFactoryDependencies moved{};
    CopyInChecked(&moved, other, sizeof(moved), message);
    RequireWritable(dependencies, sizeof(*dependencies), message);
    RequireWritable(other, sizeof(*other), message);
    WebRtcPeerConnectionFactoryDependencies cleared{};
    cleared.networkThread = moved.networkThread;
    cleared.workerThread = moved.workerThread;
    cleared.signalingThread = moved.signalingThread;
    cleared.socketFactory = moved.socketFactory;
    CopyOutChecked(dependencies, &moved, sizeof(moved), message);
    CopyOutChecked(other, &cleared, sizeof(cleared), message);
}

void APS5_VABI _ZN6webrtc33PeerConnectionFactoryDependenciesD1Ev_nid_postfix(
    WebRtcPeerConnectionFactoryDependencies* dependencies) {
    constexpr const char* message = "PeerConnectionFactoryDependencies destructor requires a valid object";
    WebRtcPeerConnectionFactoryDependencies local{};
    CopyInChecked(&local, dependencies, sizeof(local), message);
    RequireWritable(dependencies, sizeof(*dependencies), message);
    std::array<void*, ManagedFieldsReverseOrder.size()> cleanupSlots{};
    for (std::size_t i = 0; i < ManagedFieldsReverseOrder.size(); ++i) {
        void* const object = local.*(ManagedFieldsReverseOrder[i].member);
        if (object)
            cleanupSlots[i] = ResolveVTableSlot(object, ManagedFieldsReverseOrder[i].slotIndex, message);
    }
    auto* const base = reinterpret_cast<std::uint8_t*>(dependencies);
    for (std::size_t i = 0; i < ManagedFieldsReverseOrder.size(); ++i) {
        void* const object = local.*(ManagedFieldsReverseOrder[i].member);
        if (!object)
            continue;
        void* const nullPointer = nullptr;
        CopyOutChecked(base + ManagedFieldsReverseOrder[i].offset, &nullPointer, sizeof(nullPointer), message);
        if (ManagedFieldsReverseOrder[i].kind == CleanupKind::Release)
            reinterpret_cast<ReleaseFunction>(cleanupSlots[i])(object);
        else
            reinterpret_cast<DeletingDestructor>(cleanupSlots[i])(object);
    }
}

}
