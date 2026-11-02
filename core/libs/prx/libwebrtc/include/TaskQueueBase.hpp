#pragma once

struct WebRtcTaskQueueBase;

struct WebRtcCurrentTaskQueueSetter {
    WebRtcTaskQueueBase* previous;
};

static_assert(sizeof(WebRtcCurrentTaskQueueSetter) == 8);
