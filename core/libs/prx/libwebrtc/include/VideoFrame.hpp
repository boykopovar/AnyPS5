#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcOptionalInt64 {
    std::int64_t value;
    bool engaged;
};

struct WebRtcVideoFrameBuilder {
    std::uint16_t id;
    void* videoFrameBuffer;
    std::int64_t timestampUs;
    WebRtcOptionalInt64 presentationTimestamp;
    WebRtcOptionalInt64 referenceTime;
    std::uint32_t timestampRtp;
    std::int64_t ntpTimeMs;
    std::int32_t rotation;
    alignas(4) unsigned char colorSpace[72];
    bool useLowLatencyRendering;
    std::int32_t maxCompositionDelayInFrames;
    bool hasMaxCompositionDelayInFrames;
    std::int32_t updateRect[4];
    bool hasUpdateRect;
    void* packetInfos;
};

static_assert(sizeof(WebRtcOptionalInt64) == 16);
static_assert(sizeof(WebRtcVideoFrameBuilder) == 192);
static_assert(offsetof(WebRtcVideoFrameBuilder, videoFrameBuffer) == 8);
static_assert(offsetof(WebRtcVideoFrameBuilder, timestampUs) == 16);
static_assert(offsetof(WebRtcVideoFrameBuilder, timestampRtp) == 56);
static_assert(offsetof(WebRtcVideoFrameBuilder, ntpTimeMs) == 64);
static_assert(offsetof(WebRtcVideoFrameBuilder, rotation) == 72);
static_assert(offsetof(WebRtcVideoFrameBuilder, colorSpace) == 76);
static_assert(offsetof(WebRtcVideoFrameBuilder, useLowLatencyRendering) == 148);
static_assert(offsetof(WebRtcVideoFrameBuilder, maxCompositionDelayInFrames) == 152);
static_assert(offsetof(WebRtcVideoFrameBuilder, updateRect) == 160);
static_assert(offsetof(WebRtcVideoFrameBuilder, hasUpdateRect) == 176);
static_assert(offsetof(WebRtcVideoFrameBuilder, packetInfos) == 184);

struct WebRtcVideoFrame {
    std::uint16_t id;
    void* videoFrameBuffer;
    std::uint32_t timestampRtp;
    std::int64_t ntpTimeMs;
    std::int64_t timestampUs;
};

static_assert(offsetof(WebRtcVideoFrame, videoFrameBuffer) == 8);
static_assert(offsetof(WebRtcVideoFrame, timestampRtp) == 16);
static_assert(offsetof(WebRtcVideoFrame, ntpTimeMs) == 24);
static_assert(offsetof(WebRtcVideoFrame, timestampUs) == 32);
