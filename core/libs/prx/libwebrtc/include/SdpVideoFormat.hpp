#pragma once

#include "prx/libwebrtc/include/CodecSpecificInfo.hpp"
#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>
#include <cstdint>

constexpr std::size_t WebRtcScalabilityModeCount = 34;

struct WebRtcVideoStringMapNode;

struct WebRtcVideoStringMapLinks {
    WebRtcVideoStringMapNode* left;
    WebRtcVideoStringMapNode* right;
    WebRtcVideoStringMapNode* parent;
    bool isBlack;
};

struct WebRtcVideoStringMapNode {
    WebRtcVideoStringMapLinks links;
    WebRtcGuestString key;
    WebRtcGuestString value;
};

struct WebRtcVideoStringMap {
    WebRtcVideoStringMapNode* beginNode;
    WebRtcVideoStringMapNode* root;
    std::uint64_t size;
};

struct WebRtcSdpVideoFormat {
    WebRtcGuestString name;
    WebRtcVideoStringMap parameters;
    WebRtcInlinedVector<WebRtcScalabilityModeCount> scalabilityModes;
};

struct WebRtcSdpVideoFormatView {
    const WebRtcSdpVideoFormat* data;
    std::size_t size;
};

static_assert(sizeof(WebRtcVideoStringMapLinks) == 32);
static_assert(sizeof(WebRtcVideoStringMapNode) == 112);
static_assert(offsetof(WebRtcVideoStringMapNode, key) == 32);
static_assert(offsetof(WebRtcVideoStringMapNode, value) == 72);
static_assert(sizeof(WebRtcVideoStringMap) == 24);
static_assert(sizeof(WebRtcInlinedVector<WebRtcScalabilityModeCount>) == 48);
static_assert(sizeof(WebRtcSdpVideoFormat) == 112);
static_assert(offsetof(WebRtcSdpVideoFormat, name) == 0);
static_assert(offsetof(WebRtcSdpVideoFormat, parameters) == 40);
static_assert(offsetof(WebRtcSdpVideoFormat, scalabilityModes) == 64);
static_assert(sizeof(WebRtcSdpVideoFormatView) == 16);
