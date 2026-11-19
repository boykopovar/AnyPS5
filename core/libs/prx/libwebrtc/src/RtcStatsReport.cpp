/* Derived from stats/rtc_stats_report.cc and api/stats/rtc_stats_report.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr const char* IncrementMessage = "RTCStatsReport::ConstIterator::operator++ requires an iterator into a valid stats map before its end";
constexpr const char* CompareMessage = "RTCStatsReport::ConstIterator::operator!= requires two readable iterators";
constexpr const char* DereferenceMessage = "RTCStatsReport::ConstIterator::operator* requires an iterator to a stats entry";
constexpr const char* ArrowMessage = "RTCStatsReport::ConstIterator::operator-> requires an iterator to a stats entry";

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

}
