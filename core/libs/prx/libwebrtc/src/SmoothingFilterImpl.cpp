/* Derived from common_audio/smoothing_filter.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
#include "prx/libc/libc.hpp"

namespace {

void UpdateAlpha(WebRtcSmoothingFilterImpl* filter, int timeConstantMilliseconds) {
    filter->alpha = timeConstantMilliseconds == 0 ? 0.0f : expf_nid_postfix(-1.0f / timeConstantMilliseconds);
}

void ExtrapolateLastSample(WebRtcSmoothingFilterImpl* filter, std::int64_t timeMilliseconds) {
    float multiplier = 0.0f;
    if (timeMilliseconds <= filter->initEndTimeMilliseconds) {
        if (filter->initTimeMilliseconds == 0) {
            multiplier = 0.0f;
        } else if (filter->initTimeMilliseconds == 1) {
            multiplier = static_cast<float>(exp_nid_postfix(static_cast<double>(filter->lastStateTimeMilliseconds - timeMilliseconds)));
        } else {
            multiplier = expf_nid_postfix(
                -(powf_nid_postfix(filter->initFactor, static_cast<float>(filter->lastStateTimeMilliseconds - filter->initEndTimeMilliseconds)) -
                  powf_nid_postfix(filter->initFactor, static_cast<float>(timeMilliseconds - filter->initEndTimeMilliseconds))) /
                filter->initConst);
        }
    } else {
        if (filter->lastStateTimeMilliseconds < filter->initEndTimeMilliseconds) ExtrapolateLastSample(filter, filter->initEndTimeMilliseconds);
        multiplier = powf_nid_postfix(filter->alpha, static_cast<float>(timeMilliseconds - filter->lastStateTimeMilliseconds));
    }
    filter->state = multiplier * filter->state + (1.0f - multiplier) * filter->lastSample;
    filter->lastStateTimeMilliseconds = timeMilliseconds;
}

void APS5_VABI DeleteSmoothingFilterImpl_nid_no_patch(WebRtcSmoothingFilterImpl* filter) {
    _ZN6webrtc19SmoothingFilterImplD1Ev_nid_postfix(filter);
    _ZdlPv_nid_postfix(filter);
}

void APS5_VABI AddSample_nid_no_patch(WebRtcSmoothingFilterImpl* filter, float sample) {
    const std::int64_t now = _ZN3rtc10TimeMillisEv_nid_postfix();
    if (!filter->hasInitEndTime) {
        filter->state = filter->lastSample = sample;
        filter->initEndTimeMilliseconds = now + filter->initTimeMilliseconds;
        filter->hasInitEndTime = true;
        filter->lastStateTimeMilliseconds = now;
        return;
    }
    ExtrapolateLastSample(filter, now);
    filter->lastSample = sample;
}

WebRtcOptionalFloat APS5_VABI GetAverage_nid_no_patch(WebRtcSmoothingFilterImpl* filter) {
    WebRtcOptionalFloat average{};
    if (!filter->hasInitEndTime) return average;
    ExtrapolateLastSample(filter, _ZN3rtc10TimeMillisEv_nid_postfix());
    average.storage.value = filter->state;
    average.engaged = true;
    return average;
}

bool APS5_VABI SetTimeConstantMs_nid_no_patch(WebRtcSmoothingFilterImpl* filter, int timeConstantMilliseconds) {
    if (!filter->hasInitEndTime || filter->lastStateTimeMilliseconds < filter->initEndTimeMilliseconds) return false;
    UpdateAlpha(filter, timeConstantMilliseconds);
    return true;
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc19SmoothingFilterImplD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteSmoothingFilterImpl_nid_no_patch),
    reinterpret_cast<void*>(&AddSample_nid_no_patch),
    reinterpret_cast<void*>(&GetAverage_nid_no_patch),
    reinterpret_cast<void*>(&SetTimeConstantMs_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc19SmoothingFilterImplC1Ei_nid_postfix(WebRtcSmoothingFilterImpl* filter, int initTimeMilliseconds) {
    filter->vtable = Vtable + 2;
    filter->initTimeMilliseconds = initTimeMilliseconds;
    filter->initFactor = initTimeMilliseconds == 0 ? 0.0f : powf_nid_postfix(static_cast<float>(initTimeMilliseconds), -1.0f / initTimeMilliseconds);
    filter->initConst = initTimeMilliseconds == 0 ? 0.0f
        : initTimeMilliseconds - powf_nid_postfix(static_cast<float>(initTimeMilliseconds), 1.0f - 1.0f / initTimeMilliseconds);
    filter->hasInitEndTime = false;
    UpdateAlpha(filter, initTimeMilliseconds);
}

void APS5_VABI _ZN6webrtc19SmoothingFilterImplD1Ev_nid_postfix(void*) {}

}
