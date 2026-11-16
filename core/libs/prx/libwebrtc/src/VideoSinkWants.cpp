/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"

#include <limits>
#include <stdexcept>

extern "C" {

void APS5_VABI _ZN3rtc14VideoSinkWantsC1Ev_nid_postfix(WebRtcVideoSinkWants* wants) {
    if (!wants) throw std::invalid_argument("VideoSinkWants requires an object");
    wants->rotationApplied = false;
    wants->blackFrames = false;
    wants->maxPixelCount = std::numeric_limits<std::int32_t>::max();
    wants->hasTargetPixelCount = false;
    wants->maxFramerateFps = std::numeric_limits<std::int32_t>::max();
    wants->resolutionAlignment = 1;
    wants->resolutionsBegin = nullptr;
    wants->resolutionsEnd = nullptr;
    wants->resolutionsCapacity = nullptr;
    wants->hasRequestedResolution = false;
    wants->isActive = false;
    wants->hasAggregates = false;
}

void APS5_VABI _ZN3rtc14VideoSinkWantsC1ERKS0__nid_postfix(WebRtcVideoSinkWants* wants, const WebRtcVideoSinkWants* other) {
    if (!wants || !other || wants == other) throw std::invalid_argument("VideoSinkWants copy requires distinct objects");
    const auto begin = reinterpret_cast<std::uintptr_t>(other->resolutionsBegin);
    const auto end = reinterpret_cast<std::uintptr_t>(other->resolutionsEnd);
    const auto capacity = reinterpret_cast<std::uintptr_t>(other->resolutionsCapacity);
    if (end < begin || capacity < end || (!begin && capacity) ||
        begin % alignof(WebRtcVideoSinkFrameSize) || (end - begin) % sizeof(WebRtcVideoSinkFrameSize) ||
        (capacity - begin) % sizeof(WebRtcVideoSinkFrameSize))
        throw std::invalid_argument("VideoSinkWants requires a valid resolutions vector");
    const auto count = (end - begin) / sizeof(WebRtcVideoSinkFrameSize);
    auto* resolutions = count ? static_cast<WebRtcVideoSinkFrameSize*>(_Znwm_nid_postfix(end - begin)) : nullptr;
    for (std::size_t i = 0; i < count; ++i) {
        resolutions[i].width = other->resolutionsBegin[i].width;
        resolutions[i].height = other->resolutionsBegin[i].height;
    }
    wants->rotationApplied = other->rotationApplied;
    wants->blackFrames = other->blackFrames;
    wants->maxPixelCount = other->maxPixelCount;
    wants->hasTargetPixelCount = other->hasTargetPixelCount;
    if (other->hasTargetPixelCount) wants->targetPixelCount = other->targetPixelCount;
    wants->maxFramerateFps = other->maxFramerateFps;
    wants->resolutionAlignment = other->resolutionAlignment;
    wants->resolutionsBegin = resolutions;
    wants->resolutionsEnd = count ? resolutions + count : nullptr;
    wants->resolutionsCapacity = wants->resolutionsEnd;
    wants->hasRequestedResolution = other->hasRequestedResolution;
    if (other->hasRequestedResolution) wants->requestedResolution = other->requestedResolution;
    wants->isActive = other->isActive;
    wants->hasAggregates = other->hasAggregates;
    if (other->hasAggregates) wants->anyActiveWithoutRequestedResolution = other->anyActiveWithoutRequestedResolution;
}

void APS5_VABI _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(WebRtcVideoSinkWants* wants) {
    if (!wants) throw std::invalid_argument("VideoSinkWants destructor requires an object");
    _ZdlPv_nid_postfix(wants->resolutionsBegin);
}

}
