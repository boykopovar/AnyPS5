#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcVideoFrameBuffer {
    const void* vtable;
};

struct WebRtcVideoFrameBufferTypes {
    const std::int32_t* data;
    std::size_t size;
};

static_assert(sizeof(WebRtcVideoFrameBuffer) == 8);
static_assert(sizeof(WebRtcVideoFrameBufferTypes) == 16);
