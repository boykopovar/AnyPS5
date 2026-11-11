#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRtcpTransportFeedback {
    const void* vtable;
    std::uint32_t senderSsrc;
    std::uint32_t mediaSsrc;
    std::uint16_t baseSequenceNumber;
    std::uint16_t sequenceNumberCount;
    std::uint32_t baseTimeTicks;
    std::uint8_t feedbackSequence;
    bool includeTimestamps;
};

static_assert(offsetof(WebRtcRtcpTransportFeedback, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpTransportFeedback, mediaSsrc) == 12);
static_assert(offsetof(WebRtcRtcpTransportFeedback, baseSequenceNumber) == 16);
static_assert(offsetof(WebRtcRtcpTransportFeedback, sequenceNumberCount) == 18);
static_assert(offsetof(WebRtcRtcpTransportFeedback, baseTimeTicks) == 20);
static_assert(offsetof(WebRtcRtcpTransportFeedback, includeTimestamps) == 25);
