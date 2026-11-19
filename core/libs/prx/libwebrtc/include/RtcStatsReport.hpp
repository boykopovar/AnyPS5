#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcStatsMapNode;

struct WebRtcStatsMapLinks {
    WebRtcStatsMapNode* left;
    WebRtcStatsMapNode* right;
    WebRtcStatsMapNode* parent;
    bool isBlack;
};

struct WebRtcStatsMapNode {
    WebRtcStatsMapLinks links;
    WebRtcGuestString id;
    const void* stats;
};

struct WebRtcStatsMap {
    WebRtcStatsMapNode* beginNode;
    WebRtcStatsMapNode* root;
    std::uint64_t size;
};

struct WebRtcRtcStatsReport {
    std::int32_t refCount;
    std::int64_t timestampUs;
    WebRtcStatsMap stats;
};

struct WebRtcRtcStatsReportConstIterator {
    const WebRtcRtcStatsReport* report;
    const WebRtcStatsMapNode* node;
};

static_assert(sizeof(WebRtcStatsMapLinks) == 32);
static_assert(sizeof(WebRtcStatsMapNode) == 80);
static_assert(offsetof(WebRtcStatsMapNode, id) == 32);
static_assert(offsetof(WebRtcStatsMapNode, stats) == 72);
static_assert(sizeof(WebRtcStatsMap) == 24);
static_assert(sizeof(WebRtcRtcStatsReport) == 40);
static_assert(offsetof(WebRtcRtcStatsReport, timestampUs) == 8);
static_assert(offsetof(WebRtcRtcStatsReport, stats) == 16);
static_assert(sizeof(WebRtcRtcStatsReportConstIterator) == 16);
static_assert(offsetof(WebRtcRtcStatsReportConstIterator, node) == 8);
