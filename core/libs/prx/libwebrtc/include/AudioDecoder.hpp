#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcAudioDecoder;
struct WebRtcInt16Buffer;

struct WebRtcAudioDecoderParseResult {
    std::uint32_t timestamp;
    std::int32_t priority;
    void* frame;
};

static_assert(sizeof(WebRtcAudioDecoderParseResult) == 16);
static_assert(offsetof(WebRtcAudioDecoderParseResult, frame) == 8);
