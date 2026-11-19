#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

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

extern "C" void WebRtcValidateSdpAudioFormat_nid_no_patch(const WebRtcSdpAudioFormat* format, const char* message);
extern "C" void WebRtcCopySdpNameAndParameters_nid_no_patch(WebRtcGuestString* name, WebRtcStringMap* parameters, const WebRtcGuestString* sourceName, const WebRtcStringMap* sourceParameters, const char* message);
extern "C" void WebRtcMoveSdpNameAndParameters_nid_no_patch(WebRtcGuestString* name, WebRtcStringMap* parameters, WebRtcGuestString* sourceName, WebRtcStringMap* sourceParameters, const char* message);
extern "C" void WebRtcDestroySdpNameAndParameters_nid_no_patch(WebRtcGuestString* name, WebRtcStringMap* parameters, const char* message);
extern "C" void WebRtcReadSdpNameAndParameters_nid_no_patch(const WebRtcGuestString* name, const WebRtcStringMap* parameters, const char* message, std::string* outName, std::map<std::string, std::string>* outParameters);

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
