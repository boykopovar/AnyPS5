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
#include "prx/libc/include/GuestPtr.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <stdexcept>

namespace {

constexpr std::size_t ColorSpaceCopySize = offsetof(WebRtcColorSpace, hasHdrMetadata) + sizeof(bool);

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void CopyColorSpace(WebRtcColorSpace* colorSpace, const WebRtcColorSpace* other, const char* targetMessage, const char* sourceMessage) {
    if (!colorSpace)
        throw std::invalid_argument(targetMessage);
    if (!other)
        throw std::invalid_argument(sourceMessage);
    ErrnoGuard guard;
    if (colorSpace == other) {
        if (!GuestWritable_nid_no_patch(colorSpace, ColorSpaceCopySize))
            throw std::invalid_argument(targetMessage);
        return;
    }
    std::array<std::byte, ColorSpaceCopySize> host{};
    if (!GuestBytes(other).CopyIn(host.data(), host.size()))
        throw std::invalid_argument(sourceMessage);
    if (!GuestBytes(colorSpace).CopyOut(host.data(), host.size()))
        throw std::invalid_argument(targetMessage);
}

}

extern "C" {

void APS5_VABI _ZN6webrtc10ColorSpaceC1ERKS0__nid_postfix(WebRtcColorSpace* colorSpace, const WebRtcColorSpace* other) {
    CopyColorSpace(colorSpace, other, "ColorSpace copy constructor requires a color space", "ColorSpace copy constructor requires a source");
}

WebRtcColorSpace* APS5_VABI _ZN6webrtc10ColorSpaceaSERKS0__nid_postfix(WebRtcColorSpace* colorSpace, const WebRtcColorSpace* other) {
    CopyColorSpace(colorSpace, other, "ColorSpace::operator= requires a color space", "ColorSpace::operator= requires a source");
    return colorSpace;
}

}
