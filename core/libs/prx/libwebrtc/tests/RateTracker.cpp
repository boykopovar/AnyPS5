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
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RateTracker check failed at line %u\n", location.line());
    std::abort();
}

void* lastAllocated;
void* lastFreed;
std::size_t lastSize;
void* APS5_VABI RateTrackerAllocate_nid_no_patch(std::size_t size) {
    lastSize = size;
    return lastAllocated = std::malloc(size);
}
void APS5_VABI RateTrackerFree_nid_no_patch(void* pointer) {
    lastFreed = pointer;
    std::free(pointer);
}
void* APS5_VABI RateTrackerCalloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI RateTrackerReallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI RateTrackerAlign_nid_no_patch(std::size_t, std::size_t size) { return RateTrackerAllocate_nid_no_patch(size); }
void* APS5_VABI RateTrackerRealign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return RateTrackerReallocate_nid_no_patch(pointer, size); }
int APS5_VABI RateTrackerPosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = RateTrackerAlign_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

struct Tracker {
    WebRtcRateTracker object;
    std::int64_t time = 0;
    static std::int64_t APS5_VABI RateTrackerTime_nid_no_patch(const WebRtcRateTracker* object) {
        return reinterpret_cast<const Tracker*>(object)->time;
    }
    explicit Tracker(std::int64_t bucketMilliseconds = 100, std::size_t bucketCount = 10) {
        _ZN3rtc11RateTrackerC1Elm_nid_postfix(&object, bucketMilliseconds, bucketCount);
        static void* const table[] = {nullptr, nullptr, reinterpret_cast<void*>(&RateTrackerTime_nid_no_patch)};
        object.vtable = table;
    }
    ~Tracker() { _ZN3rtc11RateTrackerD1Ev_nid_postfix(&object); }
    void AddSamples(std::int64_t count) { _ZN3rtc11RateTracker10AddSamplesEl_nid_postfix(&object, count); }
    void AdvanceTime(int delta) { time += delta; }
    double ComputeRateForInterval(std::int64_t interval) const {
        return _ZNK3rtc11RateTracker22ComputeRateForIntervalEl_nid_postfix(&object, interval);
    }
    double ComputeRate() const { return ComputeRateForInterval(1000); }
};

void Test30FPS() {
    Tracker tracker;

    for (int i = 0; i < 300; ++i) {
        tracker.AddSamples(1);
        tracker.AdvanceTime(33);
        if (i % 3 == 0) {
            tracker.AdvanceTime(1);
        }
    }
    Require((30.0) == (tracker.ComputeRateForInterval(50000)));
}

void Test60FPS() {
    Tracker tracker;

    for (int i = 0; i < 300; ++i) {
        tracker.AddSamples(1);
        tracker.AdvanceTime(16);
        if (i % 3 != 0) {
            tracker.AdvanceTime(1);
        }
    }
    Require((60.0) == (tracker.ComputeRateForInterval(1000)));
}

void TestRateTrackerBasics() {
    Tracker tracker;
    Require((0.0) == (tracker.ComputeRateForInterval(1000)));

    tracker.AddSamples(1234);

    tracker.AdvanceTime(100 - 1);
    Require((0.0) == (tracker.ComputeRate()));

    tracker.AdvanceTime(1);
    Require((12340.0) == (tracker.ComputeRateForInterval(1000)));
    Require((12340.0) == (tracker.ComputeRate()));

    tracker.AddSamples(1234);
    tracker.AdvanceTime(100);
    Require((12340.0) == (tracker.ComputeRateForInterval(1000)));
    Require((12340.0) == (tracker.ComputeRate()));

    tracker.AdvanceTime(800);
    Require((1234.0 * 2.0) == (tracker.ComputeRateForInterval(1000)));
    Require((1234.0 * 2.0) == (tracker.ComputeRate()));

    Require((1234.0 * 2.0) == (tracker.ComputeRateForInterval(1000)));
    Require((1234.0 * 2.0) == (tracker.ComputeRate()));

    tracker.AdvanceTime(1000);
    Require((0.0) == (tracker.ComputeRateForInterval(1000)));
    Require((0.0) == (tracker.ComputeRate()));

    for (int i = 0; i < 5500; i += 100) {
        tracker.AddSamples(9876U);
        tracker.AdvanceTime(100);
    }
    Require((9876.0 * 10.0) == (tracker.ComputeRateForInterval(1000)));
    Require((9876.0 * 10.0) == (tracker.ComputeRate()));

    tracker.AdvanceTime(500);
    Require((9876.0 * 5.0) == (tracker.ComputeRateForInterval(1000)));
    Require((9876.0 * 5.0) == (tracker.ComputeRate()));

    Require((0.0) == (tracker.ComputeRateForInterval(500)));
}

void TestLongPeriodBetweenSamples() {
    Tracker tracker;
    tracker.AddSamples(1);
    tracker.AdvanceTime(1000);
    Require((1.0) == (tracker.ComputeRate()));

    tracker.AdvanceTime(2000);
    Require((0.0) == (tracker.ComputeRate()));

    tracker.AdvanceTime(2000);
    tracker.AddSamples(1);
    Require((1.0) == (tracker.ComputeRate()));
}

void TestRolloff() {
    Tracker tracker;
    for (int i = 0; i < 10; ++i) {
        tracker.AddSamples(1U);
        tracker.AdvanceTime(100);
    }
    Require((10.0) == (tracker.ComputeRate()));

    for (int i = 0; i < 10; ++i) {
        tracker.AddSamples(1U);
        tracker.AdvanceTime(50);
    }
    Require((15.0) == (tracker.ComputeRate()));
    Require((20.0) == (tracker.ComputeRateForInterval(500)));

    for (int i = 0; i < 10; ++i) {
        tracker.AddSamples(1U);
        tracker.AdvanceTime(50);
    }
    Require((20.0) == (tracker.ComputeRate()));
}

