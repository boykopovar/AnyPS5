#pragma once

#include "SceTypes.hpp"

#include <cstddef>

struct WebRtcEvent {
    PthreadMutex eventMutex;
    PthreadCond eventCond;
    bool isManualReset;
    bool eventStatus;
};

static_assert(sizeof(WebRtcEvent) == 24);
static_assert(offsetof(WebRtcEvent, eventMutex) == 0);
static_assert(offsetof(WebRtcEvent, eventCond) == 8);
static_assert(offsetof(WebRtcEvent, isManualReset) == 16);
static_assert(offsetof(WebRtcEvent, eventStatus) == 17);

struct WebRtcNullSocketServer {
    void* const* vtable;
    void* networkBinder;
    WebRtcEvent event;
};

static_assert(sizeof(WebRtcNullSocketServer) == 40);
static_assert(offsetof(WebRtcNullSocketServer, vtable) == 0);
static_assert(offsetof(WebRtcNullSocketServer, networkBinder) == 8);
static_assert(offsetof(WebRtcNullSocketServer, event) == 16);
