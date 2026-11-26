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
#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libc/libc.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void RequireWritable(void* address, std::size_t bytes, const char* message) {
    if (!address)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(address, bytes))
        throw std::invalid_argument(message);
}

void CopyInTracker(const WebRtcRateTracker* source, WebRtcRateTracker& out, const char* message) {
    if (!source)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestPtr<const WebRtcRateTracker>(source).CopyIn(out))
        throw std::invalid_argument(message);
}

void CopyOutTracker(WebRtcRateTracker* destination, const WebRtcRateTracker& value, const char* message) {
    if (!destination)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestPtr<WebRtcRateTracker>(destination).CopyOut(value))
        throw std::invalid_argument(message);
}

std::int64_t ReadBucket(const std::int64_t* buckets, std::size_t index, const char* message) {
    if (!buckets)
        throw std::invalid_argument(message);
    std::int64_t value = 0;
    ErrnoGuard guard;
    if (!GuestPtr<const std::int64_t>(buckets + index).CopyIn(value))
        throw std::invalid_argument(message);
    return value;
}

void WriteBucket(std::int64_t* buckets, std::size_t index, std::int64_t value, const char* message) {
    if (!buckets)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestPtr<std::int64_t>(buckets + index).CopyOut(value))
        throw std::invalid_argument(message);
}

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

std::int64_t ReadTime(const WebRtcRateTracker* tracker, const WebRtcRateTracker& host, const char* message) {
    if (!host.vtable)
        throw std::invalid_argument(message);
    void* fnPtr = nullptr;
    {
        ErrnoGuard guard;
        if (!GuestPtr<void* const>(host.vtable + 2).CopyIn(fnPtr))
            throw std::invalid_argument(message);
    }
    if (!fnPtr)
        throw std::invalid_argument(message);
    using Clock = std::int64_t(APS5_VABI*)(const WebRtcRateTracker*);
    return reinterpret_cast<Clock>(fnPtr)(tracker);
}

void ValidateBuckets(const WebRtcRateTracker& host, bool writable, const char* message) {
    if (host.bucketMilliseconds <= 0 || host.bucketCount == 0 || !host.sampleBuckets)
        throw std::invalid_argument(message);
    if (host.bucketCount >= std::numeric_limits<std::size_t>::max() / sizeof(std::int64_t))
        throw std::invalid_argument(message);
    if (host.bucketCount > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() / host.bucketMilliseconds))
        throw std::invalid_argument(message);
    if (host.bucketStartTimeMilliseconds != -1 && host.currentBucket > host.bucketCount)
        throw std::invalid_argument(message);
    const std::size_t bucketBytes = (host.bucketCount + 1u) * sizeof(std::int64_t);
    if (writable)
        RequireWritable(host.sampleBuckets, bucketBytes, message);
    else
        ReadBucket(host.sampleBuckets, 0, message);
}

std::size_t NextBucketIndex(const WebRtcRateTracker& host, std::size_t bucketIndex) {
    return (bucketIndex + 1u) % (host.bucketCount + 1u);
}

void EnsureInitialized(const WebRtcRateTracker* tracker, WebRtcRateTracker& host, const char* message) {
    if (host.bucketStartTimeMilliseconds == -1) {
        const auto now = ReadTime(tracker, host, message);
        WriteBucket(host.sampleBuckets, 0, 0, message);
        host.initializationTimeMilliseconds = now;
        host.bucketStartTimeMilliseconds = now;
        host.currentBucket = 0;
    }
}

}

