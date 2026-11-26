#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRtcpReportBlock {
    std::uint32_t sourceSsrc;
    std::uint8_t fractionLost;
    std::int32_t cumulativeLost;
    std::uint32_t extendedHighSeqNum;
    std::uint32_t jitter;
    std::uint32_t lastSr;
    std::uint32_t delaySinceLastSr;
};

struct WebRtcRtcpReceiverReport {
    const void* vtable;
    std::uint32_t senderSsrc;
    WebRtcRtcpReportBlock* reportBlocksBegin;
    WebRtcRtcpReportBlock* reportBlocksEnd;
    WebRtcRtcpReportBlock* reportBlocksCapacity;
};

static_assert(sizeof(WebRtcRtcpReportBlock) == 28);
static_assert(offsetof(WebRtcRtcpReportBlock, cumulativeLost) == 8);
static_assert(offsetof(WebRtcRtcpReportBlock, delaySinceLastSr) == 24);
static_assert(sizeof(WebRtcRtcpReceiverReport) == 40);
static_assert(offsetof(WebRtcRtcpReceiverReport, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpReceiverReport, reportBlocksBegin) == 16);
static_assert(offsetof(WebRtcRtcpReceiverReport, reportBlocksCapacity) == 32);
