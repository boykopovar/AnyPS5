#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRaceChecker {
    volatile std::int32_t accessCount;
    volatile std::uint64_t accessingThread;
};

static_assert(sizeof(WebRtcRaceChecker) == 16);
static_assert(offsetof(WebRtcRaceChecker, accessingThread) == 8);
