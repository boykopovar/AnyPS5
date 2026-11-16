#pragma once

#include "SceTypes.hpp"

#include <cstddef>

struct WebRtcNullSocketServer {
    void* const* vtable;
    void* networkBinder;
    PthreadMutex eventMutex;
    PthreadCond eventCond;
    bool isManualReset;
    bool eventStatus;
};

static_assert(sizeof(WebRtcNullSocketServer) == 40);
static_assert(offsetof(WebRtcNullSocketServer, networkBinder) == 8);
static_assert(offsetof(WebRtcNullSocketServer, eventMutex) == 16);
static_assert(offsetof(WebRtcNullSocketServer, eventCond) == 24);
static_assert(offsetof(WebRtcNullSocketServer, isManualReset) == 32);
static_assert(offsetof(WebRtcNullSocketServer, eventStatus) == 33);
