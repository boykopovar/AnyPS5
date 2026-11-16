#pragma once

#include "prx/libwebrtc/include/Buffer.hpp"
#include "prx/libwebrtc/include/ColorSpace.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

struct WebRtcEncodedImageBuffer {
    const void* vtable;
    WebRtcBuffer buffer;
    std::atomic<std::int32_t> refCount;
};

struct WebRtcEncodedImageTiming {
    std::uint8_t flags;
    std::int64_t encodeStartMs;
    std::int64_t encodeFinishMs;
    std::int64_t packetizationFinishMs;
    std::int64_t pacerExitMs;
    std::int64_t networkTimestampMs;
    std::int64_t network2TimestampMs;
    std::int64_t receiveStartMs;
    std::int64_t receiveFinishMs;
};

struct WebRtcEncodedImageMapNode;

struct WebRtcEncodedImageMapLinks {
    WebRtcEncodedImageMapNode* left;
    WebRtcEncodedImageMapNode* right;
    WebRtcEncodedImageMapNode* parent;
    bool isBlack;
};

struct WebRtcEncodedImageMapNode {
    WebRtcEncodedImageMapLinks links;
    std::int32_t key;
    std::uint64_t value;
};

struct WebRtcEncodedImageMap {
    WebRtcEncodedImageMapNode* beginNode;
    WebRtcEncodedImageMapNode* root;
    std::uint64_t size;
};

struct WebRtcRtpPacketInfoEntry {
    std::uint32_t ssrc;
    std::uint32_t* csrcsBegin;
    std::uint32_t* csrcsEnd;
    std::uint32_t* csrcsCapacity;
    std::uint8_t trivial[72];
};

struct WebRtcRtpPacketInfosData {
    std::atomic<std::int32_t> refCount;
    WebRtcRtpPacketInfoEntry* begin;
    WebRtcRtpPacketInfoEntry* end;
    WebRtcRtpPacketInfoEntry* capacity;
};

struct WebRtcEncodedImageOptionalInt32 {
    std::int32_t value;
    bool engaged;
};

struct WebRtcEncodedImageOptionalUint16 {
    std::uint16_t value;
    bool engaged;
};

struct WebRtcEncodedImageOptionalColorSpace {
    WebRtcColorSpace value;
    bool engaged;
};

struct WebRtcEncodedImageOptionalInt64 {
    std::int64_t value;
    bool engaged;
};

struct WebRtcEncodedImage {
    std::uint32_t encodedWidth;
    std::uint32_t encodedHeight;
    std::int64_t ntpTimeMs;
    std::int64_t captureTimeMs;
    std::int32_t frameType;
    std::int32_t rotation;
    std::uint8_t contentType;
    std::int32_t qp;
    WebRtcEncodedImageTiming timing;
    std::int64_t playoutDelayMin;
    std::int64_t playoutDelayMax;
    bool hasPlayoutDelay;
    void* encodedData;
    std::uint64_t size;
    std::uint32_t timestampRtp;
    WebRtcEncodedImageOptionalInt32 simulcastIndex;
    WebRtcEncodedImageOptionalInt64 presentationTimestamp;
    WebRtcEncodedImageOptionalInt32 spatialIndex;
    WebRtcEncodedImageOptionalInt32 temporalIndex;
    WebRtcEncodedImageMap spatialLayerFrameSizeBytes;
    WebRtcEncodedImageOptionalColorSpace colorSpace;
    WebRtcEncodedImageOptionalUint16 videoFrameTrackingId;
    WebRtcRtpPacketInfosData* packetInfos;
    bool retransmissionAllowed;
    bool atTargetQuality;
    bool isSteadyStateRefreshFrame;
    double corruptionDetectionStdDev;
    std::int32_t corruptionDetectionLumaErrorThreshold;
    std::int32_t corruptionDetectionChromaErrorThreshold;
    bool hasCorruptionDetectionFilterSettings;
};

static_assert(sizeof(std::atomic<std::int32_t>) == 4);
static_assert(std::atomic<std::int32_t>::is_always_lock_free);
static_assert(sizeof(WebRtcEncodedImageBuffer) == 40);
static_assert(offsetof(WebRtcEncodedImageBuffer, buffer) == 8);
static_assert(offsetof(WebRtcEncodedImageBuffer, refCount) == 32);
static_assert(sizeof(WebRtcEncodedImageTiming) == 72);
static_assert(sizeof(WebRtcEncodedImageMapLinks) == 32);
static_assert(sizeof(WebRtcEncodedImageMapNode) == 48);
static_assert(sizeof(WebRtcEncodedImageOptionalInt32) == 8);
static_assert(sizeof(WebRtcEncodedImageOptionalUint16) == 4);
static_assert(sizeof(WebRtcEncodedImageOptionalColorSpace) == 72);
static_assert(offsetof(WebRtcEncodedImageOptionalColorSpace, engaged) == 68);
static_assert(sizeof(WebRtcEncodedImageOptionalInt64) == 16);
static_assert(offsetof(WebRtcEncodedImageMapNode, key) == 32);
static_assert(offsetof(WebRtcEncodedImageMapNode, value) == 40);
static_assert(sizeof(WebRtcEncodedImageMap) == 24);
static_assert(sizeof(WebRtcRtpPacketInfoEntry) == 104);
static_assert(offsetof(WebRtcRtpPacketInfoEntry, csrcsBegin) == 8);
static_assert(sizeof(WebRtcRtpPacketInfosData) == 32);
static_assert(offsetof(WebRtcRtpPacketInfosData, begin) == 8);
static_assert(sizeof(WebRtcEncodedImage) == 344);
static_assert(offsetof(WebRtcEncodedImage, frameType) == 24);
static_assert(offsetof(WebRtcEncodedImage, contentType) == 32);
static_assert(offsetof(WebRtcEncodedImage, qp) == 36);
static_assert(offsetof(WebRtcEncodedImage, timing) == 40);
static_assert(offsetof(WebRtcEncodedImage, playoutDelayMin) == 112);
static_assert(offsetof(WebRtcEncodedImage, hasPlayoutDelay) == 128);
static_assert(offsetof(WebRtcEncodedImage, encodedData) == 136);
static_assert(offsetof(WebRtcEncodedImage, size) == 144);
static_assert(offsetof(WebRtcEncodedImage, timestampRtp) == 152);
static_assert(offsetof(WebRtcEncodedImage, simulcastIndex) == 156);
static_assert(offsetof(WebRtcEncodedImage, presentationTimestamp) == 168);
static_assert(offsetof(WebRtcEncodedImage, spatialIndex) == 184);
static_assert(offsetof(WebRtcEncodedImage, temporalIndex) == 192);
static_assert(offsetof(WebRtcEncodedImage, spatialLayerFrameSizeBytes) == 200);
static_assert(offsetof(WebRtcEncodedImage, colorSpace) == 224);
static_assert(offsetof(WebRtcEncodedImage, videoFrameTrackingId) == 296);
static_assert(offsetof(WebRtcEncodedImage, packetInfos) == 304);
static_assert(offsetof(WebRtcEncodedImage, retransmissionAllowed) == 312);
static_assert(offsetof(WebRtcEncodedImage, isSteadyStateRefreshFrame) == 314);
static_assert(offsetof(WebRtcEncodedImage, corruptionDetectionStdDev) == 320);
static_assert(offsetof(WebRtcEncodedImage, hasCorruptionDetectionFilterSettings) == 336);
