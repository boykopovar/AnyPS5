/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
Copyright (c) 2011, The WebRTC project authors. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * Neither the name of Google nor the names of its contributors may
    be used to endorse or promote products derived from this software
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libc/libc.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

struct RefCountedBufferSnapshot {
    std::size_t size;
    std::size_t capacity;
    std::uint8_t* data;
    int refCount;
};

static_assert(sizeof(RefCountedBufferSnapshot) == sizeof(WebRtcRefCountedBuffer));

void RequireWritableBuffer(WebRtcCopyOnWriteBuffer* buffer, const char* message) {
    if (!buffer)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(buffer, sizeof(*buffer)))
        throw std::invalid_argument(message);
}

void CopyInBuffer(const WebRtcCopyOnWriteBuffer* source, WebRtcCopyOnWriteBuffer& out, const char* message) {
    if (!source)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestPtr<const WebRtcCopyOnWriteBuffer>(source).CopyIn(out))
        throw std::invalid_argument(message);
}

void CopyOutBuffer(WebRtcCopyOnWriteBuffer* destination, const WebRtcCopyOnWriteBuffer& value, const char* message) {
    ErrnoGuard guard;
    if (!GuestPtr<WebRtcCopyOnWriteBuffer>(destination).CopyOut(value))
        throw std::invalid_argument(message);
}

void RequireWritableBacking(WebRtcRefCountedBuffer* backing, const char* message) {
    if (!backing)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(backing, sizeof(*backing)))
        throw std::invalid_argument(message);
}

void ReadBacking(const WebRtcRefCountedBuffer* backing, RefCountedBufferSnapshot& out, const char* message) {
    ErrnoGuard guard;
    if (!GuestBytes(backing).CopyIn(&out, sizeof(out)))
        throw std::invalid_argument(message);
}

std::atomic<int>* BackingRefCount(WebRtcRefCountedBuffer* backing) {
    return reinterpret_cast<std::atomic<int>*>(
        reinterpret_cast<std::uintptr_t>(backing) + offsetof(WebRtcRefCountedBuffer, refCount));
}

WebRtcRefCountedBuffer* CreateBuffer(std::size_t size, std::size_t capacity) {
    auto* storage = _Znwm_nid_postfix(sizeof(WebRtcRefCountedBuffer));
    capacity = std::max(size, capacity);
    std::uint8_t* data;
    try {
        data = capacity > 0 ? static_cast<std::uint8_t*>(_Znam_nid_postfix(capacity)) : nullptr;
    } catch (...) {
        _ZdlPv_nid_postfix(storage);
        throw;
    }
    return new (storage) WebRtcRefCountedBuffer{size, capacity, data, 0};
}

void ReleaseBuffer(WebRtcRefCountedBuffer* buffer, const char* message) {
    if (!buffer)
        return;
    RequireWritableBacking(buffer, message);
    RefCountedBufferSnapshot snap{};
    ReadBacking(buffer, snap, message);
    if (BackingRefCount(buffer)->fetch_sub(1, std::memory_order_acq_rel) == 1) {
        _ZdaPv_nid_postfix(snap.data);
        buffer->~WebRtcRefCountedBuffer();
        _ZdlPv_nid_postfix(buffer);
    }
}

}

