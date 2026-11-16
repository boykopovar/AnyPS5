/* Derived from stats/rtc_stats_report.cc, api/stats/rtc_stats_report.h, api/stats/rtc_stats.h and api/ref_counted_base.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libc/libc.hpp"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t DeletingDestructorSlot = 1;
constexpr std::size_t MaxUnboundedWalkSteps = std::size_t{1} << 20;

using DeleteStatsFunction = void(APS5_VABI*)(const WebRtcRtcStats*);

std::mutex g_reportMutex;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void RequireWritable(void* address, std::size_t bytes, const char* message) {
    if (!address)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(address, bytes))
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

void RequireAlignedReport(const WebRtcRtcStatsReport* report, const char* message) {
    if (!report || reinterpret_cast<std::uintptr_t>(report) % alignof(WebRtcRtcStatsReport) != 0)
        throw std::invalid_argument(message);
}

const WebRtcRtcStatsMapNode* EndNode(const WebRtcRtcStatsMap& map) {
    return reinterpret_cast<const WebRtcRtcStatsMapNode*>(&map.root);
}

const WebRtcRtcStatsMapNode* EndNode(const WebRtcRtcStatsReport* report) {
    return reinterpret_cast<const WebRtcRtcStatsMapNode*>(&report->stats.root);
}

void ValidateString(const WebRtcGuestString& string, const char* message) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity)
        throw std::invalid_argument(message);
    if (string.capacity <= WebRtcGuestStringInlineCapacity)
        return;
    if (!string.pointer || string.size > std::numeric_limits<std::size_t>::max() - 1)
        throw std::invalid_argument(message);
    if (string.capacity + 1 >= GuestStringStorage::LargeAllocation) {
        void* raw = nullptr;
        CopyInChecked(&raw, static_cast<const void* const*>(static_cast<const void*>(string.pointer)) - 1, sizeof(raw), message);
    }
}

void ReleaseString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
    string.reserved = 0;
    string.buffer[0] = '\0';
    string.size = 0;
    string.capacity = WebRtcGuestStringInlineCapacity;
}

void ValidateDeleteSlot(const WebRtcRtcStats* stats, const char* message) {
    if (!stats)
        return;
    const void* const* vtable = nullptr;
    CopyInChecked(&vtable, stats, sizeof(vtable), message);
    if (!vtable)
        throw std::invalid_argument(message);
    const void* slot = nullptr;
    CopyInChecked(&slot, vtable + DeletingDestructorSlot, sizeof(slot), message);
    if (!slot)
        throw std::invalid_argument(message);
}

void CollectValidatedNodes(const WebRtcRtcStatsReport* reportPtr, const char* message, std::vector<WebRtcRtcStatsMapNode*>& outNodes) {
    outNodes.clear();
    const WebRtcRtcStatsMapNode* const end = EndNode(reportPtr);
    WebRtcRtcStatsMap map{};
    CopyInChecked(&map, &reportPtr->stats, sizeof(map), message);
    if (map.size == 0) {
        if (map.root != nullptr || map.beginNode != end)
            throw std::invalid_argument(message);
        return;
    }
    if (!map.root || !map.beginNode || map.beginNode == end)
        throw std::invalid_argument(message);
    struct Entry {
        WebRtcRtcStatsMapNode* node;
        const WebRtcRtcStatsMapNode* expectedParent;
    };
    std::vector<Entry> pending;
    pending.push_back({map.root, end});
    while (!pending.empty()) {
        const Entry current = pending.back();
        pending.pop_back();
        if (outNodes.size() >= map.size)
            throw std::invalid_argument(message);
        RequireWritable(current.node, sizeof(*current.node), message);
        WebRtcRtcStatsMapNode nodeCopy{};
        CopyInChecked(&nodeCopy, current.node, sizeof(nodeCopy), message);
        if (nodeCopy.links.parent != current.expectedParent)
            throw std::invalid_argument(message);
        ValidateString(nodeCopy.key, message);
        ValidateDeleteSlot(nodeCopy.value, message);
        outNodes.push_back(current.node);
        if (nodeCopy.links.right)
            pending.push_back({nodeCopy.links.right, current.node});
        if (nodeCopy.links.left)
            pending.push_back({nodeCopy.links.left, current.node});
    }
    if (outNodes.size() != map.size)
        throw std::invalid_argument(message);
    const WebRtcRtcStatsMapNode* leftmost = map.root;
    std::size_t leftSteps = 0;
    while (true) {
        if (++leftSteps > map.size)
            throw std::invalid_argument(message);
        WebRtcRtcStatsMapNode leftCopy{};
        CopyInChecked(&leftCopy, leftmost, sizeof(leftCopy), message);
        if (!leftCopy.links.left)
            break;
        leftmost = leftCopy.links.left;
    }
    if (leftmost != map.beginNode)
        throw std::invalid_argument(message);
}

WebRtcRtcStatsReportConstIterator* MakeIterator(WebRtcRtcStatsReportConstIterator* result, const WebRtcRtcStatsReport* report, bool isBegin, const char* message) {
    RequireWritable(result, sizeof(*result), message);
    RequireAlignedReport(report, message);
    std::lock_guard lock(g_reportMutex);
    RequireWritable(const_cast<WebRtcRtcStatsReport*>(report), sizeof(*report), message);
    WebRtcRtcStatsMap statsCopy{};
    CopyInChecked(&statsCopy, &report->stats, sizeof(statsCopy), message);
    const WebRtcRtcStatsMapNode* const end = EndNode(report);
    if (statsCopy.size == 0) {
        if (statsCopy.root != nullptr || statsCopy.beginNode != end)
            throw std::invalid_argument(message);
    } else if (!statsCopy.root || !statsCopy.beginNode || statsCopy.beginNode == end) {
        throw std::invalid_argument(message);
    }
    std::int32_t expectedRef = report->refCount.load(std::memory_order_acquire);
    while (true) {
        if (expectedRef <= 0 || expectedRef == std::numeric_limits<std::int32_t>::max())
            throw std::invalid_argument(message);
        if (report->refCount.compare_exchange_weak(expectedRef, expectedRef + 1, std::memory_order_acq_rel, std::memory_order_acquire))
            break;
    }
    const WebRtcRtcStatsReportConstIterator out{report, isBegin ? statsCopy.beginNode : end};
    CopyOutChecked(result, &out, sizeof(out), message);
    return result;
}

const WebRtcRtcStats* DereferenceIterator(const WebRtcRtcStatsReportConstIterator* iterator, const char* message) {
    WebRtcRtcStatsReportConstIterator itCopy{};
    CopyInChecked(&itCopy, iterator, sizeof(itCopy), message);
    if (!itCopy.it)
        throw std::invalid_argument(message);
    if (itCopy.report) {
        WebRtcRtcStatsMap statsCopy{};
        CopyInChecked(&statsCopy, &itCopy.report->stats, sizeof(statsCopy), message);
        if (itCopy.it == EndNode(itCopy.report))
            throw std::invalid_argument(message);
    }
    WebRtcRtcStatsMapNode nodeCopy{};
    CopyInChecked(&nodeCopy, itCopy.it, sizeof(nodeCopy), message);
    if (!nodeCopy.value)
        throw std::invalid_argument(message);
    return nodeCopy.value;
}

}

extern "C" {

WebRtcRtcStatsReportConstIterator* APS5_VABI _ZNK6webrtc14RTCStatsReport5beginEv_nid_postfix(WebRtcRtcStatsReportConstIterator* result, const WebRtcRtcStatsReport* report) {
    return MakeIterator(result, report, true, "RTCStatsReport::begin requires valid result and report");
}

WebRtcRtcStatsReportConstIterator* APS5_VABI _ZNK6webrtc14RTCStatsReport3endEv_nid_postfix(WebRtcRtcStatsReportConstIterator* result, const WebRtcRtcStatsReport* report) {
    return MakeIterator(result, report, false, "RTCStatsReport::end requires valid result and report");
}

bool APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorneERKS1__nid_postfix(const WebRtcRtcStatsReportConstIterator* iterator, const WebRtcRtcStatsReportConstIterator* other) {
    constexpr const char* message = "RTCStatsReport::ConstIterator::operator!= requires valid iterators";
    WebRtcRtcStatsReportConstIterator left{};
    WebRtcRtcStatsReportConstIterator right{};
    CopyInChecked(&left, iterator, sizeof(left), message);
    CopyInChecked(&right, other, sizeof(right), message);
    return left.it != right.it;
}

WebRtcRtcStatsReportConstIterator* APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(WebRtcRtcStatsReportConstIterator* iterator) {
    constexpr const char* message = "RTCStatsReport::ConstIterator::operator++ requires a valid incrementable iterator";
    RequireWritable(iterator, sizeof(*iterator), message);
    WebRtcRtcStatsReportConstIterator itCopy{};
    CopyInChecked(&itCopy, iterator, sizeof(itCopy), message);
    if (!itCopy.it)
        throw std::invalid_argument(message);
    std::size_t maxSteps = MaxUnboundedWalkSteps;
    if (itCopy.report) {
        WebRtcRtcStatsMap statsCopy{};
        CopyInChecked(&statsCopy, &itCopy.report->stats, sizeof(statsCopy), message);
        if (itCopy.it == EndNode(itCopy.report))
            throw std::invalid_argument(message);
        maxSteps = static_cast<std::size_t>(statsCopy.size) + 1;
    }
    WebRtcRtcStatsMapNode nodeCopy{};
    CopyInChecked(&nodeCopy, itCopy.it, sizeof(nodeCopy), message);
    if (nodeCopy.links.right) {
        const WebRtcRtcStatsMapNode* next = nodeCopy.links.right;
        std::size_t steps = 0;
        while (true) {
            if (++steps > maxSteps)
                throw std::invalid_argument(message);
            WebRtcRtcStatsMapNode childCopy{};
            CopyInChecked(&childCopy, next, sizeof(childCopy), message);
            if (!childCopy.links.left)
                break;
            next = childCopy.links.left;
        }
        const WebRtcRtcStatsReportConstIterator updated{itCopy.report, next};
        CopyOutChecked(iterator, &updated, sizeof(updated), message);
        return iterator;
    }
    const WebRtcRtcStatsMapNode* current = itCopy.it;
    const WebRtcRtcStatsMapNode* parent = nodeCopy.links.parent;
    std::size_t steps = 0;
    while (parent) {
        if (++steps > maxSteps)
            throw std::invalid_argument(message);
        const WebRtcRtcStatsMapNode* parentLeft = nullptr;
        CopyInChecked(&parentLeft, &parent->links.left, sizeof(parentLeft), message);
        if (parentLeft == current)
            break;
        const WebRtcRtcStatsMapNode* nextParent = nullptr;
        CopyInChecked(&nextParent, &parent->links.parent, sizeof(nextParent), message);
        current = parent;
        parent = nextParent;
    }
    if (!parent)
        throw std::invalid_argument(message);
    const WebRtcRtcStatsReportConstIterator updated{itCopy.report, parent};
    CopyOutChecked(iterator, &updated, sizeof(updated), message);
    return iterator;
}

WebRtcRtcStatsReportConstIterator* APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorppEi_nid_postfix(WebRtcRtcStatsReportConstIterator* iterator, int) {
    return _ZN6webrtc14RTCStatsReport13ConstIteratorppEv_nid_postfix(iterator);
}

const WebRtcRtcStats* APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratordeEv_nid_postfix(const WebRtcRtcStatsReportConstIterator* iterator) {
    return DereferenceIterator(iterator, "RTCStatsReport::ConstIterator::operator* requires a valid dereferenceable iterator");
}

const WebRtcRtcStats* APS5_VABI _ZNK6webrtc14RTCStatsReport13ConstIteratorptEv_nid_postfix(const WebRtcRtcStatsReportConstIterator* iterator) {
    return DereferenceIterator(iterator, "RTCStatsReport::ConstIterator::operator-> requires a valid dereferenceable iterator");
}

void APS5_VABI _ZN6webrtc14RTCStatsReport13ConstIteratorD1Ev_nid_postfix(WebRtcRtcStatsReportConstIterator* iterator) {
    constexpr const char* message = "RTCStatsReport::ConstIterator destructor requires a valid iterator";
    RequireWritable(iterator, sizeof(*iterator), message);
    WebRtcRtcStatsReportConstIterator itCopy{};
    CopyInChecked(&itCopy, iterator, sizeof(itCopy), message);
    if (itCopy.report) {
        auto* report = const_cast<WebRtcRtcStatsReport*>(itCopy.report);
        RequireAlignedReport(report, message);
        std::vector<WebRtcRtcStatsMapNode*> nodes;
        bool shouldDestroy = false;
        {
            std::lock_guard lock(g_reportMutex);
            RequireWritable(report, sizeof(*report), message);
            std::int32_t currentRef = report->refCount.load(std::memory_order_acquire);
            while (true) {
                if (currentRef <= 0)
                    throw std::invalid_argument(message);
                if (currentRef == 1) {
                    if (nodes.empty())
                        CollectValidatedNodes(report, message, nodes);
                } else {
                    nodes.clear();
                }
                if (report->refCount.compare_exchange_weak(currentRef, currentRef - 1, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    shouldDestroy = (currentRef == 1);
                    break;
                }
            }
            if (shouldDestroy) {
                const WebRtcRtcStatsMap emptyMap{EndNode(report), nullptr, 0};
                CopyOutChecked(&report->stats, &emptyMap, sizeof(emptyMap), message);
            }
        }
        if (shouldDestroy) {
            for (WebRtcRtcStatsMapNode* node : nodes) {
                if (node->value)
                    reinterpret_cast<DeleteStatsFunction>(const_cast<void*>(node->value->vtable[DeletingDestructorSlot]))(node->value);
                ReleaseString(node->key);
                _ZdlPv_nid_postfix(node);
            }
            _ZdlPv_nid_postfix(report);
        }
    }
    const WebRtcRtcStatsReportConstIterator cleared{};
    CopyOutChecked(iterator, &cleared, sizeof(cleared), message);
}

}
