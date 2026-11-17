/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/include/GuestCpu.hpp"

#include <cstdint>

namespace {

bool HasAvx2(unsigned int leaf1Ecx) {
    if (static_cast<int>(GuestCpu::Cpuid(0, 0)[0]) < 7)
        return false;
    if ((leaf1Ecx & 0x10000000u) == 0 || (leaf1Ecx & 0x04000000u) == 0 || (leaf1Ecx & 0x08000000u) == 0)
        return false;
    if ((GuestCpu::Xgetbv(0) & 0x6u) != 0x6u)
        return false;
    const auto leaf7 = GuestCpu::Cpuid(7, 0);
    return (leaf7[1] & 0x00000020u) != 0 && (leaf7[1] & 0x00000100u) != 0;
}

}

extern "C" {

WebRtcAvailableCpuFeatures APS5_VABI _ZN6webrtc23GetAvailableCpuFeaturesEv_nid_postfix() {
    const auto leaf1 = GuestCpu::Cpuid(1, 0);
    return {(leaf1[3] & 0x04000000u) != 0, HasAvx2(leaf1[2]), false};
}

}
