#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcRtcpCommonFeedbackSize = 8;

struct WebRtcRtcpPli {
    const void* vtable;
    std::uint32_t senderSsrc;
    std::uint32_t mediaSsrc;
};

static_assert(sizeof(WebRtcRtcpPli) == 16);
static_assert(offsetof(WebRtcRtcpPli, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpPli, mediaSsrc) == 12);
