#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcVideoFrameDimensions {
    std::uint16_t id;
    const void* buffer;
};

static_assert(offsetof(WebRtcVideoFrameDimensions, buffer) == 8);
static_assert(sizeof(WebRtcVideoFrameDimensions) == 16);
