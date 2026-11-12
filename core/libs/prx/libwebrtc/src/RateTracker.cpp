/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
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

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

std::int64_t APS5_VABI RateTrackerTime_nid_no_patch(const WebRtcRateTracker*) {
    return _ZN3rtc10TimeMillisEv_nid_postfix();
}

void APS5_VABI DeleteRateTracker_nid_no_patch(WebRtcRateTracker* tracker) {
    _ZN3rtc11RateTrackerD1Ev_nid_postfix(tracker);
    _ZdlPv_nid_postfix(tracker);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3rtc11RateTrackerD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteRateTracker_nid_no_patch), reinterpret_cast<void*>(&RateTrackerTime_nid_no_patch),
};

std::int64_t ReadTime(const WebRtcRateTracker* tracker) {
    using Clock = std::int64_t(APS5_VABI*)(const WebRtcRateTracker*);
    return reinterpret_cast<Clock>(tracker->vtable[2])(tracker);
}

std::size_t NextBucketIndex(const WebRtcRateTracker* tracker, std::size_t bucketIndex) {
    return (bucketIndex + 1u) % (tracker->bucketCount + 1u);
}

void EnsureInitialized(WebRtcRateTracker* tracker) {
    if (tracker->bucketStartTimeMilliseconds == -1) {
        tracker->initializationTimeMilliseconds = ReadTime(tracker);
        tracker->bucketStartTimeMilliseconds = tracker->initializationTimeMilliseconds;
        tracker->currentBucket = 0;
        tracker->sampleBuckets[tracker->currentBucket] = 0;
    }
}

}

extern "C" {

void APS5_VABI _ZN3rtc11RateTrackerC1Elm_nid_postfix(WebRtcRateTracker* tracker, std::int64_t bucketMilliseconds, std::size_t bucketCount) {
    if (bucketMilliseconds <= 0 || bucketCount == 0)
        throw std::invalid_argument("RateTracker requires positive bucket parameters");
    if (bucketCount >= std::numeric_limits<std::size_t>::max() / sizeof(std::int64_t))
        throw std::bad_array_new_length();
    auto* buckets = static_cast<std::int64_t*>(_Znam_nid_postfix((bucketCount + 1) * sizeof(std::int64_t)));
    tracker->vtable = Vtable + 2;
    tracker->bucketMilliseconds = bucketMilliseconds;
    tracker->bucketCount = bucketCount;
    tracker->sampleBuckets = buckets;
    tracker->totalSampleCount = 0;
    tracker->bucketStartTimeMilliseconds = -1;
}

void APS5_VABI _ZN3rtc11RateTrackerD1Ev_nid_postfix(WebRtcRateTracker* tracker) {
    tracker->vtable = Vtable + 2;
    _ZdaPv_nid_postfix(tracker->sampleBuckets);
}

void APS5_VABI _ZN3rtc11RateTracker10AddSamplesEl_nid_postfix(WebRtcRateTracker* tracker, std::int64_t sampleCount) {
    const auto currentTime = ReadTime(tracker);
    EnsureInitialized(tracker);
    for (std::size_t i = 0; i <= tracker->bucketCount &&
         currentTime >= tracker->bucketStartTimeMilliseconds + tracker->bucketMilliseconds; ++i) {
        tracker->bucketStartTimeMilliseconds += tracker->bucketMilliseconds;
        tracker->currentBucket = NextBucketIndex(tracker, tracker->currentBucket);
        tracker->sampleBuckets[tracker->currentBucket] = 0;
    }
    tracker->bucketStartTimeMilliseconds += tracker->bucketMilliseconds *
        ((currentTime - tracker->bucketStartTimeMilliseconds) / tracker->bucketMilliseconds);
    tracker->sampleBuckets[tracker->currentBucket] += sampleCount;
    tracker->totalSampleCount += sampleCount;
}

double APS5_VABI _ZNK3rtc11RateTracker22ComputeRateForIntervalEl_nid_postfix(const WebRtcRateTracker* tracker, std::int64_t intervalMilliseconds) {
    if (tracker->bucketStartTimeMilliseconds == -1) return 0.0;
    const auto currentTime = ReadTime(tracker);
    auto availableInterval = std::min(intervalMilliseconds,
        tracker->bucketMilliseconds * static_cast<std::int64_t>(tracker->bucketCount));
    std::size_t bucketsToSkip;
    std::int64_t millisecondsToSkip;
    if (currentTime > tracker->initializationTimeMilliseconds + availableInterval) {
        const auto timeToSkip = currentTime - tracker->bucketStartTimeMilliseconds +
            static_cast<std::int64_t>(tracker->bucketCount) * tracker->bucketMilliseconds - availableInterval;
        bucketsToSkip = timeToSkip / tracker->bucketMilliseconds;
        millisecondsToSkip = timeToSkip % tracker->bucketMilliseconds;
    } else {
        bucketsToSkip = tracker->bucketCount - tracker->currentBucket;
        millisecondsToSkip = 0;
        availableInterval = currentTime - tracker->initializationTimeMilliseconds;
        if (availableInterval < tracker->bucketMilliseconds) return 0.0;
    }
    if (bucketsToSkip > tracker->bucketCount || availableInterval == 0) return 0.0;
    const auto startBucket = NextBucketIndex(tracker, tracker->currentBucket + bucketsToSkip);
    std::int64_t totalSamples = ((tracker->sampleBuckets[startBucket] *
        (tracker->bucketMilliseconds - millisecondsToSkip)) + (tracker->bucketMilliseconds >> 1)) /
        tracker->bucketMilliseconds;
    for (auto i = NextBucketIndex(tracker, startBucket); i != NextBucketIndex(tracker, tracker->currentBucket);
         i = NextBucketIndex(tracker, i)) {
        totalSamples += tracker->sampleBuckets[i];
    }
    return static_cast<double>(totalSamples * 1000) / static_cast<double>(availableInterval);
}

}
