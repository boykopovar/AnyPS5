#pragma once

struct WebRtcAudioNetworkAdaptorConfig {
    void* eventLog;
};

static_assert(sizeof(WebRtcAudioNetworkAdaptorConfig) == 8);
