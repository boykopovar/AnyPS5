#pragma once

#include <cstddef>

struct WebRtcPeerConnectionFactoryDependencies {
    void* networkThread;
    void* workerThread;
    void* signalingThread;
    void* socketFactory;
    void* packetSocketFactory;
    void* taskQueueFactory;
    void* eventLogFactory;
    void* fecControllerFactory;
    void* networkStatePredictorFactory;
    void* networkControllerFactory;
    void* networkManager;
    void* networkMonitorFactory;
    void* netEqFactory;
    void* sctpFactory;
    void* trials;
    void* transportControllerSendFactory;
    void* decodeMetronome;
    void* encodeMetronome;
    void* adm;
    void* audioEncoderFactory;
    void* audioDecoderFactory;
    void* audioMixer;
    void* audioProcessing;
    void* audioProcessingBuilder;
    void* audioFrameProcessor;
    void* videoEncoderFactory;
    void* videoDecoderFactory;
    void* mediaFactory;
};

static_assert(sizeof(WebRtcPeerConnectionFactoryDependencies) == 224);
static_assert(offsetof(WebRtcPeerConnectionFactoryDependencies, networkThread) == 0);
static_assert(offsetof(WebRtcPeerConnectionFactoryDependencies, packetSocketFactory) == 32);
static_assert(offsetof(WebRtcPeerConnectionFactoryDependencies, networkStatePredictorFactory) == 64);
static_assert(offsetof(WebRtcPeerConnectionFactoryDependencies, networkMonitorFactory) == 88);
static_assert(offsetof(WebRtcPeerConnectionFactoryDependencies, adm) == 144);
static_assert(offsetof(WebRtcPeerConnectionFactoryDependencies, audioProcessing) == 176);
static_assert(offsetof(WebRtcPeerConnectionFactoryDependencies, audioProcessingBuilder) == 184);
static_assert(offsetof(WebRtcPeerConnectionFactoryDependencies, videoEncoderFactory) == 200);
static_assert(offsetof(WebRtcPeerConnectionFactoryDependencies, mediaFactory) == 216);
