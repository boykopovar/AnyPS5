/* Adapted for AnyPS5 from webrtc 8d78f5de6c27b2c793039989ea381f1428fb0100
 * modules/rtp_rtcp/source/rtcp_packet/transport_feedback_unittest.cc
 * (https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/rtp_rtcp/source/rtcp_packet/transport_feedback_unittest.cc).
 */
/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "prx/libwebrtc/libwebrtc.hpp"

#include "prx/libc/libc.hpp"

#include "prx/libc/include/ApplicationHeap.hpp"

#include "prx/libwebrtc/tests/UpstreamRtcpVectors.hpp"

#include <cstdint>

#include <cstdio>

#include <cstdlib>

#include <cstring>

#include <unordered_set>

#include <vector>

#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtcp::TransportFeedback check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_set<void*> allocations;
std::size_t allocated = 0;
std::size_t freed = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    auto* value = std::malloc(size);
    Require(value != nullptr);
    allocations.insert(value);
    ++allocated;
    return value;
}

void APS5_VABI Free_nid_no_patch(void* value) {
    Require(allocations.erase(value) == 1);
    ++freed;
    std::free(value);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* value, std::size_t size) { return std::realloc(value, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* value, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(value, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** value, std::size_t alignment, std::size_t size) {
    *value = Align_nid_no_patch(alignment, size);
    return *value ? 0 : 12;
}

inline std::uint8_t fromhex(char c) {
    if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
    if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
    return 0;
}

std::vector<std::uint8_t> HexToBytes(const char* hex) {
    std::vector<std::uint8_t> bytes;
    for (; hex[0] && hex[1]; hex += 2) {
        int high = fromhex(hex[0]);
        int low = fromhex(hex[1]);
        bytes.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return bytes;
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch),
        reinterpret_cast<void*>(&PosixAlign_nid_no_patch), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api);

    const auto& cases = WebRtcUpstreamRtcp::FeedbackCases;
    for (const auto& feedbackCase : cases) {
        WebRtcRtcpTransportFeedback feedback;
        _ZN6webrtc4rtcp17TransportFeedbackC1Ev_nid_postfix(&feedback);

        auto payload = HexToBytes(feedbackCase.payloadHex);

        std::vector<std::uint8_t> paddedPayload = payload;
        while (paddedPayload.size() < 18) {
            paddedPayload.push_back(0);
        }

        WebRtcRtcpCommonHeader header;
        header.packetType = 205;
        header.countOrFormat = 15;
        header.paddingSize = feedbackCase.hasPadding ? 1 : 0;
        header.payloadSize = static_cast<std::uint32_t>(paddedPayload.size());
        header.payload = paddedPayload.data();

        std::uint8_t minimalHeader[] = {0x8F, 205, 0, 0};
        WebRtcRtcpCommonHeader commonHeader;
        _ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(&commonHeader, minimalHeader, 4);

        Require(_ZN6webrtc4rtcp17TransportFeedback5ParseERKNS0_12CommonHeaderE_nid_postfix(&feedback, &header) == true);

        std::size_t received = feedback.receivedPackets.begin ? feedback.receivedPackets.end - feedback.receivedPackets.begin : 0;

        Require(feedback.baseSequenceNumber == feedbackCase.baseSeq);
        Require(feedback.sequenceNumberCount == feedbackCase.statusCount);
        Require(feedback.baseTimeTicks == feedbackCase.baseTimeTicks);
        Require(feedback.includeTimestamps == feedbackCase.includeTimestamps);
        Require(received == feedbackCase.receivedCount);

        if (received > 0) {
            Require(feedback.receivedPackets.begin[0].sequenceNumber == feedbackCase.firstRecvSeq);
            Require(feedback.receivedPackets.begin[received - 1].sequenceNumber == feedbackCase.lastRecvSeq);
            Require(feedback.receivedPackets.begin[0].deltaTicks == feedbackCase.firstDeltaTicks);
        }
        if (received > 1) {
            Require(feedback.receivedPackets.begin[received - 1].deltaTicks == feedbackCase.lastDeltaTicks);
        }

        using Destroy = void(APS5_VABI*)(WebRtcRtcpTransportFeedback*);
        reinterpret_cast<Destroy>(feedback.vtable[0])(&feedback);
    }

    return 0;
}
