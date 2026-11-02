#pragma once

#include <cstddef>

struct WebRtcPlatformThread {
    void* handle;
    bool hasHandle;
    alignas(8) bool joinable;
};

static_assert(sizeof(WebRtcPlatformThread) == 24);
static_assert(offsetof(WebRtcPlatformThread, hasHandle) == 8);
static_assert(offsetof(WebRtcPlatformThread, joinable) == 16);
