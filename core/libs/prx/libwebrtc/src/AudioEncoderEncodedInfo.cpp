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
#include "prx/libwebrtc/include/ModuleStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestPtr.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int CodecTypeOther = 0;

WebRtcAudioEncodedInfo ReadInfo(const WebRtcAudioEncodedInfo* info, const char* message) {
    WebRtcAudioEncodedInfo host{};
    if (!GuestPtr<const WebRtcAudioEncodedInfo>(info).CopyIn(host))
        throw std::invalid_argument(message);
    const auto begin = reinterpret_cast<std::uintptr_t>(host.redundantBegin);
    const auto end = reinterpret_cast<std::uintptr_t>(host.redundantEnd);
    const auto capacity = reinterpret_cast<std::uintptr_t>(host.redundantCapacity);
    if (end < begin || capacity < end)
        throw std::invalid_argument(message);
    return host;
}

WebRtcAudioEncodedInfo ReadWritableInfo(WebRtcAudioEncodedInfo* info, const char* message) {
    if (!info || !GuestWritable_nid_no_patch(info, sizeof(*info)))
        throw std::invalid_argument(message);
    return ReadInfo(info, message);
}

void WriteInfo(WebRtcAudioEncodedInfo* info, const WebRtcAudioEncodedInfo& host, const char* message) {
    if (!GuestPtr<WebRtcAudioEncodedInfo>(info).CopyOut(host))
        throw std::invalid_argument(message);
}

std::size_t RedundantCount(const WebRtcAudioEncodedInfo& info) {
    return static_cast<std::size_t>(info.redundantEnd - info.redundantBegin);
}

std::size_t RedundantCapacity(const WebRtcAudioEncodedInfo& info) {
    return static_cast<std::size_t>(info.redundantCapacity - info.redundantBegin);
}

void FreeRedundant(WebRtcAudioEncodedInfo& info) {
    WebRtcModuleStorage::Free(info.redundantBegin, RedundantCapacity(info) * sizeof(WebRtcAudioEncodedInfoLeaf));
    info.redundantBegin = nullptr;
    info.redundantEnd = nullptr;
    info.redundantCapacity = nullptr;
}

void AllocateRedundant(WebRtcAudioEncodedInfo& info, std::size_t count) {
    auto* leaves = static_cast<WebRtcAudioEncodedInfoLeaf*>(WebRtcModuleStorage::Allocate(count * sizeof(WebRtcAudioEncodedInfoLeaf)));
    info.redundantBegin = leaves;
    info.redundantEnd = leaves;
    info.redundantCapacity = leaves + count;
}

constexpr std::size_t ChunkLeaves = 4096 / sizeof(WebRtcAudioEncodedInfoLeaf);

void CheckRedundantReadable(const WebRtcAudioEncodedInfo& info, const char* message) {
    WebRtcAudioEncodedInfoLeaf chunk[ChunkLeaves];
    const std::size_t count = RedundantCount(info);
    for (std::size_t done = 0; done < count; done += ChunkLeaves) {
        if (!GuestPtr<const WebRtcAudioEncodedInfoLeaf>(info.redundantBegin + done).CopyIn(chunk, std::min(count - done, ChunkLeaves)))
            throw std::invalid_argument(message);
    }
}

bool CopyRedundant(WebRtcAudioEncodedInfoLeaf* target, const WebRtcAudioEncodedInfoLeaf* source, std::size_t count) {
    const bool backward = target > source && target < source + count;
    WebRtcAudioEncodedInfoLeaf chunk[ChunkLeaves];
    for (std::size_t done = 0; done < count;) {
        const std::size_t size = std::min(count - done, ChunkLeaves);
        const std::size_t offset = backward ? count - done - size : done;
        if (!GuestPtr<const WebRtcAudioEncodedInfoLeaf>(source + offset).CopyIn(chunk, size) ||
            !GuestPtr<WebRtcAudioEncodedInfoLeaf>(target + offset).CopyOut(chunk, size))
            return false;
        done += size;
    }
    return true;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoC1Ev_nid_postfix(WebRtcAudioEncodedInfo* info) {
    constexpr const char* message = "EncodedInfo constructor requires an object";
    if (!info || !GuestWritable_nid_no_patch(info, sizeof(*info)))
        throw std::invalid_argument(message);
    WebRtcAudioEncodedInfo host{};
    if (!GuestPtr<const WebRtcAudioEncodedInfo>(info).CopyIn(host))
        throw std::invalid_argument(message);
    host.leaf.encodedBytes = 0;
    host.leaf.encodedTimestamp = 0;
    host.leaf.payloadType = 0;
    host.leaf.sendEvenIfEmpty = false;
    host.leaf.speech = true;
    host.leaf.encoderType = CodecTypeOther;
    host.leaf.unknownAt64 = false;
    host.redundantBegin = nullptr;
    host.redundantEnd = nullptr;
    host.redundantCapacity = nullptr;
    WriteInfo(info, host, message);
}

void APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoC1ERKS1__nid_postfix(WebRtcAudioEncodedInfo* info, const WebRtcAudioEncodedInfo* other) {
    constexpr const char* message = "EncodedInfo copy constructor requires both objects";
    if (!info || !other || !GuestWritable_nid_no_patch(info, sizeof(*info)))
        throw std::invalid_argument(message);
    const WebRtcAudioEncodedInfo source = ReadInfo(other, message);
    CheckRedundantReadable(source, message);
    const std::size_t count = RedundantCount(source);
    WebRtcAudioEncodedInfo host{};
    std::memcpy(&host.leaf, &source.leaf, sizeof(host.leaf));
    if (count) {
        AllocateRedundant(host, count);
        if (!CopyRedundant(host.redundantBegin, source.redundantBegin, count)) {
            FreeRedundant(host);
            throw std::invalid_argument(message);
        }
        host.redundantEnd = host.redundantBegin + count;
    }
    WriteInfo(info, host, message);
}

void APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(WebRtcAudioEncodedInfo* info) {
    constexpr const char* message = "EncodedInfo destructor requires an object";
    WebRtcAudioEncodedInfo host = ReadWritableInfo(info, message);
    FreeRedundant(host);
    WriteInfo(info, host, message);
}

WebRtcAudioEncodedInfo* APS5_VABI _ZN6webrtc12AudioEncoder11EncodedInfoaSERKS1__nid_postfix(WebRtcAudioEncodedInfo* info, const WebRtcAudioEncodedInfo* other) {
    constexpr const char* message = "EncodedInfo assignment requires both objects";
    if (!other)
        throw std::invalid_argument(message);
    WebRtcAudioEncodedInfo host = ReadWritableInfo(info, message);
    if (info == other)
        return info;
    const WebRtcAudioEncodedInfo source = ReadInfo(other, message);
    CheckRedundantReadable(source, message);
    const std::size_t count = RedundantCount(source);
    std::memcpy(&host.leaf, &source.leaf, sizeof(host.leaf));
    if (count > RedundantCapacity(host)) {
        FreeRedundant(host);
        WriteInfo(info, host, message);
        AllocateRedundant(host, count);
    }
    if (!CopyRedundant(host.redundantBegin, source.redundantBegin, count)) {
        WriteInfo(info, host, message);
        throw std::invalid_argument(message);
    }
    host.redundantEnd = host.redundantBegin + count;
    WriteInfo(info, host, message);
    return info;
}

}
