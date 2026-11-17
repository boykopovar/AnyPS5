#pragma once

#include <cstddef>

struct WebRtcAvailableCpuFeatures {
    bool sse2;
    bool avx2;
    bool neon;
};

static_assert(sizeof(WebRtcAvailableCpuFeatures) == 3);
static_assert(offsetof(WebRtcAvailableCpuFeatures, avx2) == 1);
static_assert(offsetof(WebRtcAvailableCpuFeatures, neon) == 2);
