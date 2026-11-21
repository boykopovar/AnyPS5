#pragma once

#include <cstddef>

struct WebRtcAudioStateConfig {
    void* audioMixer;
    void* audioProcessing;
    void* audioDeviceModule;
    void* asyncAudioProcessingFactory;
};

static_assert(sizeof(WebRtcAudioStateConfig) == 32);
static_assert(offsetof(WebRtcAudioStateConfig, asyncAudioProcessingFactory) == 24);
