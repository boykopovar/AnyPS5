#pragma once

#include "SceTypes.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcRecursiveCriticalSection {
    PthreadMutex mutex;
    std::uint64_t thread;
    int recursionCount;
};

static_assert(sizeof(WebRtcRecursiveCriticalSection) == 24);
static_assert(offsetof(WebRtcRecursiveCriticalSection, thread) == 8);
static_assert(offsetof(WebRtcRecursiveCriticalSection, recursionCount) == 16);

struct WebRtcCritScope {
    const WebRtcRecursiveCriticalSection* section;
};

static_assert(sizeof(WebRtcCritScope) == 8);
