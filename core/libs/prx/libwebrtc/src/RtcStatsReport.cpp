/* Derived from stats/rtc_stats_report.cc, api/stats/rtc_stats_report.h, api/ref_counted_base.h and rtc_base/ref_counter.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
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

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

constexpr const char* IncrementMessage = "RTCStatsReport::ConstIterator::operator++ requires an iterator into a valid stats map before its end";
constexpr const char* CompareMessage = "RTCStatsReport::ConstIterator::operator!= requires two readable iterators";
constexpr const char* DereferenceMessage = "RTCStatsReport::ConstIterator::operator* requires an iterator to a stats entry";
constexpr const char* ArrowMessage = "RTCStatsReport::ConstIterator::operator-> requires an iterator to a stats entry";
constexpr const char* BeginMessage = "RTCStatsReport::begin requires a writable result and a valid report";
constexpr const char* EndMessage = "RTCStatsReport::end requires a writable result and a valid report";
constexpr const char* DestructorMessage = "RTCStatsReport::ConstIterator destructor requires an iterator holding a valid report reference";
constexpr std::size_t DeletingDestructorSlot = 1;

using DeletingDestructor = void(APS5_VABI*)(const void*);

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

template<typename T> T ReadGuest(const void* address, const char* message) {
    if (!address)
        throw std::invalid_argument(message);
    T value{};
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(&value, address, sizeof(value)))
        throw std::invalid_argument(message);
    return value;
}

void RequireWritable(const void* address, std::size_t bytes, const char* message) {
    if (!address)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(const_cast<void*>(address), bytes))
        throw std::invalid_argument(message);
}

const WebRtcStatsMapNode* EndNode(const WebRtcRtcStatsReport* report) {
    return reinterpret_cast<const WebRtcStatsMapNode*>(&report->stats.root);
}

WebRtcRtcStatsReportConstIterator ReadIterator(const WebRtcRtcStatsReportConstIterator* iterator, const char* message) {
    const auto value = ReadGuest<WebRtcRtcStatsReportConstIterator>(iterator, message);
    if (!value.report || !value.node)
        throw std::invalid_argument(message);
    return value;
}

const WebRtcStatsMapNode* ReadLink(const WebRtcStatsMapNode* const* link) {
    return ReadGuest<const WebRtcStatsMapNode*>(link, IncrementMessage);
}

const WebRtcStatsMapNode* NextNode(const WebRtcRtcStatsReportConstIterator& iterator) {
    const WebRtcStatsMapNode* const end = EndNode(iterator.report);
    if (iterator.node == end)
        throw std::invalid_argument(IncrementMessage);
    const auto size = ReadGuest<std::uint64_t>(&iterator.report->stats.size, IncrementMessage);
    std::uint64_t visited = 0;
    const auto visit = [&] {
        if (++visited > size)
            throw std::invalid_argument(IncrementMessage);
    };
    const WebRtcStatsMapNode* node = iterator.node;
    if (const WebRtcStatsMapNode* right = ReadLink(&node->links.right)) {
        node = right;
        visit();
        for (const WebRtcStatsMapNode* left = ReadLink(&node->links.left); left; left = ReadLink(&node->links.left)) {
            visit();
            node = left;
        }
        return node;
    }
    for (;;) {
        const WebRtcStatsMapNode* parent = ReadLink(&node->links.parent);
        if (!parent)
            throw std::invalid_argument(IncrementMessage);
        if (ReadLink(&parent->links.left) == node)
            return parent;
        if (parent == end)
            throw std::invalid_argument(IncrementMessage);
        visit();
        node = parent;
    }
}

WebRtcRtcStatsReportConstIterator* Increment(WebRtcRtcStatsReportConstIterator* iterator) {
    const auto current = ReadIterator(iterator, IncrementMessage);
    const WebRtcStatsMapNode* next = NextNode(current);
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(&iterator->node, &next, sizeof(next)))
        throw std::invalid_argument(IncrementMessage);
    return iterator;
}

const void* EntryStats(const WebRtcRtcStatsReportConstIterator* iterator, const char* message) {
    const auto current = ReadIterator(iterator, message);
    if (current.node == EndNode(current.report))
        throw std::invalid_argument(message);
    return ReadGuest<const void*>(&current.node->stats, message);
}

WebRtcRtcStatsReportConstIterator* MakeIterator(WebRtcRtcStatsReportConstIterator* result, const WebRtcRtcStatsReport* report, bool atEnd, const char* message) {
    RequireWritable(result, sizeof(*result), message);
    RequireWritable(report, sizeof(*report), message);
    const WebRtcStatsMapNode* node = atEnd ? EndNode(report) : report->stats.beginNode;
    if (!node)
        throw std::invalid_argument(message);
    std::atomic_ref<std::int32_t>(const_cast<WebRtcRtcStatsReport*>(report)->refCount).fetch_add(1, std::memory_order_relaxed);
    result->report = report;
    result->node = node;
    return result;
}

void ValidateString(const WebRtcGuestString& string) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity)
        throw std::invalid_argument(DestructorMessage);
    if (string.capacity <= WebRtcGuestStringInlineCapacity)
        return;
    if (!string.pointer)
        throw std::invalid_argument(DestructorMessage);
    if (string.capacity + 1 >= GuestStringStorage::LargeAllocation) {
        const auto raw = ReadGuest<const void*>(static_cast<const void* const*>(static_cast<const void*>(string.pointer)) - 1, DestructorMessage);
        if (!raw)
            throw std::invalid_argument(DestructorMessage);
    }
}

void ValidateStats(const void* stats) {
    if (!stats)
        return;
    const auto vtable = ReadGuest<const void* const*>(stats, DestructorMessage);
    if (!vtable || !ReadGuest<const void*>(vtable + DeletingDestructorSlot, DestructorMessage))
        throw std::invalid_argument(DestructorMessage);
}

std::vector<WebRtcStatsMapNode*> CollectPostOrder(WebRtcRtcStatsReport* report) {
    std::vector<WebRtcStatsMapNode*> nodes;
    WebRtcStatsMap& map = report->stats;
    WebRtcStatsMapNode* const end = const_cast<WebRtcStatsMapNode*>(EndNode(report));
    if (map.size == 0) {
        if (map.root || map.beginNode != end)
            throw std::invalid_argument(DestructorMessage);
        return nodes;
    }
    if (!map.root || !map.beginNode || map.beginNode == end)
        throw std::invalid_argument(DestructorMessage);
    struct Entry {
        WebRtcStatsMapNode* node;
        WebRtcStatsMapNode* parent;
        bool expanded;
    };
    std::vector<Entry> pending{{map.root, end, false}};
    std::uint64_t seen = 0;
    while (!pending.empty()) {
        Entry& current = pending.back();
        if (current.expanded) {
            nodes.push_back(current.node);
            pending.pop_back();
            continue;
        }
        current.expanded = true;
        if (++seen > map.size)
            throw std::invalid_argument(DestructorMessage);
        RequireWritable(current.node, sizeof(WebRtcStatsMapNode), DestructorMessage);
        const WebRtcStatsMapLinks links = current.node->links;
        if (links.parent != current.parent)
            throw std::invalid_argument(DestructorMessage);
        ValidateString(current.node->id);
        ValidateStats(current.node->stats);
        WebRtcStatsMapNode* const node = current.node;
        if (links.right)
            pending.push_back({links.right, node, false});
        if (links.left)
            pending.push_back({links.left, node, false});
    }
    if (seen != map.size)
        throw std::invalid_argument(DestructorMessage);
    const WebRtcStatsMapNode* leftmost = map.root;
    while (leftmost->links.left)
        leftmost = leftmost->links.left;
    if (leftmost != map.beginNode)
        throw std::invalid_argument(DestructorMessage);
    return nodes;
}

void DeleteReport(WebRtcRtcStatsReport* report, const std::vector<WebRtcStatsMapNode*>& nodes) {
    for (WebRtcStatsMapNode* node : nodes) {
        if (const void* stats = node->stats) {
            const auto* vtable = *static_cast<const void* const* const*>(stats);
            reinterpret_cast<DeletingDestructor>(const_cast<void*>(vtable[DeletingDestructorSlot]))(stats);
        }
        if (node->id.capacity > WebRtcGuestStringInlineCapacity)
            GuestStringStorage::Deallocate(node->id.pointer, node->id.capacity + 1);
        _ZdlPv_nid_postfix(node);
    }
    _ZdlPv_nid_postfix(report);
}

void ReleaseReport(const WebRtcRtcStatsReport* constReport) {
    auto* report = const_cast<WebRtcRtcStatsReport*>(constReport);
    RequireWritable(report, sizeof(*report), DestructorMessage);
    std::atomic_ref<std::int32_t> count(report->refCount);
    const std::int32_t current = count.load(std::memory_order_acquire);
    if (current <= 0)
        throw std::invalid_argument(DestructorMessage);
    std::vector<WebRtcStatsMapNode*> nodes;
    if (current == 1)
        nodes = CollectPostOrder(report);
    if (count.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
    if (current != 1)
        nodes = CollectPostOrder(report);
    DeleteReport(report, nodes);
}

}

extern "C" {

WebRtcRtcStatsReportConstIterator* APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(WebRtcRtcStatsReportConstIterator* iterator) {
    return Increment(iterator);
}

WebRtcRtcStatsReportConstIterator* APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(WebRtcRtcStatsReportConstIterator* iterator, int) {
    return Increment(iterator);
}

bool APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(const WebRtcRtcStatsReportConstIterator* iterator, const WebRtcRtcStatsReportConstIterator* other) {
    const auto left = ReadGuest<WebRtcRtcStatsReportConstIterator>(iterator, CompareMessage);
    const auto right = ReadGuest<WebRtcRtcStatsReportConstIterator>(other, CompareMessage);
    return left.node != right.node;
}

const void* APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(const WebRtcRtcStatsReportConstIterator* iterator) {
    const void* stats = EntryStats(iterator, DereferenceMessage);
    if (!stats)
        throw std::invalid_argument(DereferenceMessage);
    return stats;
}

const void* APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(const WebRtcRtcStatsReportConstIterator* iterator) {
    return EntryStats(iterator, ArrowMessage);
}

WebRtcRtcStatsReportConstIterator* APS5_VABI _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(WebRtcRtcStatsReportConstIterator* result, const WebRtcRtcStatsReport* report) {
    return MakeIterator(result, report, false, BeginMessage);
}

WebRtcRtcStatsReportConstIterator* APS5_VABI _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(WebRtcRtcStatsReportConstIterator* result, const WebRtcRtcStatsReport* report) {
    return MakeIterator(result, report, true, EndMessage);
}

void APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(WebRtcRtcStatsReportConstIterator* iterator) {
    const auto value = ReadGuest<WebRtcRtcStatsReportConstIterator>(iterator, DestructorMessage);
    if (value.report)
        ReleaseReport(value.report);
}

}
