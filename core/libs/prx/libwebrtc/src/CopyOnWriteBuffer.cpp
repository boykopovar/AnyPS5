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
#include "prx/libc/libc.hpp"

#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>

namespace {

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

void ReleaseBuffer(WebRtcRefCountedBuffer* buffer) {
    if (buffer && buffer->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        _ZdaPv_nid_postfix(buffer->data);
        buffer->~WebRtcRefCountedBuffer();
        _ZdlPv_nid_postfix(buffer);
    }
}

}

extern "C" {

void APS5_VABI _ZN3rtc17CopyOnWriteBufferC1Em_nid_postfix(WebRtcCopyOnWriteBuffer* buffer, std::size_t size) {
    if (!buffer) throw std::invalid_argument("CopyOnWriteBuffer requires an object");
    auto* backing = size > 0 ? CreateBuffer(size, size) : nullptr;
    if (backing) backing->refCount.fetch_add(1, std::memory_order_relaxed);
    buffer->buffer = backing;
    buffer->offset = 0;
    buffer->size = size;
}

void APS5_VABI _ZN3rtc17CopyOnWriteBufferC1Emm_nid_postfix(WebRtcCopyOnWriteBuffer* buffer, std::size_t size, std::size_t capacity) {
    if (!buffer) throw std::invalid_argument("CopyOnWriteBuffer requires an object");
    auto* backing = size > 0 || capacity > 0 ? CreateBuffer(size, capacity) : nullptr;
    if (backing) backing->refCount.fetch_add(1, std::memory_order_relaxed);
    buffer->buffer = backing;
    buffer->offset = 0;
    buffer->size = size;
}

void APS5_VABI _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(WebRtcCopyOnWriteBuffer* buffer, const WebRtcCopyOnWriteBuffer* other) {
    if (!buffer || !other) throw std::invalid_argument("CopyOnWriteBuffer copy requires objects");
    buffer->buffer = other->buffer;
    if (buffer->buffer) buffer->buffer->refCount.fetch_add(1, std::memory_order_relaxed);
    buffer->offset = other->offset;
    buffer->size = other->size;
}

void APS5_VABI _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(WebRtcCopyOnWriteBuffer* buffer) {
    if (!buffer) throw std::invalid_argument("CopyOnWriteBuffer destructor requires an object");
    ReleaseBuffer(buffer->buffer);
}

void APS5_VABI _ZN3rtc17CopyOnWriteBuffer24UnshareAndEnsureCapacityEm_nid_postfix(WebRtcCopyOnWriteBuffer* buffer, std::size_t capacity) {
    if (!buffer || !buffer->buffer) throw std::invalid_argument("CopyOnWriteBuffer unshare requires backing storage");
    auto* previous = buffer->buffer;
    if (previous->refCount.load(std::memory_order_acquire) == 1 && capacity <= previous->capacity - buffer->offset)
        return;
    auto* backing = CreateBuffer(buffer->size, capacity);
    if (buffer->size > 0) std::memcpy(backing->data, previous->data + buffer->offset, buffer->size);
    backing->refCount.fetch_add(1, std::memory_order_relaxed);
    ReleaseBuffer(previous);
    buffer->buffer = backing;
    buffer->offset = 0;
}

}
