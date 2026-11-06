#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRateTracker {
    void* const* vtable;
    std::int64_t bucketMilliseconds;
    std::size_t bucketCount;
    std::int64_t* sampleBuckets;
    std::size_t totalSampleCount;
    std::size_t currentBucket;
    std::int64_t bucketStartTimeMilliseconds;
    std::int64_t initializationTimeMilliseconds;
};

static_assert(sizeof(WebRtcRateTracker) == 64);
static_assert(offsetof(WebRtcRateTracker, vtable) == 0);
static_assert(offsetof(WebRtcRateTracker, bucketMilliseconds) == 8);
static_assert(offsetof(WebRtcRateTracker, bucketCount) == 16);
static_assert(offsetof(WebRtcRateTracker, sampleBuckets) == 24);
static_assert(offsetof(WebRtcRateTracker, totalSampleCount) == 32);
static_assert(offsetof(WebRtcRateTracker, currentBucket) == 40);
static_assert(offsetof(WebRtcRateTracker, bucketStartTimeMilliseconds) == 48);
static_assert(offsetof(WebRtcRateTracker, initializationTimeMilliseconds) == 56);
