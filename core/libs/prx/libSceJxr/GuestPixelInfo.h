#pragma once
#include <stdint.h>

typedef struct JxrPixelInfo {
    const void* guid;
    uint64_t channels;
    int32_t colorFormat;
    int32_t bitDepth;
    uint32_t bitsPerUnit;
    uint64_t flags;
    uint32_t interpretation;
    uint32_t samplesPerPixel;
    uint32_t bitsPerSample;
    uint32_t sampleFormat;
} JxrPixelInfo;