extern "C" {

void APS5_VABI _ZN3rtc11RateTrackerC1Elm_nid_postfix(WebRtcRateTracker* tracker, std::int64_t bucketMilliseconds, std::size_t bucketCount) {
    constexpr const char* message = "RateTracker requires valid tracker and positive bucket parameters";
    if (bucketMilliseconds <= 0 || bucketCount == 0)
        throw std::invalid_argument("RateTracker requires positive bucket parameters");
    RequireWritable(tracker, sizeof(*tracker), message);
    if (bucketCount >= std::numeric_limits<std::size_t>::max() / sizeof(std::int64_t))
        throw std::bad_array_new_length();
    WebRtcRateTracker host{};
    CopyInTracker(tracker, host, message);
    auto* buckets = static_cast<std::int64_t*>(_Znam_nid_postfix((bucketCount + 1) * sizeof(std::int64_t)));
    host.vtable = Vtable + 2;
    host.bucketMilliseconds = bucketMilliseconds;
    host.bucketCount = bucketCount;
    host.sampleBuckets = buckets;
    host.totalSampleCount = 0;
    host.bucketStartTimeMilliseconds = -1;
    try {
        CopyOutTracker(tracker, host, message);
    } catch (...) {
        _ZdaPv_nid_postfix(buckets);
        throw;
    }
}

void APS5_VABI _ZN3rtc11RateTrackerD1Ev_nid_postfix(WebRtcRateTracker* tracker) {
    constexpr const char* message = "RateTracker destructor requires a valid tracker";
    RequireWritable(tracker, sizeof(*tracker), message);
    WebRtcRateTracker host{};
    CopyInTracker(tracker, host, message);
    if (host.sampleBuckets)
        ReadBucket(host.sampleBuckets, 0, message);
    {
        ErrnoGuard guard;
        if (!GuestPtr<void* const*>(reinterpret_cast<void* const**>(tracker)).CopyOut(Vtable + 2))
            throw std::invalid_argument(message);
    }
    _ZdaPv_nid_postfix(host.sampleBuckets);
}

void APS5_VABI _ZN3rtc11RateTracker10AddSamplesEl_nid_postfix(WebRtcRateTracker* tracker, std::int64_t sampleCount) {
    constexpr const char* message = "RateTracker::AddSamples requires a valid tracker";
    RequireWritable(tracker, sizeof(*tracker), message);
    WebRtcRateTracker host{};
    CopyInTracker(tracker, host, message);
    ValidateBuckets(host, true, message);
    const auto currentTime = ReadTime(tracker, host, message);
    EnsureInitialized(tracker, host, message);
    for (std::size_t i = 0; i <= host.bucketCount &&
         currentTime >= host.bucketStartTimeMilliseconds + host.bucketMilliseconds; ++i) {
        host.bucketStartTimeMilliseconds += host.bucketMilliseconds;
        host.currentBucket = NextBucketIndex(host, host.currentBucket);
        WriteBucket(host.sampleBuckets, host.currentBucket, 0, message);
    }
    host.bucketStartTimeMilliseconds += host.bucketMilliseconds *
        ((currentTime - host.bucketStartTimeMilliseconds) / host.bucketMilliseconds);
    const auto updated = ReadBucket(host.sampleBuckets, host.currentBucket, message) + sampleCount;
    WriteBucket(host.sampleBuckets, host.currentBucket, updated, message);
    host.totalSampleCount += sampleCount;
    CopyOutTracker(tracker, host, message);
}

double APS5_VABI _ZNK3rtc11RateTracker22ComputeRateForIntervalEl_nid_postfix(const WebRtcRateTracker* tracker, std::int64_t intervalMilliseconds) {
    constexpr const char* message = "RateTracker::ComputeRateForInterval requires a valid tracker";
    WebRtcRateTracker host{};
    CopyInTracker(tracker, host, message);
    ValidateBuckets(host, false, message);
    if (host.bucketStartTimeMilliseconds == -1) return 0.0;
    const auto currentTime = ReadTime(tracker, host, message);
    auto availableInterval = std::min(intervalMilliseconds,
        host.bucketMilliseconds * static_cast<std::int64_t>(host.bucketCount));
    std::size_t bucketsToSkip;
    std::int64_t millisecondsToSkip;
    if (currentTime > host.initializationTimeMilliseconds + availableInterval) {
        const auto timeToSkip = currentTime - host.bucketStartTimeMilliseconds +
            static_cast<std::int64_t>(host.bucketCount) * host.bucketMilliseconds - availableInterval;
        bucketsToSkip = timeToSkip / host.bucketMilliseconds;
        millisecondsToSkip = timeToSkip % host.bucketMilliseconds;
    } else {
        bucketsToSkip = host.bucketCount - host.currentBucket;
        millisecondsToSkip = 0;
        availableInterval = currentTime - host.initializationTimeMilliseconds;
        if (availableInterval < host.bucketMilliseconds) return 0.0;
    }
    if (bucketsToSkip > host.bucketCount || availableInterval == 0) return 0.0;
    const auto startBucket = NextBucketIndex(host, host.currentBucket + bucketsToSkip);
    std::int64_t totalSamples = ((ReadBucket(host.sampleBuckets, startBucket, message) *
        (host.bucketMilliseconds - millisecondsToSkip)) + (host.bucketMilliseconds >> 1)) /
        host.bucketMilliseconds;
    for (auto i = NextBucketIndex(host, startBucket); i != NextBucketIndex(host, host.currentBucket);
         i = NextBucketIndex(host, i)) {
        totalSamples += ReadBucket(host.sampleBuckets, i, message);
    }
    return static_cast<double>(totalSamples * 1000) / static_cast<double>(availableInterval);
}

}
