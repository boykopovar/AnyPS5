#pragma once

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

constexpr std::size_t WebRtcRtcpCommonFeedbackSize = 8;

struct WebRtcRtcpPli {
    const void* vtable;
    std::uint32_t senderSsrc;
    std::uint32_t mediaSsrc;
};

struct WebRtcRtcpPacketReadyCallback {
    void* function;
    void (APS5_VABI* call)(void* function, const std::uint8_t* data, std::size_t size);
};

static_assert(sizeof(WebRtcRtcpPli) == 16);
static_assert(offsetof(WebRtcRtcpPli, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpPli, mediaSsrc) == 12);
