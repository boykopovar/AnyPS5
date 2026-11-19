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

#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {

bool APS5_VABI _ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(WebRtcRtcpCommonHeader* header, const std::uint8_t* buffer, std::size_t size) {
    if (!header)
        throw std::invalid_argument("CommonHeader::Parse requires a header");
    if (size < WebRtcRtcpCommonHeaderSize)
        return false;
    if (!buffer)
        throw std::invalid_argument("CommonHeader::Parse requires a buffer");
    if ((buffer[0] >> 6) != 2)
        return false;
    bool hasPadding = (buffer[0] & 0x20) != 0;
    header->countOrFormat = buffer[0] & 0x1F;
    header->packetType = buffer[1];
    header->payloadSize = ((static_cast<std::uint32_t>(buffer[2]) << 8) | buffer[3]) * 4u;
    header->payload = buffer + WebRtcRtcpCommonHeaderSize;
    header->paddingSize = 0;
    if (size < WebRtcRtcpCommonHeaderSize + header->payloadSize)
        return false;
    if (hasPadding) {
        if (header->payloadSize == 0)
            return false;
        header->paddingSize = header->payload[header->payloadSize - 1];
        if (header->paddingSize == 0)
            return false;
        if (header->paddingSize > header->payloadSize)
            return false;
        header->payloadSize -= header->paddingSize;
    }
    return true;
}

}
