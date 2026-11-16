#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

struct WebRtcRtcStatsMapNode;

struct WebRtcRtcStatsMapLinks {
    WebRtcRtcStatsMapNode* left;
    WebRtcRtcStatsMapNode* right;
    WebRtcRtcStatsMapNode* parent;
    bool isBlack;
};

struct WebRtcRtcStats {
    const void* const* vtable;
};

struct WebRtcRtcStatsMapNode {
    WebRtcRtcStatsMapLinks links;
    WebRtcGuestString key;
    const WebRtcRtcStats* value;
};

struct WebRtcRtcStatsMap {
    const WebRtcRtcStatsMapNode* beginNode;
    WebRtcRtcStatsMapNode* root;
    std::uint64_t size;
};

struct WebRtcRtcStatsReport {
    mutable std::atomic<std::int32_t> refCount;
    std::int64_t timestampUs;
    WebRtcRtcStatsMap stats;
};

struct WebRtcRtcStatsReportConstIterator {
    const WebRtcRtcStatsReport* report;
    const WebRtcRtcStatsMapNode* it;
};

static_assert(sizeof(WebRtcRtcStatsMapLinks) == 32);
static_assert(sizeof(WebRtcRtcStatsMapNode) == 80);
static_assert(offsetof(WebRtcRtcStatsMapNode, key) == 32);
static_assert(offsetof(WebRtcRtcStatsMapNode, value) == 72);
static_assert(sizeof(WebRtcRtcStatsMap) == 24);
static_assert(sizeof(WebRtcRtcStatsReport) == 40);
static_assert(offsetof(WebRtcRtcStatsReport, timestampUs) == 8);
static_assert(offsetof(WebRtcRtcStatsReport, stats) == 16);
static_assert(sizeof(WebRtcRtcStatsReportConstIterator) == 16);
static_assert(offsetof(WebRtcRtcStatsReportConstIterator, report) == 0);
static_assert(offsetof(WebRtcRtcStatsReportConstIterator, it) == 8);
