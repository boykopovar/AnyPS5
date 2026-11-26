/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>

template <typename T>
struct WebRtcFeedbackVector {
    T* begin;
    T* end;
    T* capacity;
};

struct WebRtcFeedbackReceivedPacket {
    std::uint16_t sequenceNumber;
    std::int16_t deltaTicks;
};

struct WebRtcFeedbackLastChunk {
    std::uint8_t deltaSizes[14];
    std::size_t size;
    bool allSame;
    bool hasLargeDelta;
};

struct WebRtcRtcpTransportFeedback {
    void* const* vtable;
    std::uint32_t senderSsrc;
    std::uint32_t mediaSsrc;
    std::uint16_t baseSequenceNumber;
    std::uint16_t sequenceNumberCount;
    std::uint32_t baseTimeTicks;
    std::uint8_t feedbackSequence;
    bool includeTimestamps;
    std::int64_t lastTimestamp;
    WebRtcFeedbackVector<WebRtcFeedbackReceivedPacket> receivedPackets;
    WebRtcFeedbackVector<WebRtcFeedbackReceivedPacket> allPackets;
    WebRtcFeedbackVector<std::uint16_t> encodedChunks;
    WebRtcFeedbackLastChunk lastChunk;
    std::size_t sizeBytes;
};

static_assert(offsetof(WebRtcRtcpTransportFeedback, senderSsrc) == 8);
static_assert(offsetof(WebRtcRtcpTransportFeedback, mediaSsrc) == 12);
static_assert(offsetof(WebRtcRtcpTransportFeedback, baseSequenceNumber) == 16);
static_assert(offsetof(WebRtcRtcpTransportFeedback, sequenceNumberCount) == 18);
static_assert(offsetof(WebRtcRtcpTransportFeedback, baseTimeTicks) == 20);
static_assert(offsetof(WebRtcRtcpTransportFeedback, includeTimestamps) == 25);

static_assert(sizeof(WebRtcFeedbackReceivedPacket) == 4);
static_assert(sizeof(WebRtcFeedbackLastChunk) == 32);
static_assert(sizeof(WebRtcRtcpTransportFeedback) == 152);
static_assert(offsetof(WebRtcRtcpTransportFeedback, lastTimestamp) == 32);
static_assert(offsetof(WebRtcRtcpTransportFeedback, receivedPackets) == 40);
static_assert(offsetof(WebRtcRtcpTransportFeedback, allPackets) == 64);
static_assert(offsetof(WebRtcRtcpTransportFeedback, encodedChunks) == 88);
static_assert(offsetof(WebRtcRtcpTransportFeedback, lastChunk) == 112);
static_assert(offsetof(WebRtcRtcpTransportFeedback, sizeBytes) == 144);
