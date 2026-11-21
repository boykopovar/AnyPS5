#pragma once

#include <atomic>
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

struct WebRtcRefCountedI420Buffer {
    WebRtcI420Buffer buffer;
    std::atomic<std::int32_t> refCount;
};

static_assert(sizeof(WebRtcRefCountedI420Buffer) == 48);
static_assert(offsetof(WebRtcRefCountedI420Buffer, refCount) == 40);
