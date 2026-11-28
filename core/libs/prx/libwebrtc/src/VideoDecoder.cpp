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
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/GuestPtr.hpp"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t ImplementationNameSlot = 8;
constexpr std::size_t StringCapacityMask = 15;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

std::string ReadGuestCString(const char* source) {
    if (!source)
        throw std::invalid_argument("VideoDecoder::GetDecoderInfo: ImplementationName returned null");
    std::string out;
    ErrnoGuard guard;
    for (std::size_t i = 0;; ++i) {
        char ch = '\0';
        if (!GuestCopyIn_nid_no_patch(&ch, source + i, sizeof(ch)))
            throw std::invalid_argument("VideoDecoder::GetDecoderInfo requires a readable implementation name");
        if (ch == '\0')
            break;
        out.push_back(ch);
    }
    return out;
}

}

extern "C" {

WebRtcVideoDecoderInfo* APS5_VABI _ZNK6webrtc12VideoDecoder14GetDecoderInfoEv_nid_postfix(WebRtcVideoDecoderInfo* info, const WebRtcVideoDecoder* decoder) {
    if (!info)
        throw std::invalid_argument("VideoDecoder::GetDecoderInfo requires a result");
    if (!decoder)
        throw std::invalid_argument("VideoDecoder::GetDecoderInfo requires a decoder");
    using ImplementationName = const char*(APS5_VABI*)(const WebRtcVideoDecoder*);
    const void* const* vtable = nullptr;
    ImplementationName callback = nullptr;
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(info, sizeof(*info)))
            throw std::invalid_argument("VideoDecoder::GetDecoderInfo requires a writable result");
        if (!GuestPtr<const void* const*>(decoder).CopyIn(vtable) || !vtable ||
            !GuestCopyIn_nid_no_patch(&callback, vtable + ImplementationNameSlot, sizeof(callback)) || !callback)
            throw std::invalid_argument("VideoDecoder::GetDecoderInfo requires ImplementationName");
    }
    const std::string hostName = ReadGuestCString(callback(decoder));
    const std::size_t length = hostName.size();
    WebRtcVideoDecoderInfo hostInfo{};
    hostInfo.implementationName.reserved = 0;
    hostInfo.implementationName.size = length;
    hostInfo.implementationName.capacity = WebRtcGuestStringInlineCapacity;
    if (length > WebRtcGuestStringInlineCapacity) {
        hostInfo.implementationName.capacity = length | StringCapacityMask;
        char* data = static_cast<char*>(GuestStringStorage::Allocate(hostInfo.implementationName.capacity + 1));
        std::memcpy(data, hostName.c_str(), length + 1);
        hostInfo.implementationName.pointer = data;
    } else {
        std::memcpy(hostInfo.implementationName.buffer, hostName.c_str(), length + 1);
    }
    hostInfo.isHardwareAccelerated = false;
    {
        ErrnoGuard guard;
        if (!GuestPtr<WebRtcVideoDecoderInfo>(info).CopyOut(hostInfo)) {
            if (hostInfo.implementationName.capacity > WebRtcGuestStringInlineCapacity)
                GuestStringStorage::Deallocate(hostInfo.implementationName.pointer, hostInfo.implementationName.capacity + 1);
            throw std::invalid_argument("VideoDecoder::GetDecoderInfo requires a writable result");
        }
    }
    return info;
}

}
