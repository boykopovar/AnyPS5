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
#include "prx/libc/include/GuestPtr.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

bool CommitAndReturn(WebRtcRtcpCommonHeader* header, const WebRtcRtcpCommonHeader& hostHeader, bool result) {
    if (!GuestPtr<WebRtcRtcpCommonHeader>(header).CopyOut(hostHeader))
        throw std::invalid_argument("CommonHeader::Parse requires a writable header");
    return result;
}

}

extern "C" {

bool APS5_VABI _ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(WebRtcRtcpCommonHeader* header, const std::uint8_t* buffer, std::size_t size) {
    if (!header || !GuestWritable_nid_no_patch(header, sizeof(*header)))
        throw std::invalid_argument("CommonHeader::Parse requires a writable header");
    WebRtcRtcpCommonHeader hostHeader{};
    if (!GuestPtr<WebRtcRtcpCommonHeader>(header).CopyIn(hostHeader))
        throw std::invalid_argument("CommonHeader::Parse requires a header");
    if (size < WebRtcRtcpCommonHeaderSize)
        return false;
    if (!buffer)
        throw std::invalid_argument("CommonHeader::Parse requires a buffer");
    std::array<std::uint8_t, WebRtcRtcpCommonHeaderSize> bytes{};
    if (!GuestBytes(buffer).CopyIn(bytes.data(), bytes.size()))
        throw std::invalid_argument("CommonHeader::Parse requires a readable buffer");
    if ((bytes[0] >> 6) != 2)
        return false;
    bool hasPadding = (bytes[0] & 0x20) != 0;
    hostHeader.countOrFormat = bytes[0] & 0x1F;
    hostHeader.packetType = bytes[1];
    hostHeader.payloadSize = ((static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3]) * 4u;
    hostHeader.payload = buffer + WebRtcRtcpCommonHeaderSize;
    hostHeader.paddingSize = 0;
    if (size < WebRtcRtcpCommonHeaderSize + hostHeader.payloadSize)
        return CommitAndReturn(header, hostHeader, false);
    if (hasPadding) {
        if (hostHeader.payloadSize == 0)
            return CommitAndReturn(header, hostHeader, false);
        std::uint8_t padding = 0;
        if (!GuestBytes(hostHeader.payload + hostHeader.payloadSize - 1).CopyIn(&padding, sizeof(padding)))
            throw std::invalid_argument("CommonHeader::Parse requires readable padding");
        hostHeader.paddingSize = padding;
        if (hostHeader.paddingSize == 0)
            return CommitAndReturn(header, hostHeader, false);
        if (hostHeader.paddingSize > hostHeader.payloadSize)
            return CommitAndReturn(header, hostHeader, false);
        hostHeader.payloadSize -= hostHeader.paddingSize;
    }
    return CommitAndReturn(header, hostHeader, true);
}

}
