/* Derived from call/call_config.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
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

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t DeletingDestructorSlot = 1;
constexpr std::size_t ReleaseSlot = 1;
constexpr std::int32_t DefaultStartBitrateBps = 300000;

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

bool RangesOverlap(const void* first, std::size_t firstSize, const void* second, std::size_t secondSize) {
    const auto a = reinterpret_cast<std::uintptr_t>(first);
    const auto b = reinterpret_cast<std::uintptr_t>(second);
    return a <= b ? (b - a) < firstSize : (a - b) < secondSize;
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

std::int32_t* RefCountField(void* storage) {
    auto* const bytes = static_cast<std::uint8_t*>(storage);
    return reinterpret_cast<std::int32_t*>(bytes + offsetof(WebRtcRefCountedBase, refCount));
}

}

extern "C" {

void APS5_VABI _ZN6webrtc10CallConfigC1ERKNS_11EnvironmentEPNS_13TaskQueueBaseE_nid_postfix(
    WebRtcCallConfig* config, const WebRtcEnvironment* env, WebRtcTaskQueueBase* networkTaskQueue) {
    constexpr const char* message = "CallConfig constructor requires valid objects";
    if (!config || !env || RangesOverlap(config, sizeof(*config), env, sizeof(*env)))
        throw std::invalid_argument(message);
    WebRtcEnvironment envCopy{};
    CopyInChecked(&envCopy, env, sizeof(envCopy), message);
    RequireWritable(config, sizeof(*config), message);
    if (envCopy.storage) {
        if (reinterpret_cast<std::uintptr_t>(envCopy.storage) % alignof(std::int32_t) != 0)
            throw std::invalid_argument(message);
        RequireWritable(envCopy.storage, sizeof(WebRtcRefCountedBase), message);
        std::int32_t currentCount = 0;
        CopyInChecked(&currentCount, RefCountField(envCopy.storage), sizeof(currentCount), message);
        if (currentCount < 0 || currentCount == std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument(message);
    }
    WebRtcCallConfig staged{};
    staged.env = envCopy;
    staged.bitrateConfig = {0, DefaultStartBitrateBps, -1};
    staged.networkTaskQueue = networkTaskQueue;
    CopyOutChecked(config, &staged, sizeof(staged), message);
    if (envCopy.storage) {
        std::atomic_ref<std::int32_t> refCount(*RefCountField(envCopy.storage));
        refCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void APS5_VABI _ZN6webrtc10CallConfigD1Ev_nid_postfix(WebRtcCallConfig* config) {
    constexpr const char* message = "CallConfig destructor requires a valid object";
    WebRtcCallConfig local{};
    CopyInChecked(&local, config, sizeof(local), message);
    RequireWritable(config, sizeof(*config), message);

    void* perCallSlot = nullptr;
    if (local.perCallNetworkControllerFactory)
        perCallSlot = ResolveVTableSlot(local.perCallNetworkControllerFactory, DeletingDestructorSlot, message);

    void* audioStateSlot = nullptr;
    if (local.audioState)
        audioStateSlot = ResolveVTableSlot(local.audioState, ReleaseSlot, message);

    void* storageSlot = nullptr;
    if (local.env.storage) {
        if (reinterpret_cast<std::uintptr_t>(local.env.storage) % alignof(std::int32_t) != 0)
            throw std::invalid_argument(message);
        RequireWritable(local.env.storage, sizeof(WebRtcRefCountedBase), message);
        std::int32_t currentCount = 0;
        CopyInChecked(&currentCount, RefCountField(local.env.storage), sizeof(currentCount), message);
        if (currentCount <= 0)
            throw std::invalid_argument(message);
        storageSlot = ResolveVTableSlot(local.env.storage, DeletingDestructorSlot, message);
    }

    auto* const base = reinterpret_cast<std::uint8_t*>(config);
    void* const nullPointer = nullptr;

    if (local.perCallNetworkControllerFactory) {
        CopyOutChecked(base + offsetof(WebRtcCallConfig, perCallNetworkControllerFactory), &nullPointer, sizeof(nullPointer), message);
        reinterpret_cast<DeletingDestructor>(perCallSlot)(local.perCallNetworkControllerFactory);
    }
    if (local.audioState) {
        CopyOutChecked(base + offsetof(WebRtcCallConfig, audioState), &nullPointer, sizeof(nullPointer), message);
        reinterpret_cast<ReleaseFunction>(audioStateSlot)(local.audioState);
    }
    if (local.env.storage) {
        CopyOutChecked(base + offsetof(WebRtcCallConfig, env) + offsetof(WebRtcEnvironment, storage), &nullPointer, sizeof(nullPointer), message);
        std::atomic_ref<std::int32_t> refCount(*RefCountField(local.env.storage));
        if (refCount.fetch_sub(1, std::memory_order_acq_rel) - 1 == 0)
            reinterpret_cast<DeletingDestructor>(storageSlot)(local.env.storage);
    }
}

}
