/* Derived from stats/rtc_stats_report.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/libc.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t DeletingDestructorSlot = 1;
constexpr int MaximumTreeHeight = 128;

using DeletingDestructor = void(APS5_VABI*)(void*);

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void RequireWritable(const void* address, std::size_t bytes, const char* message) {
    if (!address)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(const_cast<void*>(address), bytes))
        throw std::invalid_argument(message);
}

void CopyInChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!source)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

void CopyOutChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!destination)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

const WebRtcStatsMapNode* EndNode(const WebRtcStatsReport* report) {
    return reinterpret_cast<const WebRtcStatsMapNode*>(reinterpret_cast<std::uintptr_t>(report) +
        offsetof(WebRtcStatsReport, stats) + offsetof(WebRtcStatsMap, root));
}

std::int32_t* ReferenceCount(const WebRtcStatsReport* report) {
    return reinterpret_cast<std::int32_t*>(reinterpret_cast<std::uintptr_t>(report) +
        offsetof(WebRtcStatsReport, referenceCount));
}

WebRtcStatsReportConstIterator ReadIterator(const WebRtcStatsReportConstIterator* iterator, const char* message) {
    WebRtcStatsReportConstIterator value{};
    CopyInChecked(&value, iterator, sizeof(value), message);
    if (!value.node)
        throw std::invalid_argument(message);
    return value;
}

WebRtcStatsMapLinks ReadLinks(const WebRtcStatsMapNode* node, const char* message) {
    WebRtcStatsMapLinks links{};
    CopyInChecked(&links, node, sizeof(links), message);
    return links;
}

const WebRtcStatsMapNode* ReadLeft(const WebRtcStatsMapNode* node, const char* message) {
    const WebRtcStatsMapNode* left = nullptr;
    CopyInChecked(&left, node, sizeof(left), message);
    return left;
}

const WebRtcStatsMapNode* Next(const WebRtcStatsMapNode* node, const char* message) {
    const WebRtcStatsMapLinks links = ReadLinks(node, message);
    if (links.right) {
        const WebRtcStatsMapNode* next = links.right;
        for (int depth = 0; depth < MaximumTreeHeight; ++depth) {
            const WebRtcStatsMapNode* left = ReadLeft(next, message);
            if (!left)
                return next;
            next = left;
        }
        throw std::invalid_argument(message);
    }
    const WebRtcStatsMapNode* current = node;
    const WebRtcStatsMapNode* parent = links.parent;
    for (int depth = 0; depth < MaximumTreeHeight; ++depth) {
        if (!parent)
            throw std::invalid_argument(message);
        if (ReadLeft(parent, message) == current)
            return parent;
        current = parent;
        parent = ReadLinks(current, message).parent;
    }
    throw std::invalid_argument(message);
}

void* ReadStats(const WebRtcStatsReportConstIterator& iterator, const char* message) {
    if (iterator.report && iterator.node == EndNode(iterator.report))
        throw std::invalid_argument(message);
    void* stats = nullptr;
    CopyInChecked(&stats, reinterpret_cast<const unsigned char*>(iterator.node) + offsetof(WebRtcStatsMapNode, stats),
        sizeof(stats), message);
    return stats;
}

void ValidateString(const WebRtcGuestString& string, const char* message) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity)
        throw std::invalid_argument(message);
    if (string.capacity <= WebRtcGuestStringInlineCapacity)
        return;
    if (!string.pointer)
        throw std::invalid_argument(message);
    if (string.capacity + 1 >= GuestStringStorage::LargeAllocation) {
        void* raw = nullptr;
        CopyInChecked(&raw, static_cast<const void* const*>(static_cast<const void*>(string.pointer)) - 1, sizeof(raw), message);
        if (!raw)
            throw std::invalid_argument(message);
    }
}

void ValidateStats(const void* stats, const char* message) {
    if (!stats)
        return;
    const void* const* vtable = nullptr;
    CopyInChecked(&vtable, stats, sizeof(vtable), message);
    if (!vtable)
        throw std::invalid_argument(message);
    const void* destructor = nullptr;
    CopyInChecked(&destructor, vtable + DeletingDestructorSlot, sizeof(destructor), message);
    if (!destructor)
        throw std::invalid_argument(message);
}

std::vector<WebRtcStatsMapNode*> CollectNodes(const WebRtcStatsReport* report, const WebRtcStatsMap& map, const char* message) {
    std::vector<WebRtcStatsMapNode*> nodes;
    auto* const end = const_cast<WebRtcStatsMapNode*>(EndNode(report));
    if (map.size == 0) {
        if (map.root || map.beginNode != end)
            throw std::invalid_argument(message);
        return nodes;
    }
    if (!map.root || !map.beginNode || map.beginNode == end)
        throw std::invalid_argument(message);
    struct Entry {
        WebRtcStatsMapNode* node;
        WebRtcStatsMapNode* expectedParent;
    };
    std::vector<Entry> pending{{map.root, end}};
    while (!pending.empty()) {
        const Entry current = pending.back();
        pending.pop_back();
        if (nodes.size() >= map.size)
            throw std::invalid_argument(message);
        WebRtcStatsMapNode node{};
        CopyInChecked(&node, current.node, sizeof(node), message);
        if (node.links.parent != current.expectedParent)
            throw std::invalid_argument(message);
        RequireWritable(current.node, sizeof(WebRtcStatsMapNode), message);
        ValidateString(node.id, message);
        ValidateStats(node.stats, message);
        nodes.push_back(current.node);
        if (node.links.left)
            pending.push_back({node.links.left, current.node});
        if (node.links.right)
            pending.push_back({node.links.right, current.node});
    }
    if (nodes.size() != map.size)
        throw std::invalid_argument(message);
    std::reverse(nodes.begin(), nodes.end());
    return nodes;
}

std::vector<WebRtcStatsMapNode*> CollectReportNodes(const WebRtcStatsReport* report, const char* message) {
    WebRtcStatsReport hostReport{};
    CopyInChecked(&hostReport, report, sizeof(hostReport), message);
    return CollectNodes(report, hostReport.stats, message);
}

void DestroyReport(const WebRtcStatsReport* report, const std::vector<WebRtcStatsMapNode*>& nodes) {
    for (WebRtcStatsMapNode* node : nodes) {
        if (void* stats = node->stats) {
            const auto* vtable = *static_cast<const void* const* const*>(stats);
            reinterpret_cast<DeletingDestructor>(const_cast<void*>(vtable[DeletingDestructorSlot]))(stats);
        }
        if (node->id.capacity > WebRtcGuestStringInlineCapacity)
            GuestStringStorage::Deallocate(node->id.pointer, node->id.capacity + 1);
        _ZdlPv_nid_postfix(node);
    }
    _ZdlPv_nid_postfix(const_cast<WebRtcStatsReport*>(report));
}

WebRtcStatsReportConstIterator* MakeIterator(WebRtcStatsReportConstIterator* result, const WebRtcStatsReport* report, bool atEnd,
    const char* message) {
    WebRtcStatsReport hostReport{};
    CopyInChecked(&hostReport, report, sizeof(hostReport), message);
    if (!hostReport.stats.beginNode)
        throw std::invalid_argument(message);
    RequireWritable(result, sizeof(*result), message);
    RequireWritable(ReferenceCount(report), sizeof(std::int32_t), message);
    const WebRtcStatsReportConstIterator iterator{report, atEnd ? EndNode(report) : hostReport.stats.beginNode};
    std::atomic_ref<std::int32_t>(*ReferenceCount(report)).fetch_add(1, std::memory_order_relaxed);
    CopyOutChecked(result, &iterator, sizeof(iterator), message);
    return result;
}

WebRtcStatsReportConstIterator* Advance(WebRtcStatsReportConstIterator* iterator, const char* message) {
    WebRtcStatsReportConstIterator value = ReadIterator(iterator, message);
    RequireWritable(iterator, sizeof(*iterator), message);
    if (value.report && value.node == EndNode(value.report))
        throw std::invalid_argument(message);
    value.node = Next(value.node, message);
    CopyOutChecked(iterator, &value, sizeof(value), message);
    return iterator;
}

}

extern "C" {

WebRtcStatsReportConstIterator* APS5_VABI _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(
    WebRtcStatsReportConstIterator* result, const WebRtcStatsReport* report) {
    return MakeIterator(result, report, false, "RTCStatsReport::begin requires a result and a report");
}

WebRtcStatsReportConstIterator* APS5_VABI _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(
    WebRtcStatsReportConstIterator* result, const WebRtcStatsReport* report) {
    return MakeIterator(result, report, true, "RTCStatsReport::end requires a result and a report");
}

WebRtcStatsReportConstIterator* APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(
    WebRtcStatsReportConstIterator* iterator) {
    return Advance(iterator, "RTCStatsReport::ConstIterator::operator++ requires an iterator before the end");
}

WebRtcStatsReportConstIterator* APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(
    WebRtcStatsReportConstIterator* iterator, int) {
    return Advance(iterator, "RTCStatsReport::ConstIterator::operator++(int) requires an iterator before the end");
}

const void* APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(const WebRtcStatsReportConstIterator* iterator) {
    constexpr const char* message = "RTCStatsReport::ConstIterator::operator* requires an iterator before the end";
    return ReadStats(ReadIterator(iterator, message), message);
}

const void* APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(const WebRtcStatsReportConstIterator* iterator) {
    constexpr const char* message = "RTCStatsReport::ConstIterator::operator-> requires an iterator before the end";
    return ReadStats(ReadIterator(iterator, message), message);
}

bool APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(const WebRtcStatsReportConstIterator* iterator,
    const WebRtcStatsReportConstIterator* other) {
    constexpr const char* message = "RTCStatsReport::ConstIterator::operator!= requires two iterators";
    return ReadIterator(iterator, message).node != ReadIterator(other, message).node;
}

void APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(WebRtcStatsReportConstIterator* iterator) {
    constexpr const char* message = "RTCStatsReport::ConstIterator destructor requires a valid iterator";
    WebRtcStatsReportConstIterator value{};
    CopyInChecked(&value, iterator, sizeof(value), message);
    if (!value.report)
        return;
    RequireWritable(ReferenceCount(value.report), sizeof(std::int32_t), message);
    std::atomic_ref<std::int32_t> count(*ReferenceCount(value.report));
    if (count.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
    std::vector<WebRtcStatsMapNode*> nodes;
    try {
        nodes = CollectReportNodes(value.report, message);
    } catch (...) {
        count.fetch_add(1, std::memory_order_relaxed);
        throw;
    }
    DestroyReport(value.report, nodes);
}

}
