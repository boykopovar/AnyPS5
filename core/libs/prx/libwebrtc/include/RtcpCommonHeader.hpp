#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcRtcpCommonHeaderSize = 4;

struct WebRtcRtcpCommonHeader {
    std::uint8_t packetType;
    std::uint8_t countOrFormat;
    std::uint8_t paddingSize;
    std::uint32_t payloadSize;
    const std::uint8_t* payload;
};

static_assert(sizeof(WebRtcRtcpCommonHeader) == 16);
static_assert(offsetof(WebRtcRtcpCommonHeader, paddingSize) == 2);
static_assert(offsetof(WebRtcRtcpCommonHeader, payloadSize) == 4);
static_assert(offsetof(WebRtcRtcpCommonHeader, payload) == 8);
