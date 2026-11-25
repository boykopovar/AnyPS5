/* Derived from api/rtp_transceiver_interface.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
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
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void CopyInChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!source)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

void ProbeReadableChunks(const void* address, std::size_t bytes, const char* message) {
    std::array<std::byte, 4096> scratch{};
    auto* cursor = static_cast<const std::byte*>(address);
    while (bytes > 0) {
        const std::size_t chunk = std::min(bytes, scratch.size());
        CopyInChecked(scratch.data(), cursor, chunk, message);
        cursor += chunk;
        bytes -= chunk;
    }
}

void ValidateString(const WebRtcGuestString& string, const char* message) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity)
        throw std::invalid_argument(message);
    if (string.capacity <= WebRtcGuestStringInlineCapacity)
        return;
    if (!string.pointer || string.size == std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument(message);
    if (string.capacity + 1 >= GuestStringStorage::LargeAllocation) {
        void* raw = nullptr;
        CopyInChecked(&raw, static_cast<const void* const*>(static_cast<const void*>(string.pointer)) - 1, sizeof(raw), message);
    }
    ProbeReadableChunks(string.pointer, string.size + 1, message);
}

void ReleaseString(const WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
}

template<typename TValue>
std::size_t VectorCount(TValue* begin, TValue* end, TValue* capacityEnd, const char* message) {
    const auto first = reinterpret_cast<std::uintptr_t>(begin);
    const auto last = reinterpret_cast<std::uintptr_t>(end);
    const auto capacity = reinterpret_cast<std::uintptr_t>(capacityEnd);
    if (last < first || capacity < last || (!first && capacity) || first % alignof(TValue) ||
        (last - first) % sizeof(TValue) || (capacity - first) % sizeof(TValue))
        throw std::invalid_argument(message);
    return (last - first) / sizeof(TValue);
}

template<typename TValue>
TValue* ElementAt(TValue* begin, std::size_t index) {
    return reinterpret_cast<TValue*>(reinterpret_cast<std::uintptr_t>(begin) + index * sizeof(TValue));
}

struct MapNodeResource {
    WebRtcStringMapNode* address;
    WebRtcStringMapNode value;
};

struct MapResources {
    std::vector<MapNodeResource> nodes;
};

const MapNodeResource* FindNode(const MapResources& resources, const WebRtcStringMapNode* address) {
    const auto found = std::find_if(resources.nodes.begin(), resources.nodes.end(),
                                    [address](const MapNodeResource& node) { return node.address == address; });
    return found == resources.nodes.end() ? nullptr : &*found;
}

MapResources ValidateMap(const WebRtcStringMap& map, const WebRtcStringMap* address, const char* message) {
    MapResources resources;
    const auto* endNode = reinterpret_cast<const WebRtcStringMapNode*>(&address->root);
    if (map.size == 0) {
        if (map.root || map.beginNode != endNode)
            throw std::invalid_argument(message);
        return resources;
    }
    if (!map.root || !map.beginNode || map.beginNode == endNode)
        throw std::invalid_argument(message);
    struct PendingNode {
        WebRtcStringMapNode* address;
        const WebRtcStringMapNode* parent;
    };
    std::vector<PendingNode> pending{{map.root, endNode}};
    while (!pending.empty()) {
        const PendingNode current = pending.back();
        pending.pop_back();
        if (resources.nodes.size() >= map.size ||
            std::any_of(resources.nodes.begin(), resources.nodes.end(),
                        [current](const MapNodeResource& node) { return node.address == current.address; }))
            throw std::invalid_argument(message);
        if (reinterpret_cast<std::uintptr_t>(current.address) % alignof(WebRtcStringMapNode))
            throw std::invalid_argument(message);
        WebRtcStringMapNode value{};
        CopyInChecked(&value, current.address, sizeof(value), message);
        if (value.links.parent != current.parent)
            throw std::invalid_argument(message);
        ValidateString(value.key, message);
        ValidateString(value.value, message);
        resources.nodes.push_back({current.address, value});
        if (value.links.right)
            pending.push_back({value.links.right, current.address});
        if (value.links.left)
            pending.push_back({value.links.left, current.address});
    }
    if (resources.nodes.size() != map.size)
        throw std::invalid_argument(message);
    const MapNodeResource* leftmost = FindNode(resources, map.root);
    while (leftmost && leftmost->value.links.left)
        leftmost = FindNode(resources, leftmost->value.links.left);
    if (!leftmost || leftmost->address != map.beginNode)
        throw std::invalid_argument(message);
    return resources;
}

struct CodecResources {
    WebRtcRtpCodec value{};
    MapResources parameters;
};

struct EncodingResources {
    WebRtcRtpEncodingParameters value{};
    CodecResources codec;
};

CodecResources ValidateCodec(const WebRtcRtpCodec& codec, const WebRtcRtpCodec* address, const char* message) {
    ValidateString(codec.name, message);
    VectorCount(codec.rtcpFeedback.begin, codec.rtcpFeedback.end, codec.rtcpFeedback.capacityEnd, message);
    return {codec, ValidateMap(codec.parameters, &address->parameters, message)};
}

EncodingResources ValidateEncoding(const WebRtcRtpEncodingParameters* address, const char* message) {
    EncodingResources resources;
    CopyInChecked(&resources.value, address, sizeof(resources.value), message);
    if (resources.value.scalabilityMode.engaged)
        ValidateString(resources.value.scalabilityMode.value, message);
    ValidateString(resources.value.rid, message);
    if (resources.value.codec.engaged)
        resources.codec = ValidateCodec(resources.value.codec.value, &address->codec.value, message);
    return resources;
}

void ReleaseMap(const MapResources& resources) {
    for (const MapNodeResource& node : resources.nodes) {
        ReleaseString(node.value.value);
        ReleaseString(node.value.key);
        _ZdlPv_nid_postfix(node.address);
    }
}

void ReleaseCodec(const CodecResources& resources) {
    ReleaseMap(resources.parameters);
    if (resources.value.rtcpFeedback.begin)
        _ZdlPv_nid_postfix(resources.value.rtcpFeedback.begin);
    ReleaseString(resources.value.name);
}

void ReleaseEncoding(const EncodingResources& resources) {
    if (resources.value.codec.engaged)
        ReleaseCodec(resources.codec);
    ReleaseString(resources.value.rid);
    if (resources.value.scalabilityMode.engaged)
        ReleaseString(resources.value.scalabilityMode.value);
}

}

extern "C" {

void APS5_VABI _ZN6webrtc18RtpTransceiverInitC1Ev_nid_postfix(WebRtcRtpTransceiverInit* init) {
    if (!init || !GuestWritable_nid_no_patch(init, sizeof(*init)))
        throw std::invalid_argument("RtpTransceiverInit requires an object");
    *init = WebRtcRtpTransceiverInit{};
    init->direction = WebRtcRtpTransceiverDirectionSendRecv;
}

void APS5_VABI _ZN6webrtc18RtpTransceiverInitD1Ev_nid_postfix(WebRtcRtpTransceiverInit* init) {
    WebRtcRtpTransceiverInit value{};
    CopyInChecked(&value, init, sizeof(value), "RtpTransceiverInit destructor requires an object");
    const auto streamCount = VectorCount(value.streamIds.begin, value.streamIds.end, value.streamIds.capacityEnd,
                                         "RtpTransceiverInit destructor requires valid stream IDs");
    const auto encodingCount = VectorCount(value.sendEncodings.begin, value.sendEncodings.end, value.sendEncodings.capacityEnd,
                                           "RtpTransceiverInit destructor requires valid send encodings");
    std::vector<WebRtcGuestString> streamIds;
    streamIds.reserve(streamCount);
    for (std::size_t index = 0; index < streamCount; ++index) {
        WebRtcGuestString string{};
        CopyInChecked(&string, ElementAt(value.streamIds.begin, index), sizeof(string),
                      "RtpTransceiverInit destructor requires valid stream IDs");
        ValidateString(string, "RtpTransceiverInit destructor requires valid stream IDs");
        streamIds.push_back(string);
    }
    std::vector<EncodingResources> encodings;
    encodings.reserve(encodingCount);
    for (std::size_t index = 0; index < encodingCount; ++index)
        encodings.push_back(ValidateEncoding(ElementAt(value.sendEncodings.begin, index),
                                             "RtpTransceiverInit destructor requires valid send encodings"));
    for (auto encoding = encodings.rbegin(); encoding != encodings.rend(); ++encoding)
        ReleaseEncoding(*encoding);
    if (value.sendEncodings.begin)
        _ZdlPv_nid_postfix(value.sendEncodings.begin);
    for (auto stream = streamIds.rbegin(); stream != streamIds.rend(); ++stream)
        ReleaseString(*stream);
    if (value.streamIds.begin)
        _ZdlPv_nid_postfix(value.streamIds.begin);
}

}
