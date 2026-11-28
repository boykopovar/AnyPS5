#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>

namespace NKWebKit::TextureMapperAbi {

struct TextureMapperFPSCounter {
    bool isShowingFPS;
    double fpsInterval;
    double fpsTimestamp;
    int lastFPS;
    int frameCount;
};

static_assert(sizeof(TextureMapperFPSCounter) == 32);
static_assert(offsetof(TextureMapperFPSCounter, fpsInterval) == 8);
static_assert(offsetof(TextureMapperFPSCounter, fpsTimestamp) == 16);
static_assert(offsetof(TextureMapperFPSCounter, lastFPS) == 24);
static_assert(offsetof(TextureMapperFPSCounter, frameCount) == 28);

}  // namespace NKWebKit::TextureMapperAbi

extern "C" {

void APS5_VABI _ZN7WebCore23TextureMapperFPSCounterC1Ev(NKWebKit::TextureMapperAbi::TextureMapperFPSCounter* self);
void APS5_VABI _ZN7WebCore23TextureMapperFPSCounterC2Ev(NKWebKit::TextureMapperAbi::TextureMapperFPSCounter* self);

}
