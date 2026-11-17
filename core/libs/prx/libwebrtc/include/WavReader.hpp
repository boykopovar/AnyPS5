#pragma once

#include "prx/libwebrtc/include/FileWrapper.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcWavReader {
    const void* vtable;
    std::int32_t sampleRate;
    std::size_t numChannels;
    std::int32_t format;
    std::size_t numSamplesInFile;
    std::size_t numUnreadSamples;
    WebRtcFileWrapper file;
    std::int64_t dataStartPos;
};

static_assert(sizeof(WebRtcWavReader) == 64);
static_assert(offsetof(WebRtcWavReader, sampleRate) == 8);
static_assert(offsetof(WebRtcWavReader, numChannels) == 16);
static_assert(offsetof(WebRtcWavReader, format) == 24);
static_assert(offsetof(WebRtcWavReader, numSamplesInFile) == 32);
static_assert(offsetof(WebRtcWavReader, numUnreadSamples) == 40);
static_assert(offsetof(WebRtcWavReader, file) == 48);
static_assert(offsetof(WebRtcWavReader, dataStartPos) == 56);
