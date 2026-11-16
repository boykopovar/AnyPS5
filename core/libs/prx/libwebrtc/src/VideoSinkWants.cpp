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
#include "prx/libc/include/GuestPtr.hpp"

#include <cerrno>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

bool ReadableResolutions(const WebRtcVideoSinkFrameSize* begin, std::size_t count) {
    if (count == 0)
        return true;
    if (!begin)
        return false;
    constexpr std::size_t maxChunkElements = 512;
    std::vector<WebRtcVideoSinkFrameSize> chunk(std::min(count, maxChunkElements));
    std::size_t offset = 0;
    ErrnoGuard guard;
    while (offset < count) {
        const std::size_t n = std::min(count - offset, chunk.size());
        if (!GuestPtr<const WebRtcVideoSinkFrameSize>(begin + offset).CopyIn(chunk.data(), n))
            return false;
        offset += n;
    }
    return true;
}

}

extern "C" {

void APS5_VABI _ZN3rtc14VideoSinkWantsC1Ev_nid_postfix(WebRtcVideoSinkWants* wants) {
    if (!wants) throw std::invalid_argument("VideoSinkWants requires an object");
    WebRtcVideoSinkWants host{};
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(wants, sizeof(*wants)) ||
            !GuestPtr<const WebRtcVideoSinkWants>(wants).CopyIn(host))
            throw std::invalid_argument("VideoSinkWants requires a writable object");
    }
    host.rotationApplied = false;
    host.blackFrames = false;
    host.maxPixelCount = std::numeric_limits<std::int32_t>::max();
    host.hasTargetPixelCount = false;
    host.maxFramerateFps = std::numeric_limits<std::int32_t>::max();
    host.resolutionAlignment = 1;
    host.resolutionsBegin = nullptr;
    host.resolutionsEnd = nullptr;
    host.resolutionsCapacity = nullptr;
    host.hasRequestedResolution = false;
    host.isActive = false;
    host.hasAggregates = false;
    ErrnoGuard guard;
    if (!GuestPtr<WebRtcVideoSinkWants>(wants).CopyOut(host))
        throw std::invalid_argument("VideoSinkWants requires a writable object");
}

void APS5_VABI _ZN3rtc14VideoSinkWantsC1ERKS0__nid_postfix(WebRtcVideoSinkWants* wants, const WebRtcVideoSinkWants* other) {
    if (!wants || !other || wants == other) throw std::invalid_argument("VideoSinkWants copy requires distinct objects");
    WebRtcVideoSinkWants hostWants{};
    WebRtcVideoSinkWants hostOther{};
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(wants, sizeof(*wants)) ||
            !GuestPtr<const WebRtcVideoSinkWants>(wants).CopyIn(hostWants) ||
            !GuestPtr<const WebRtcVideoSinkWants>(other).CopyIn(hostOther))
            throw std::invalid_argument("VideoSinkWants copy requires valid objects");
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(hostOther.resolutionsBegin);
    const auto end = reinterpret_cast<std::uintptr_t>(hostOther.resolutionsEnd);
    const auto capacity = reinterpret_cast<std::uintptr_t>(hostOther.resolutionsCapacity);
    if (end < begin || capacity < end || (!begin && capacity) ||
        begin % alignof(WebRtcVideoSinkFrameSize) || (end - begin) % sizeof(WebRtcVideoSinkFrameSize) ||
        (capacity - begin) % sizeof(WebRtcVideoSinkFrameSize))
        throw std::invalid_argument("VideoSinkWants requires a valid resolutions vector");
    const auto count = (end - begin) / sizeof(WebRtcVideoSinkFrameSize);
    if (!ReadableResolutions(hostOther.resolutionsBegin, count))
        throw std::invalid_argument("VideoSinkWants requires a readable resolutions vector");
    auto* resolutions = count ? static_cast<WebRtcVideoSinkFrameSize*>(_Znwm_nid_postfix(end - begin)) : nullptr;
    if (count > 0) {
        std::vector<WebRtcVideoSinkFrameSize> scratch(count);
        bool copied = false;
        {
            ErrnoGuard guard;
            copied = GuestPtr<const WebRtcVideoSinkFrameSize>(hostOther.resolutionsBegin).CopyIn(scratch.data(), count) &&
                     GuestPtr<WebRtcVideoSinkFrameSize>(resolutions).CopyOut(scratch.data(), count);
        }
        if (!copied) {
            _ZdlPv_nid_postfix(resolutions);
            throw std::invalid_argument("VideoSinkWants copy failed while copying resolutions");
        }
    }
    hostWants.rotationApplied = hostOther.rotationApplied;
    hostWants.blackFrames = hostOther.blackFrames;
    hostWants.maxPixelCount = hostOther.maxPixelCount;
    hostWants.hasTargetPixelCount = hostOther.hasTargetPixelCount;
    if (hostOther.hasTargetPixelCount) hostWants.targetPixelCount = hostOther.targetPixelCount;
    hostWants.maxFramerateFps = hostOther.maxFramerateFps;
    hostWants.resolutionAlignment = hostOther.resolutionAlignment;
    hostWants.resolutionsBegin = resolutions;
    hostWants.resolutionsEnd = count ? resolutions + count : nullptr;
    hostWants.resolutionsCapacity = hostWants.resolutionsEnd;
    hostWants.hasRequestedResolution = hostOther.hasRequestedResolution;
    if (hostOther.hasRequestedResolution) hostWants.requestedResolution = hostOther.requestedResolution;
    hostWants.isActive = hostOther.isActive;
    hostWants.hasAggregates = hostOther.hasAggregates;
    if (hostOther.hasAggregates) hostWants.anyActiveWithoutRequestedResolution = hostOther.anyActiveWithoutRequestedResolution;
    ErrnoGuard guard;
    if (!GuestPtr<WebRtcVideoSinkWants>(wants).CopyOut(hostWants)) {
        if (resolutions)
            _ZdlPv_nid_postfix(resolutions);
        throw std::invalid_argument("VideoSinkWants copy requires a writable destination");
    }
}

void APS5_VABI _ZN3rtc14VideoSinkWantsD1Ev_nid_postfix(WebRtcVideoSinkWants* wants) {
    if (!wants) throw std::invalid_argument("VideoSinkWants destructor requires an object");
    WebRtcVideoSinkWants hostWants{};
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(wants, sizeof(*wants)) ||
            !GuestPtr<const WebRtcVideoSinkWants>(wants).CopyIn(hostWants))
            throw std::invalid_argument("VideoSinkWants destructor requires a valid object");
    }
    _ZdlPv_nid_postfix(hostWants.resolutionsBegin);
}

}
