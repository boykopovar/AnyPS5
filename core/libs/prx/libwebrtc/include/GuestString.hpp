#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcGuestString {
    std::uint64_t reserved;
    union {
        char buffer[16];
        char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

static_assert(sizeof(WebRtcGuestString) == 40);
static_assert(offsetof(WebRtcGuestString, size) == 24);
static_assert(offsetof(WebRtcGuestString, capacity) == 32);