extern "C" {

void APS5_VABI _ZN3rtc17CopyOnWriteBufferC1Em_nid_postfix(WebRtcCopyOnWriteBuffer* buffer, std::size_t size) {
    constexpr const char* message = "CopyOnWriteBuffer requires an object";
    RequireWritableBuffer(buffer, message);
    auto* backing = size > 0 ? CreateBuffer(size, size) : nullptr;
    if (backing)
        BackingRefCount(backing)->fetch_add(1, std::memory_order_relaxed);
    CopyOutBuffer(buffer, WebRtcCopyOnWriteBuffer{backing, 0, size}, message);
}

void APS5_VABI _ZN3rtc17CopyOnWriteBufferC1Emm_nid_postfix(WebRtcCopyOnWriteBuffer* buffer, std::size_t size, std::size_t capacity) {
    constexpr const char* message = "CopyOnWriteBuffer requires an object";
    RequireWritableBuffer(buffer, message);
    auto* backing = size > 0 || capacity > 0 ? CreateBuffer(size, capacity) : nullptr;
    if (backing)
        BackingRefCount(backing)->fetch_add(1, std::memory_order_relaxed);
    CopyOutBuffer(buffer, WebRtcCopyOnWriteBuffer{backing, 0, size}, message);
}

void APS5_VABI _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(WebRtcCopyOnWriteBuffer* buffer, const WebRtcCopyOnWriteBuffer* other) {
    constexpr const char* message = "CopyOnWriteBuffer copy requires objects";
    RequireWritableBuffer(buffer, message);
    WebRtcCopyOnWriteBuffer src{};
    CopyInBuffer(other, src, message);
    if (src.buffer) {
        RequireWritableBacking(src.buffer, message);
        BackingRefCount(src.buffer)->fetch_add(1, std::memory_order_relaxed);
    }
    CopyOutBuffer(buffer, src, message);
}

void APS5_VABI _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(WebRtcCopyOnWriteBuffer* buffer) {
    constexpr const char* message = "CopyOnWriteBuffer destructor requires an object";
    WebRtcCopyOnWriteBuffer host{};
    CopyInBuffer(buffer, host, message);
    ReleaseBuffer(host.buffer, message);
}

void DestroyBackingImmediate(WebRtcRefCountedBuffer* backing, std::uint8_t* data) {
    _ZdaPv_nid_postfix(data);
    backing->~WebRtcRefCountedBuffer();
    _ZdlPv_nid_postfix(backing);
}

void APS5_VABI _ZN3rtc17CopyOnWriteBuffer24UnshareAndEnsureCapacityEm_nid_postfix(WebRtcCopyOnWriteBuffer* buffer, std::size_t capacity) {
    constexpr const char* message = "CopyOnWriteBuffer unshare requires backing storage";
    RequireWritableBuffer(buffer, message);
    WebRtcCopyOnWriteBuffer host{};
    CopyInBuffer(buffer, host, message);
    if (!host.buffer)
        throw std::invalid_argument(message);
    RequireWritableBacking(host.buffer, message);
    RefCountedBufferSnapshot prev{};
    ReadBacking(host.buffer, prev, message);
    if (BackingRefCount(host.buffer)->load(std::memory_order_acquire) == 1 &&
        host.offset <= prev.capacity && capacity <= prev.capacity - host.offset)
        return;

    std::uint8_t chunkBuf[4096];
    if (host.size > 0) {
        if (!prev.data || host.offset > prev.capacity || host.size > prev.capacity - host.offset)
            throw std::invalid_argument(message);
        {
            ErrnoGuard guard;
            if (!GuestBytes(prev.data + host.offset).CopyIn(chunkBuf, 1) ||
                !GuestBytes(prev.data + host.offset + host.size - 1).CopyIn(chunkBuf, 1))
                throw std::invalid_argument(message);
        }
        std::size_t probed = 0;
        while (probed < host.size) {
            const std::size_t chunk = std::min<std::size_t>(host.size - probed, sizeof(chunkBuf));
            ErrnoGuard guard;
            if (!GuestBytes(prev.data + host.offset + probed).CopyIn(chunkBuf, chunk))
                throw std::invalid_argument(message);
            probed += chunk;
        }
    }

    auto* backing = CreateBuffer(host.size, capacity);
    if (host.size > 0) {
        std::uint8_t* dstData = nullptr;
        bool readOk = false;
        {
            ErrnoGuard guard;
            readOk = GuestPtr<std::uint8_t* const>(
                reinterpret_cast<const std::uint8_t* const*>(reinterpret_cast<std::uintptr_t>(backing) + offsetof(WebRtcRefCountedBuffer, data)))
                .CopyIn(dstData);
        }
        if (!readOk || !dstData) {
            DestroyBackingImmediate(backing, dstData);
            throw std::invalid_argument(message);
        }
        {
            ErrnoGuard guard;
            if (!GuestWritable_nid_no_patch(dstData, host.size)) {
                DestroyBackingImmediate(backing, dstData);
                throw std::invalid_argument(message);
            }
        }
        std::size_t copied = 0;
        while (copied < host.size) {
            const std::size_t chunk = std::min<std::size_t>(host.size - copied, sizeof(chunkBuf));
            bool chunkOk = false;
            {
                ErrnoGuard guard;
                chunkOk = GuestBytes(prev.data + host.offset + copied).CopyIn(chunkBuf, chunk) &&
                    GuestBytes(dstData + copied).CopyOut(chunkBuf, chunk);
            }
            if (!chunkOk) {
                DestroyBackingImmediate(backing, dstData);
                throw std::invalid_argument(message);
            }
            copied += chunk;
        }
    }
    BackingRefCount(backing)->fetch_add(1, std::memory_order_relaxed);
    ReleaseBuffer(host.buffer, message);
    host.buffer = backing;
    host.offset = 0;
    CopyOutBuffer(buffer, host, message);
}

}
