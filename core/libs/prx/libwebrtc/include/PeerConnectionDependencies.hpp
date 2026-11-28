#pragma once

#include <cstddef>

struct WebRtcPeerConnectionDependencies {
    void* observer;
    void* allocator;
    void* asyncDnsResolverFactory;
    void* iceTransportFactory;
    void* certGenerator;
    void* tlsCertVerifier;
    void* videoBitrateAllocatorFactory;
    void* networkControllerFactory;
    void* trials;
};

static_assert(sizeof(WebRtcPeerConnectionDependencies) == 72);
static_assert(offsetof(WebRtcPeerConnectionDependencies, observer) == 0);
static_assert(offsetof(WebRtcPeerConnectionDependencies, allocator) == 8);
static_assert(offsetof(WebRtcPeerConnectionDependencies, asyncDnsResolverFactory) == 16);
static_assert(offsetof(WebRtcPeerConnectionDependencies, iceTransportFactory) == 24);
static_assert(offsetof(WebRtcPeerConnectionDependencies, certGenerator) == 32);
static_assert(offsetof(WebRtcPeerConnectionDependencies, tlsCertVerifier) == 40);
static_assert(offsetof(WebRtcPeerConnectionDependencies, videoBitrateAllocatorFactory) == 48);
static_assert(offsetof(WebRtcPeerConnectionDependencies, networkControllerFactory) == 56);
static_assert(offsetof(WebRtcPeerConnectionDependencies, trials) == 64);
