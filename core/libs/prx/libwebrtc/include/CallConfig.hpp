#pragma once

#include <cstddef>
#include <cstdint>

struct WebRtcRefCountedBase {
    const void* const* vtable;
    std::int32_t refCount;
};

static_assert(sizeof(WebRtcRefCountedBase) == 16);
static_assert(offsetof(WebRtcRefCountedBase, vtable) == 0);
static_assert(offsetof(WebRtcRefCountedBase, refCount) == 8);

struct WebRtcEnvironment {
    void* storage;
    const void* fieldTrials;
    void* clock;
    void* taskQueueFactory;
    void* eventLog;
};

static_assert(sizeof(WebRtcEnvironment) == 40);
static_assert(offsetof(WebRtcEnvironment, storage) == 0);
static_assert(offsetof(WebRtcEnvironment, fieldTrials) == 8);
static_assert(offsetof(WebRtcEnvironment, clock) == 16);
static_assert(offsetof(WebRtcEnvironment, taskQueueFactory) == 24);
static_assert(offsetof(WebRtcEnvironment, eventLog) == 32);

struct WebRtcBitrateConstraints {
    std::int32_t minBitrateBps;
    std::int32_t startBitrateBps;
    std::int32_t maxBitrateBps;
};

static_assert(sizeof(WebRtcBitrateConstraints) == 12);

struct WebRtcOptionalTimeDelta {
    std::int64_t valueUs;
    bool engaged;
};

static_assert(sizeof(WebRtcOptionalTimeDelta) == 16);
static_assert(offsetof(WebRtcOptionalTimeDelta, engaged) == 8);

struct WebRtcCallConfig {
    WebRtcEnvironment env;
    WebRtcBitrateConstraints bitrateConfig;
    void* audioState;
    void* audioProcessing;
    void* fecControllerFactory;
    void* networkStatePredictorFactory;
    void* perCallNetworkControllerFactory;
    void* networkControllerFactory;
    void* netEqFactory;
    void* networkTaskQueue;
    void* rtpTransportControllerSendFactory;
    void* decodeMetronome;
    void* encodeMetronome;
    WebRtcOptionalTimeDelta pacerBurstInterval;
    bool enableSendPacketBatching;
};

static_assert(sizeof(WebRtcCallConfig) == 168);
static_assert(offsetof(WebRtcCallConfig, env) == 0);
static_assert(offsetof(WebRtcCallConfig, bitrateConfig) == 40);
static_assert(offsetof(WebRtcCallConfig, audioState) == 56);
static_assert(offsetof(WebRtcCallConfig, perCallNetworkControllerFactory) == 88);
static_assert(offsetof(WebRtcCallConfig, networkTaskQueue) == 112);
static_assert(offsetof(WebRtcCallConfig, pacerBurstInterval) == 144);
static_assert(offsetof(WebRtcCallConfig, enableSendPacketBatching) == 160);
