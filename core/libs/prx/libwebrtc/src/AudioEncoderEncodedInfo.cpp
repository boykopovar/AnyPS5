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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>

namespace {

constexpr int CodecTypeOther = 0;
constexpr std::size_t BigAllocationBytes = 0x1000;
constexpr std::size_t BigAllocationPadding = 0x27;
constexpr std::uintptr_t BigAllocationAlignment = 0x20;

std::size_t RedundantCount(const WebRtcAudioEncodedInfo* info) {
    return static_cast<std::size_t>(info->redundantEnd - info->redundantBegin);
}

std::size_t RedundantCapacity(const WebRtcAudioEncodedInfo* info) {
    return static_cast<std::size_t>(info->redundantCapacity - info->redundantBegin);
}

void* AllocateStorage(std::size_t bytes) {
    const bool big = bytes >= BigAllocationBytes;
    void* raw = malloc_nid_postfix(big ? bytes + BigAllocationPadding : bytes);
    if (!raw)
        throw std::bad_alloc();
    if (!big)
        return raw;
    const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + BigAllocationPadding) & ~(BigAllocationAlignment - 1);
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return reinterpret_cast<void*>(aligned);
}

void FreeRedundant(WebRtcAudioEncodedInfo* info) {
    if (info->redundantBegin) {
        void* storage = info->redundantBegin;
        if (RedundantCapacity(info) * sizeof(WebRtcAudioEncodedInfoLeaf) >= BigAllocationBytes)
            storage = static_cast<void**>(storage)[-1];
        free_nid_postfix(storage);
    }
    info->redundantBegin = nullptr;
    info->redundantEnd = nullptr;
    info->redundantCapacity = nullptr;
}

void AllocateRedundant(WebRtcAudioEncodedInfo* info, std::size_t count) {
    auto* leaves = static_cast<WebRtcAudioEncodedInfoLeaf*>(AllocateStorage(count * sizeof(WebRtcAudioEncodedInfoLeaf)));
    info->redundantBegin = leaves;
    info->redundantEnd = leaves;
    info->redundantCapacity = leaves + count;
}

void CopyLeaf(WebRtcAudioEncodedInfo* info, const WebRtcAudioEncodedInfo* other) {
    std::memcpy(&info->leaf, &other->leaf, sizeof(info->leaf));
}

}

extern "C" {

void APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoC1Ev_nid_postfix(WebRtcAudioEncodedInfo* info) {
    if (!info)
        throw std::invalid_argument("EncodedInfo constructor requires an object");
    info->leaf.encodedBytes = 0;
    info->leaf.encodedTimestamp = 0;
    info->leaf.payloadType = 0;
    info->leaf.sendEvenIfEmpty = false;
    info->leaf.speech = true;
    info->leaf.encoderType = CodecTypeOther;
    info->leaf.unknownAt64 = false;
    info->redundantBegin = nullptr;
    info->redundantEnd = nullptr;
    info->redundantCapacity = nullptr;
}

void APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoC1ERKS1__nid_postfix(WebRtcAudioEncodedInfo* info, const WebRtcAudioEncodedInfo* other) {
    if (!info || !other)
        throw std::invalid_argument("EncodedInfo copy constructor requires both objects");
    CopyLeaf(info, other);
    info->redundantBegin = nullptr;
    info->redundantEnd = nullptr;
    info->redundantCapacity = nullptr;
    const std::size_t count = RedundantCount(other);
    if (count == 0)
        return;
    AllocateRedundant(info, count);
    std::memcpy(info->redundantBegin, other->redundantBegin, count * sizeof(WebRtcAudioEncodedInfoLeaf));
    info->redundantEnd = info->redundantBegin + count;
}

void APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(WebRtcAudioEncodedInfo* info) {
    if (!info)
        throw std::invalid_argument("EncodedInfo destructor requires an object");
    FreeRedundant(info);
}

WebRtcAudioEncodedInfo* APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoaSERKS1__nid_postfix(WebRtcAudioEncodedInfo* info, const WebRtcAudioEncodedInfo* other) {
    if (!info || !other)
        throw std::invalid_argument("EncodedInfo assignment requires both objects");
    if (info == other)
        return info;
    CopyLeaf(info, other);
    const std::size_t count = RedundantCount(other);
    if (count > RedundantCapacity(info)) {
        FreeRedundant(info);
        AllocateRedundant(info, count);
    }
    if (count != 0)
        std::memmove(info->redundantBegin, other->redundantBegin, count * sizeof(WebRtcAudioEncodedInfoLeaf));
    info->redundantEnd = info->redundantBegin + count;
    return info;
}

}
