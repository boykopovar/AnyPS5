#pragma once

#include <cstddef>

struct WebRtcCallbackListReceivers {
    void* begin;
    void* end;
    void* capacity;
    bool sendInProgress;
};

static_assert(sizeof(WebRtcCallbackListReceivers) == 32);
static_assert(alignof(WebRtcCallbackListReceivers) == 8);
static_assert(offsetof(WebRtcCallbackListReceivers, begin) == 0);
static_assert(offsetof(WebRtcCallbackListReceivers, end) == 8);
static_assert(offsetof(WebRtcCallbackListReceivers, capacity) == 16);
static_assert(offsetof(WebRtcCallbackListReceivers, sendInProgress) == 24);
