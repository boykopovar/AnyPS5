#pragma once

#include <cstddef>

struct WebRtcPlatformThread {
    const void* vtable;
    void* handle;
    bool hasHandle;
    alignas(8) bool joinable;
};

static_assert(sizeof(WebRtcPlatformThread) == 32);
static_assert(offsetof(WebRtcPlatformThread, handle) == 8);
static_assert(offsetof(WebRtcPlatformThread, hasHandle) == 16);
static_assert(offsetof(WebRtcPlatformThread, joinable) == 24);
