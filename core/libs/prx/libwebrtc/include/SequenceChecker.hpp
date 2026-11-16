#pragma once

#include "SceTypes.hpp"

#include <cstddef>

struct WebRtcTaskQueueBase;

struct WebRtcSequenceCheckerImpl {
    PthreadMutex lock;
    bool attached;
    Pthread validThread;
    const WebRtcTaskQueueBase* validQueue;
};

static_assert(sizeof(WebRtcSequenceCheckerImpl) == 32);
static_assert(offsetof(WebRtcSequenceCheckerImpl, attached) == 8);
static_assert(offsetof(WebRtcSequenceCheckerImpl, validThread) == 16);
static_assert(offsetof(WebRtcSequenceCheckerImpl, validQueue) == 24);
