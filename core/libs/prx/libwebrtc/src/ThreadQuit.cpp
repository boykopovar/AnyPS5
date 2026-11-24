/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
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

/* https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/rtc_base/thread.cc */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <limits>

namespace {

constexpr std::uintptr_t StopOffset = 0x64;
constexpr std::uintptr_t SocketServerOffset = 0x68;
constexpr std::uintptr_t WakeUpOffset = 5 * sizeof(void*);

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void* Field(void* pointer, std::uintptr_t offset, const char* caller) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (!pointer || address > std::numeric_limits<std::uintptr_t>::max() - offset)
        NotImplemented_nid_no_patch(caller);
    return reinterpret_cast<void*>(address + offset);
}

template<class TValue>
TValue Read(const void* pointer, const char* caller) {
    TValue value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(value))) NotImplemented_nid_no_patch(caller);
    return value;
}

}

extern "C" {

void APS5_VABI _ZN3rtc6Thread4QuitEv_nid_postfix(void* thread) {
    using WakeUp = void (APS5_VABI*)(void*);
    void* server;
    WakeUp wakeUp;
    {
        ErrnoGuard guard;
        auto* stop = static_cast<std::uint32_t*>(Field(thread, StopOffset, __func__));
        if (reinterpret_cast<std::uintptr_t>(stop) % std::atomic_ref<std::uint32_t>::required_alignment != 0)
            NotImplemented_nid_no_patch(__func__);
        {
            GuestArena::HostWrite access(stop, sizeof(*stop));
            if (!access.Open() || !GuestWritable_nid_no_patch(stop, sizeof(*stop))) NotImplemented_nid_no_patch(__func__);
            std::atomic_ref(*stop).store(1, std::memory_order_release);
        }
        server = Read<void*>(Field(thread, SocketServerOffset, __func__), __func__);
        void* vtable = Read<void*>(Field(server, 0, __func__), __func__);
        wakeUp = Read<WakeUp>(Field(vtable, WakeUpOffset, __func__), __func__);
        if (!wakeUp) NotImplemented_nid_no_patch(__func__);
    }
    wakeUp(server);
}

}
