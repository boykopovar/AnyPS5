/*
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
#include "prx/libc/include/GuestPtr.hpp"

#include <cmath>
#include <stdexcept>

extern "C" {

void APS5_VABI _ZN3rtc9ExpFilter5ResetEf_nid_postfix(WebRtcExpFilter* filter, float alpha) {
    if (!filter || !GuestWritable_nid_no_patch(filter, sizeof(*filter)))
        throw std::invalid_argument("ExpFilter::Reset requires a filter");
    WebRtcExpFilter host{};
    if (!GuestPtr<WebRtcExpFilter>(filter).CopyIn(host))
        throw std::invalid_argument("ExpFilter::Reset requires a filter");
    host.alpha = alpha;
    host.filtered = WebRtcExpFilterValueUndefined;
    if (!GuestPtr<WebRtcExpFilter>(filter).CopyOut(host))
        throw std::invalid_argument("ExpFilter::Reset requires a filter");
}

float APS5_VABI _ZN3rtc9ExpFilter5ApplyEff_nid_postfix(WebRtcExpFilter* filter, float exponent, float sample) {
    if (!filter || !GuestWritable_nid_no_patch(filter, sizeof(*filter)))
        throw std::invalid_argument("ExpFilter::Apply requires a filter");
    WebRtcExpFilter host{};
    if (!GuestPtr<WebRtcExpFilter>(filter).CopyIn(host))
        throw std::invalid_argument("ExpFilter::Apply requires a filter");
    if (host.filtered == WebRtcExpFilterValueUndefined) {
        host.filtered = sample;
    } else if (exponent == 1.0f) {
        host.filtered = host.alpha * host.filtered + (1 - host.alpha) * sample;
    } else {
        float alpha = std::pow(host.alpha, exponent);
        host.filtered = alpha * host.filtered + (1 - alpha) * sample;
    }
    if (host.max != WebRtcExpFilterValueUndefined && host.filtered > host.max)
        host.filtered = host.max;
    if (!GuestPtr<WebRtcExpFilter>(filter).CopyOut(host))
        throw std::invalid_argument("ExpFilter::Apply requires a filter");
    return host.filtered;
}

}
