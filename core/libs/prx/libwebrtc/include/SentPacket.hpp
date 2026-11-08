#pragma once

#include <cstddef>
#include <cstdint>

enum class WebRtcPacketType : std::int32_t {
    kUnknown,
    kData,
    kIceConnectivityCheck,
    kIceConnectivityCheckResponse,
    kStunMessage,
    kTurnMessage,
};

enum class WebRtcPacketInfoProtocolType : std::int32_t {
    kUnknown,
    kUdp,
    kTcp,
    kSsltcp,
    kTls,
};

struct WebRtcOptionalUint16 {
    union {
        char nullState;
        std::uint16_t value;
    } storage;
    bool engaged;
};

struct WebRtcPacketInfo {
    bool includedInFeedback;
    bool includedInAllocation;
    bool isMedia;
    WebRtcPacketType packetType;
    WebRtcPacketInfoProtocolType protocol;
    WebRtcOptionalUint16 networkId;
    std::uint64_t packetSizeBytes;
    std::uint64_t turnOverheadBytes;
    std::uint64_t ipOverheadBytes;
};

struct WebRtcSentPacket {
    std::int64_t packetId;
    std::int64_t sendTimeMs;
    WebRtcPacketInfo info;
};

static_assert(sizeof(WebRtcOptionalUint16) == 4);
static_assert(offsetof(WebRtcOptionalUint16, engaged) == 2);
static_assert(sizeof(WebRtcPacketInfo) == 40);
static_assert(offsetof(WebRtcPacketInfo, packetType) == 4);
static_assert(offsetof(WebRtcPacketInfo, protocol) == 8);
static_assert(offsetof(WebRtcPacketInfo, networkId) == 12);
static_assert(offsetof(WebRtcPacketInfo, packetSizeBytes) == 16);
static_assert(offsetof(WebRtcPacketInfo, turnOverheadBytes) == 24);
static_assert(offsetof(WebRtcPacketInfo, ipOverheadBytes) == 32);
static_assert(sizeof(WebRtcSentPacket) == 56);
static_assert(offsetof(WebRtcSentPacket, sendTimeMs) == 8);
static_assert(offsetof(WebRtcSentPacket, info) == 16);
