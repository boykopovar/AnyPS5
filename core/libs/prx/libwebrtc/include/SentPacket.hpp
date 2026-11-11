#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcPacketInfo {
    bool includedInFeedback;
    bool includedInAllocation;
    bool isMedia;
    std::int32_t packetType;
    std::int32_t protocol;
    std::uint16_t networkId;
    bool hasNetworkId;
    std::uint64_t packetSizeBytes;
    std::uint64_t turnOverheadBytes;
    std::uint64_t ipOverheadBytes;
};

struct WebRtcSentPacket {
    std::int64_t packetId;
    std::int64_t sendTimeMs;
    WebRtcPacketInfo info;
};

static_assert(sizeof(WebRtcPacketInfo) == 40 && alignof(WebRtcPacketInfo) == 8);
static_assert(offsetof(WebRtcPacketInfo, includedInFeedback) == 0);
static_assert(offsetof(WebRtcPacketInfo, includedInAllocation) == 1);
static_assert(offsetof(WebRtcPacketInfo, isMedia) == 2);
static_assert(offsetof(WebRtcPacketInfo, packetType) == 4);
static_assert(offsetof(WebRtcPacketInfo, protocol) == 8);
static_assert(offsetof(WebRtcPacketInfo, networkId) == 12);
static_assert(offsetof(WebRtcPacketInfo, hasNetworkId) == 14);
static_assert(offsetof(WebRtcPacketInfo, packetSizeBytes) == 16);
static_assert(offsetof(WebRtcPacketInfo, turnOverheadBytes) == 24);
static_assert(offsetof(WebRtcPacketInfo, ipOverheadBytes) == 32);
static_assert(sizeof(WebRtcSentPacket) == 56 && alignof(WebRtcSentPacket) == 8);
static_assert(offsetof(WebRtcSentPacket, packetId) == 0);
static_assert(offsetof(WebRtcSentPacket, sendTimeMs) == 8);
static_assert(offsetof(WebRtcSentPacket, info) == 16);
