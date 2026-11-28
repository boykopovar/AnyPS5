#pragma once

#include "prx/libwebrtc/include/CopyOnWriteBuffer.hpp"
#include "prx/libwebrtc/include/RtpHeaderExtensionMap.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcRtpPacketExtensionInfo {
    std::uint8_t id;
    std::uint8_t length;
    std::uint16_t offset;
};

struct WebRtcRtpPacketReceived {
    bool marker;
    std::uint8_t payloadType;
    std::uint8_t paddingSize;
    std::uint16_t sequenceNumber;
    std::uint32_t timestamp;
    std::uint32_t ssrc;
    std::size_t payloadOffset;
    std::size_t payloadSize;
    WebRtcRtpHeaderExtensionMap extensions;
    WebRtcRtpPacketExtensionInfo* extensionEntriesBegin;
    WebRtcRtpPacketExtensionInfo* extensionEntriesEnd;
    WebRtcRtpPacketExtensionInfo* extensionEntriesCapacity;
    std::size_t extensionsSize;
    WebRtcCopyOnWriteBuffer buffer;
    std::int64_t arrivalTimeUs;
    std::int32_t ecn;
    std::int32_t payloadTypeFrequency;
    bool recovered;
    void* additionalData;
};

static_assert(sizeof(WebRtcRtpPacketExtensionInfo) == 4);
static_assert(sizeof(WebRtcRtpPacketReceived) == 144);
static_assert(offsetof(WebRtcRtpPacketReceived, sequenceNumber) == 4);
static_assert(offsetof(WebRtcRtpPacketReceived, payloadOffset) == 16);
static_assert(offsetof(WebRtcRtpPacketReceived, extensions) == 32);
static_assert(offsetof(WebRtcRtpPacketReceived, extensionEntriesBegin) == 56);
static_assert(offsetof(WebRtcRtpPacketReceived, extensionsSize) == 80);
static_assert(offsetof(WebRtcRtpPacketReceived, buffer) == 88);
static_assert(offsetof(WebRtcRtpPacketReceived, arrivalTimeUs) == 112);
static_assert(offsetof(WebRtcRtpPacketReceived, ecn) == 120);
static_assert(offsetof(WebRtcRtpPacketReceived, payloadTypeFrequency) == 124);
static_assert(offsetof(WebRtcRtpPacketReceived, recovered) == 128);
static_assert(offsetof(WebRtcRtpPacketReceived, additionalData) == 136);
