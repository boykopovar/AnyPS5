#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcI420Buffer {
    const void* vtable;
    std::int32_t width;
    std::int32_t height;
    std::int32_t strideY;
    std::int32_t strideU;
    std::int32_t strideV;
    std::uint8_t* data;
};

static_assert(sizeof(WebRtcI420Buffer) == 40);
static_assert(offsetof(WebRtcI420Buffer, strideY) == 16);
static_assert(offsetof(WebRtcI420Buffer, data) == 32);
