#pragma once

#include <cstddef>
#include <cstdint>
#include "prx/libwebrtc/include/GuestString.hpp"

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
    void* stats;
};

struct WebRtcStatsMap {
    WebRtcStatsMapNode* beginNode;
    WebRtcStatsMapNode* root;
    std::uint64_t size;
};

struct WebRtcStatsReport {
    std::int32_t referenceCount;
    std::int64_t timestampUs;
    WebRtcStatsMap stats;
};

struct WebRtcStatsReportConstIterator {
    const WebRtcStatsReport* report;
    const WebRtcStatsMapNode* node;
};

static_assert(sizeof(WebRtcStatsMapLinks) == 32);
static_assert(offsetof(WebRtcStatsMapLinks, isBlack) == 24);
static_assert(sizeof(WebRtcStatsMapNode) == 80);
static_assert(offsetof(WebRtcStatsMapNode, id) == 32);
static_assert(offsetof(WebRtcStatsMapNode, stats) == 72);
static_assert(sizeof(WebRtcStatsMap) == 24);
static_assert(sizeof(WebRtcStatsReport) == 40);
static_assert(offsetof(WebRtcStatsReport, timestampUs) == 8);
static_assert(offsetof(WebRtcStatsReport, stats) == 16);
static_assert(sizeof(WebRtcStatsReportConstIterator) == 16);
static_assert(offsetof(WebRtcStatsReportConstIterator, node) == 8);