void TestGetUnitSecondsAfterInitialValue() {
    Tracker tracker;
    tracker.AddSamples(1234);
    tracker.AdvanceTime(1000);
    Require((1234.0) == (tracker.ComputeRateForInterval(1000)));
}

void TestLargeNumbers() {
    Tracker tracker;
    const std::uint64_t largeNumber = 0x100000000;
    tracker.AddSamples(largeNumber);
    tracker.AdvanceTime(1000);
    tracker.AddSamples(largeNumber);
    Require((largeNumber * 2) == (tracker.ComputeRate()));
}

void TestAbi() {
    struct Guarded {
        std::array<unsigned char, 16> before;
        WebRtcRateTracker object;
        std::array<unsigned char, 16> after;
    } guarded;
    std::memset(&guarded, 0x5a, sizeof(guarded));
    _ZN3rtc11RateTrackerC1Elm_nid_postfix(&guarded.object, 100, 10);
    Require(lastSize == 88 && lastAllocated == guarded.object.sampleBuckets);
    Require(guarded.object.totalSampleCount == 0);
    Require(guarded.object.currentBucket == 0x5a5a5a5a5a5a5a5aULL);
    Require(guarded.object.initializationTimeMilliseconds == 0x5a5a5a5a5a5a5a5aLL);
    const auto before = _ZN3rtc10TimeMillisEv_nid_postfix();
    using Clock = std::int64_t(APS5_VABI*)(const WebRtcRateTracker*);
    const auto now = reinterpret_cast<Clock>(guarded.object.vtable[2])(&guarded.object);
    Require(now >= before && now <= _ZN3rtc10TimeMillisEv_nid_postfix());
    auto* buckets = guarded.object.sampleBuckets;
    using Destructor = void(APS5_VABI*)(WebRtcRateTracker*);
    reinterpret_cast<Destructor>(guarded.object.vtable[0])(&guarded.object);
    Require(lastFreed == buckets);
    for (auto value : guarded.before) Require(value == 0x5a);
    for (auto value : guarded.after) Require(value == 0x5a);
    auto* heapObject = static_cast<WebRtcRateTracker*>(_Znwm_nid_postfix(sizeof(WebRtcRateTracker)));
    _ZN3rtc11RateTrackerC1Elm_nid_postfix(heapObject, 100, 10);
    reinterpret_cast<Destructor>(heapObject->vtable[1])(heapObject);
    Require(lastFreed == heapObject);
    for (auto parameters : {std::pair<std::int64_t, std::size_t>{0, 10}, {-1, 10}, {100, 0}}) {
        bool threw = false;
        try {
            _ZN3rtc11RateTrackerC1Elm_nid_postfix(&guarded.object, parameters.first, parameters.second);
        } catch (const std::invalid_argument&) { threw = true; }
        Require(threw);
    }
    bool threw = false;
    try {
        _ZN3rtc11RateTrackerC1Elm_nid_postfix(&guarded.object, 100, std::numeric_limits<std::size_t>::max());
    } catch (const std::bad_array_new_length&) { threw = true; }
    Require(threw);
}

void TestReleaseSamples() {
    Tracker tracker;
    tracker.AddSamples(5);
    tracker.AddSamples(-1);
    tracker.AdvanceTime(100);
    Require(tracker.ComputeRate() == 40.0);
    Require(tracker.object.totalSampleCount == 4);
}

void TestPartialRounding() {
    Tracker tracker;
    tracker.AddSamples(1);
    tracker.AdvanceTime(1050);
    Require(tracker.ComputeRate() == 1.0);
    tracker.AdvanceTime(1);
    Require(tracker.ComputeRate() == 0.0);
    tracker.AdvanceTime(10000);
    tracker.AddSamples(7);
    Require(tracker.ComputeRate() == 7.0);
    tracker.AdvanceTime(1000);
    Require(tracker.ComputeRateForInterval(0) == 0.0);
}

void TestOddBucketRounding() {
    Tracker tracker(3, 1);
    tracker.AddSamples(1);
    const double expected[] = {0.0, 0.0, 0.0, 1000.0 / 3.0, 1000.0 / 3.0, 0.0, 0.0};
    for (double rate : expected) {
        Require(tracker.ComputeRateForInterval(3) == rate);
        tracker.AdvanceTime(1);
    }
    tracker.time = 6;
    tracker.AddSamples(7);
    tracker.time = 9;
    Require(tracker.ComputeRateForInterval(3) == 7000.0 / 3.0);
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&RateTrackerAllocate_nid_no_patch), reinterpret_cast<void*>(&RateTrackerFree_nid_no_patch),
        reinterpret_cast<void*>(&RateTrackerCalloc_nid_no_patch), reinterpret_cast<void*>(&RateTrackerReallocate_nid_no_patch),
        reinterpret_cast<void*>(&RateTrackerAlign_nid_no_patch), reinterpret_cast<void*>(&RateTrackerRealign_nid_no_patch), reinterpret_cast<void*>(&RateTrackerPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        Test30FPS();
        Test60FPS();
        TestRateTrackerBasics();
        TestLongPeriodBetweenSamples();
        TestRolloff();
        TestGetUnitSecondsAfterInitialValue();
        TestLargeNumbers();
        TestAbi();
        TestPartialRounding();
        TestReleaseSamples();
        TestOddBucketRounding();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "RateTracker exception: %s\n", error.what());
        return 1;
    }
}
