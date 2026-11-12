#pragma once

#include <cstddef>
#include <cstdint>

template<typename TValue>
struct WebRtcNackVector {
    TValue* begin;
    TValue* end;
    TValue* capacityEnd;
};

struct WebRtcPackedNack {
    std::uint16_t firstPid;
    std::uint16_t bitmask;
};

struct WebRtcRtcpNack {
    const void* vtable;
    std::uint32_t senderSsrc;
    std::uint32_t mediaSsrc;
    WebRtcNackVector<WebRtcPackedNack> packed;
    WebRtcNackVector<std::uint16_t> packetIds;
};

static_assert(sizeof(WebRtcPackedNack) == 4);
static_assert(offsetof(WebRtcPackedNack, bitmask) == 2);
static_assert(sizeof(WebRtcNackVector<std::uint16_t>) == 24);
static_assert(offsetof(WebRtcNackVector<std::uint16_t>, end) == 8);
static_assert(offsetof(WebRtcNackVector<std::uint16_t>, capacityEnd) == 16);
static_assert(sizeof(WebRtcRtcpNack) == 64);
static_assert(offsetof(WebRtcRtcpNack, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpNack, mediaSsrc) == 12);
static_assert(offsetof(WebRtcRtcpNack, packed) == 16);
static_assert(offsetof(WebRtcRtcpNack, packetIds) == 40);
