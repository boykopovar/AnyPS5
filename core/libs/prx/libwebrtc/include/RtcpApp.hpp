#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcBuffer {
    std::uint64_t size;
    std::uint64_t capacity;
    std::uint8_t* data;
};

struct WebRtcRtcpApp {
    const void* vtable;
    std::uint32_t senderSsrc;
    std::uint8_t subType;
    std::uint32_t name;
    WebRtcBuffer data;
};

static_assert(sizeof(WebRtcBuffer) == 24);
static_assert(sizeof(WebRtcRtcpApp) == 48);
static_assert(offsetof(WebRtcRtcpApp, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpApp, subType) == 12);
static_assert(offsetof(WebRtcRtcpApp, name) == 16);
static_assert(offsetof(WebRtcRtcpApp, data) == 24);
