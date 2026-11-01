#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRmsLevel {
    float sumSquare;
    std::uint64_t sampleCount;
    float maxSumSquare;
    std::uint64_t blockSize;
    bool hasBlockSize;
};

static_assert(sizeof(WebRtcRmsLevel) == 40);
static_assert(offsetof(WebRtcRmsLevel, sampleCount) == 8);
static_assert(offsetof(WebRtcRmsLevel, maxSumSquare) == 16);
static_assert(offsetof(WebRtcRmsLevel, blockSize) == 24);
static_assert(offsetof(WebRtcRmsLevel, hasBlockSize) == 32);
