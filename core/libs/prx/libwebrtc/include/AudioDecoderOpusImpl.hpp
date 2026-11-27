#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcOpusDecInst;

struct WebRtcFieldTrialsView {
    void* const* vtable;
};

struct WebRtcAudioDecoderOpusImpl {
    void* const* vtable;
    WebRtcOpusDecInst* decState;
    std::size_t channels;
    int sampleRateHz;
    bool generatePlc;
};

static_assert(sizeof(WebRtcAudioDecoderOpusImpl) == 32);
static_assert(offsetof(WebRtcAudioDecoderOpusImpl, vtable) == 0);
static_assert(offsetof(WebRtcAudioDecoderOpusImpl, decState) == 8);
static_assert(offsetof(WebRtcAudioDecoderOpusImpl, channels) == 16);
static_assert(offsetof(WebRtcAudioDecoderOpusImpl, sampleRateHz) == 24);
static_assert(offsetof(WebRtcAudioDecoderOpusImpl, generatePlc) == 28);
