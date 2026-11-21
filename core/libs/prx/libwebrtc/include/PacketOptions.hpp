#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcPacketOptions {
    std::int64_t packetId;
    bool isMedia;
    bool includedInFeedback;
    bool includedInAllocation;
    bool sendAsEct1;
    bool batchable;
    bool lastPacketInBatch;
};

static_assert(sizeof(WebRtcPacketOptions) == 16 && alignof(WebRtcPacketOptions) == 8);
static_assert(offsetof(WebRtcPacketOptions, packetId) == 0);
static_assert(offsetof(WebRtcPacketOptions, isMedia) == 8);
static_assert(offsetof(WebRtcPacketOptions, includedInFeedback) == 9);
static_assert(offsetof(WebRtcPacketOptions, includedInAllocation) == 10);
static_assert(offsetof(WebRtcPacketOptions, sendAsEct1) == 11);
static_assert(offsetof(WebRtcPacketOptions, batchable) == 12);
static_assert(offsetof(WebRtcPacketOptions, lastPacketInBatch) == 13);
