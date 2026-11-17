#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcStringMapNode;

struct WebRtcStringMapLinks {
    WebRtcStringMapNode* left;
    WebRtcStringMapNode* right;
    WebRtcStringMapNode* parent;
    bool isBlack;
};

struct WebRtcStringMapNode {
    WebRtcStringMapLinks links;
    WebRtcGuestString key;
    WebRtcGuestString value;
};

struct WebRtcStringMap {
    WebRtcStringMapNode* beginNode;
    WebRtcStringMapNode* root;
    std::uint64_t size;
};

struct WebRtcSdpAudioFormat {
    WebRtcGuestString name;
    std::int32_t clockrateHz;
    std::uint64_t numChannels;
    WebRtcStringMap parameters;
};

static_assert(sizeof(WebRtcStringMapLinks) == 32);
static_assert(sizeof(WebRtcStringMapNode) == 112);
static_assert(offsetof(WebRtcStringMapNode, key) == 32);
static_assert(offsetof(WebRtcStringMapNode, value) == 72);
static_assert(sizeof(WebRtcStringMap) == 24);
static_assert(sizeof(WebRtcSdpAudioFormat) == 80);
static_assert(offsetof(WebRtcSdpAudioFormat, name) == 0);
static_assert(offsetof(WebRtcSdpAudioFormat, clockrateHz) == 40);
static_assert(offsetof(WebRtcSdpAudioFormat, numChannels) == 48);
static_assert(offsetof(WebRtcSdpAudioFormat, parameters) == 56);
