#pragma once

#include "prx/libwebrtc/include/CopyOnWriteBuffer.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcRtpExtensionInfo {
    std::uint8_t id;
    std::uint8_t length;
    std::uint16_t offset;
};

struct WebRtcRtpExtensionInfoVector {
    WebRtcRtpExtensionInfo* begin;
    WebRtcRtpExtensionInfo* end;
    WebRtcRtpExtensionInfo* capacityEnd;
};

struct WebRtcRtpHeaderExtensionMap {
    std::uint8_t ids[22];
    bool extmapAllowMixed;
};

struct WebRtcRtpPacket {
    bool marker;
    std::uint8_t payloadType;
    std::uint8_t paddingSize;
    std::uint16_t sequenceNumber;
    std::uint32_t timestamp;
    std::uint32_t ssrc;
    std::size_t payloadOffset;
    std::size_t payloadSize;
    WebRtcRtpHeaderExtensionMap extensions;
    WebRtcRtpExtensionInfoVector extensionEntries;
    std::size_t extensionsSize;
    WebRtcCopyOnWriteBuffer buffer;
};

static_assert(sizeof(WebRtcRtpExtensionInfo) == 4);
static_assert(offsetof(WebRtcRtpExtensionInfo, length) == 1);
static_assert(offsetof(WebRtcRtpExtensionInfo, offset) == 2);
static_assert(sizeof(WebRtcRtpExtensionInfoVector) == 24);
static_assert(sizeof(WebRtcRtpHeaderExtensionMap) == 23);
static_assert(offsetof(WebRtcRtpHeaderExtensionMap, extmapAllowMixed) == 22);
static_assert(sizeof(WebRtcRtpPacket) == 112);
static_assert(offsetof(WebRtcRtpPacket, payloadType) == 1);
static_assert(offsetof(WebRtcRtpPacket, paddingSize) == 2);
static_assert(offsetof(WebRtcRtpPacket, sequenceNumber) == 4);
static_assert(offsetof(WebRtcRtpPacket, timestamp) == 8);
static_assert(offsetof(WebRtcRtpPacket, ssrc) == 12);
static_assert(offsetof(WebRtcRtpPacket, payloadOffset) == 16);
static_assert(offsetof(WebRtcRtpPacket, payloadSize) == 24);
static_assert(offsetof(WebRtcRtpPacket, extensions) == 32);
static_assert(offsetof(WebRtcRtpPacket, extensionEntries) == 56);
static_assert(offsetof(WebRtcRtpPacket, extensionsSize) == 80);
static_assert(offsetof(WebRtcRtpPacket, buffer) == 88);
