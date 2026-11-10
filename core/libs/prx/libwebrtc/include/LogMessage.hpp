#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::int32_t WebRtcLsVerbose = 0;
constexpr std::int32_t WebRtcLsInfo = 1;
constexpr std::int32_t WebRtcLsWarning = 2;
constexpr std::int32_t WebRtcLsError = 3;
constexpr std::int32_t WebRtcLsNone = 4;

struct WebRtcLogSink {
    const void* vtable;
    WebRtcLogSink* next;
    std::int32_t minSeverity;
};

static_assert(sizeof(WebRtcLogSink) == 24);
static_assert(offsetof(WebRtcLogSink, next) == 8);
static_assert(offsetof(WebRtcLogSink, minSeverity) == 16);
