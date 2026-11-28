#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcPendingTaskSafetyFlag {
    std::int32_t refCount;
    bool alive;
};

static_assert(offsetof(WebRtcPendingTaskSafetyFlag, alive) == 4);
static_assert(sizeof(WebRtcPendingTaskSafetyFlag) == 8);
