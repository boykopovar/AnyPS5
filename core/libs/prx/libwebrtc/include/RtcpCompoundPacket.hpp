#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRtcpCompoundPacket {
    const void* vtable;
    std::uint32_t senderSsrc;
    void** appendedBegin;
    void** appendedEnd;
    void** appendedCapacity;
};

static_assert(sizeof(WebRtcRtcpCompoundPacket) == 40);
static_assert(offsetof(WebRtcRtcpCompoundPacket, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpCompoundPacket, appendedBegin) == 16);
static_assert(offsetof(WebRtcRtcpCompoundPacket, appendedCapacity) == 32);
