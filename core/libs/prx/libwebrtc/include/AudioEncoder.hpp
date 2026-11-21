#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcAudioEncoder {
    const void* vtable;
};

struct WebRtcBitrateAllocationUpdate {
    std::int64_t targetBitrateBps;
    std::int64_t stableTargetBitrateBps;
    double packetLossRatio;
    std::int64_t roundTripTimeUs;
    std::int64_t bwePeriodUs;
    double cwndReduceRatio;
};

struct WebRtcOptionalUint32 {
    union {
        char nullState;
        std::uint32_t value;
    } storage;
    bool engaged;
};

struct WebRtcOptionalFloat {
    union {
        char nullState;
        float value;
    } storage;
    bool engaged;
};

struct WebRtcAnaStats {
    WebRtcOptionalUint32 bitrateActionCounter;
    WebRtcOptionalUint32 channelActionCounter;
    WebRtcOptionalUint32 dtxActionCounter;
    WebRtcOptionalUint32 fecActionCounter;
    WebRtcOptionalUint32 frameLengthIncreaseCounter;
    WebRtcOptionalUint32 frameLengthDecreaseCounter;
    WebRtcOptionalFloat uplinkPacketLossFraction;
};

static_assert(sizeof(WebRtcOptionalUint32) == 8);
static_assert(offsetof(WebRtcOptionalUint32, engaged) == 4);
static_assert(sizeof(WebRtcOptionalFloat) == 8);
static_assert(sizeof(WebRtcAnaStats) == 56);
static_assert(offsetof(WebRtcAnaStats, uplinkPacketLossFraction) == 48);
static_assert(sizeof(WebRtcAudioEncoder) == 8);
static_assert(sizeof(WebRtcBitrateAllocationUpdate) == 48);
static_assert(offsetof(WebRtcBitrateAllocationUpdate, targetBitrateBps) == 0);
static_assert(offsetof(WebRtcBitrateAllocationUpdate, bwePeriodUs) == 32);

struct WebRtcAudioEncodedInfoLeaf {
    std::size_t encodedBytes;
    std::uint32_t encodedTimestamp;
    int payloadType;
    bool sendEvenIfEmpty;
    bool speech;
    int encoderType;
    std::uint8_t unknownAt18[0x4c];
    bool unknownAt64;
};

struct WebRtcAudioEncodedInfo {
    WebRtcAudioEncodedInfoLeaf leaf;
    WebRtcAudioEncodedInfoLeaf* redundantBegin;
    WebRtcAudioEncodedInfoLeaf* redundantEnd;
    WebRtcAudioEncodedInfoLeaf* redundantCapacity;
};

static_assert(sizeof(WebRtcAudioEncodedInfoLeaf) == 0x68);
static_assert(offsetof(WebRtcAudioEncodedInfoLeaf, encodedTimestamp) == 8);
static_assert(offsetof(WebRtcAudioEncodedInfoLeaf, payloadType) == 12);
static_assert(offsetof(WebRtcAudioEncodedInfoLeaf, sendEvenIfEmpty) == 16);
static_assert(offsetof(WebRtcAudioEncodedInfoLeaf, speech) == 17);
static_assert(offsetof(WebRtcAudioEncodedInfoLeaf, encoderType) == 20);
static_assert(offsetof(WebRtcAudioEncodedInfoLeaf, unknownAt18) == 0x18);
static_assert(offsetof(WebRtcAudioEncodedInfoLeaf, unknownAt64) == 0x64);
static_assert(sizeof(WebRtcAudioEncodedInfo) == 0x80);
static_assert(offsetof(WebRtcAudioEncodedInfo, redundantBegin) == 0x68);
