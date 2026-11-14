#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

struct WebRtcRefCountedBuffer {
    std::size_t size;
    std::size_t capacity;
    std::uint8_t* data;
    std::atomic<int> refCount;
};

struct WebRtcCopyOnWriteBuffer {
    WebRtcRefCountedBuffer* buffer;
    std::size_t offset;
    std::size_t size;
};

static_assert(sizeof(WebRtcRefCountedBuffer) == 32);
static_assert(offsetof(WebRtcRefCountedBuffer, size) == 0);
static_assert(offsetof(WebRtcRefCountedBuffer, capacity) == 8);
static_assert(offsetof(WebRtcRefCountedBuffer, data) == 16);
static_assert(offsetof(WebRtcRefCountedBuffer, refCount) == 24);
static_assert(sizeof(std::atomic<int>) == 4);
static_assert(std::atomic<int>::is_always_lock_free);
static_assert(sizeof(WebRtcCopyOnWriteBuffer) == 24);
static_assert(offsetof(WebRtcCopyOnWriteBuffer, buffer) == 0);
static_assert(offsetof(WebRtcCopyOnWriteBuffer, offset) == 8);
static_assert(offsetof(WebRtcCopyOnWriteBuffer, size) == 16);
