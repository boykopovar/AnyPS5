#pragma once

#include <cstddef>

constexpr float WebRtcExpFilterValueUndefined = -1.0f;

struct WebRtcExpFilter {
    float alpha;
    float filtered;
    float max;
};

static_assert(sizeof(WebRtcExpFilter) == 12);
static_assert(offsetof(WebRtcExpFilter, filtered) == 4);
static_assert(offsetof(WebRtcExpFilter, max) == 8);
