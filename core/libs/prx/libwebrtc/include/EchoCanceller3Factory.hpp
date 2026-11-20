#pragma once

#include "prx/libwebrtc/include/EchoCanceller3Config.hpp"

#include <cstddef>

struct WebRtcEchoCanceller3Factory {
    const void* vtable;
    WebRtcEchoCanceller3Config config;
};

static_assert(sizeof(WebRtcEchoCanceller3Factory) == 616);
static_assert(offsetof(WebRtcEchoCanceller3Factory, config) == 8);
