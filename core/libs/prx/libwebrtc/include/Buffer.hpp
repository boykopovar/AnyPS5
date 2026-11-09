#pragma once

#include <cstdint>

struct WebRtcBuffer {
    std::uint64_t size;
    std::uint64_t capacity;
    std::uint8_t* data;
};

static_assert(sizeof(WebRtcBuffer) == 24);
