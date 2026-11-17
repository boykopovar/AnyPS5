/* Derived from api/audio_codecs/audio_format.cc and api/audio_codecs/audio_format.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
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
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

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

void ProbeReadableChunks(const void* address, std::size_t totalBytes, const char* message) {
    std::array<char, 4096> scratch{};
    const auto* cursor = static_cast<const char*>(address);
    std::size_t remaining = totalBytes;
    while (remaining > 0) {
        const std::size_t chunk = std::min(remaining, scratch.size());
        CopyInChecked(scratch.data(), cursor, chunk, message);
        cursor += chunk;
        remaining -= chunk;
    }
}

void CopyOutChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

WebRtcSdpAudioFormat ReadFormat(const WebRtcSdpAudioFormat* format, const char* message) {
    WebRtcSdpAudioFormat host{};
    CopyInChecked(&host, format, sizeof(host), message);
    return host;
}

WebRtcStringMapNode* GuestEndNode(const WebRtcSdpAudioFormat* format) {
    return reinterpret_cast<WebRtcStringMapNode*>(reinterpret_cast<std::uintptr_t>(format) + offsetof(WebRtcSdpAudioFormat, parameters) + offsetof(WebRtcStringMap, root));
}

void ResetMap(WebRtcStringMap& map, WebRtcStringMapNode* end) {
    map.beginNode = end;
    map.root = nullptr;
    map.size = 0;
}

void InitStringEmpty(WebRtcGuestString& string) {
    string.reserved = 0;
    string.buffer[0] = '\0';
    string.size = 0;
    string.capacity = WebRtcGuestStringInlineCapacity;
}

void ReleaseString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
    InitStringEmpty(string);
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
    ProbeReadableChunks(string.pointer, string.size + 1, message);
}

struct StringGuard {
    WebRtcGuestString value{};
    bool active = false;

    StringGuard() { InitStringEmpty(value); }
    ~StringGuard() {
        if (active)
            ReleaseString(value);
    }
};

void CopyString(const WebRtcGuestString& source, WebRtcGuestString& destination, const char* message) {
    InitStringEmpty(destination);
    if (source.size <= WebRtcGuestStringInlineCapacity) {
        if (source.size > 0 && source.capacity > WebRtcGuestStringInlineCapacity)
            CopyInChecked(destination.buffer, source.pointer, source.size, message);
        else if (source.size > 0)
            std::memcpy(destination.buffer, source.buffer, source.size);
        destination.buffer[source.size] = '\0';
        destination.size = source.size;
        return;
    }
    const std::size_t capacity = source.size | WebRtcGuestStringInlineCapacity;
    if (capacity > std::numeric_limits<std::size_t>::max() - 1)
        throw std::bad_alloc();
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    try {
        CopyInChecked(data, source.pointer, source.size, message);
    } catch (...) {
        GuestStringStorage::Deallocate(data, capacity + 1);
        throw;
    }
    data[source.size] = '\0';
    destination.pointer = data;
    destination.size = source.size;
    destination.capacity = capacity;
}

void MoveString(WebRtcGuestString& destination, WebRtcGuestString& source) {
    if (source.capacity > WebRtcGuestStringInlineCapacity) {
        destination = source;
        InitStringEmpty(source);
        return;
    }
    InitStringEmpty(destination);
    if (source.size > 0)
        std::memcpy(destination.buffer, source.buffer, source.size);
    destination.buffer[source.size] = '\0';
    destination.size = source.size;
    InitStringEmpty(source);
}

struct ValidatedNode {
    WebRtcStringMapNode* address;
    WebRtcStringMapNode copy;
};

void ValidateMapNodes(const WebRtcStringMap& map, const WebRtcStringMapNode* end, const char* message, std::vector<ValidatedNode>& outNodes) {
    outNodes.clear();
    if (map.size == 0) {
        if (map.root != nullptr || map.beginNode != end)
            throw std::invalid_argument(message);
        return;
    }
    if (!map.root || !map.beginNode || map.beginNode == end)
        throw std::invalid_argument(message);
    struct Entry {
        WebRtcStringMapNode* node;
        const WebRtcStringMapNode* expectedParent;
    };
    std::vector<Entry> pending;
    pending.push_back({map.root, end});
    while (!pending.empty()) {
        const Entry current = pending.back();
        pending.pop_back();
        if (outNodes.size() >= map.size)
            throw std::invalid_argument(message);
        WebRtcStringMapNode nodeCopy{};
        CopyInChecked(&nodeCopy, current.node, sizeof(nodeCopy), message);
        if (nodeCopy.links.parent != current.expectedParent)
            throw std::invalid_argument(message);
        ValidateString(nodeCopy.key, message);
        ValidateString(nodeCopy.value, message);
        outNodes.push_back({current.node, nodeCopy});
        if (nodeCopy.links.right)
            pending.push_back({nodeCopy.links.right, current.node});
        if (nodeCopy.links.left)
            pending.push_back({nodeCopy.links.left, current.node});
    }
    if (outNodes.size() != map.size)
        throw std::invalid_argument(message);
    const WebRtcStringMapNode* leftmost = map.root;
    WebRtcStringMapNode* left = outNodes.front().copy.links.left;
    while (left) {
        leftmost = left;
        WebRtcStringMapNode* next = nullptr;
        CopyInChecked(&next, &left->links.left, sizeof(next), message);
        left = next;
    }
    if (leftmost != map.beginNode)
        throw std::invalid_argument(message);
}

void DestroyMapNodes(const std::vector<ValidatedNode>& nodes, WebRtcStringMap& map, WebRtcStringMapNode* end) {
    for (const ValidatedNode& node : nodes) {
        WebRtcStringMapNode copy = node.copy;
        ReleaseString(copy.value);
        ReleaseString(copy.key);
        _ZdlPv_nid_postfix(node.address);
    }
    ResetMap(map, end);
}

struct NodeGuard {
    std::vector<WebRtcStringMapNode*> nodes;
    bool active = true;

    ~NodeGuard() {
        if (!active)
            return;
        for (WebRtcStringMapNode* node : nodes) {
            ReleaseString(node->value);
            ReleaseString(node->key);
            _ZdlPv_nid_postfix(node);
        }
    }
};

void CloneMap(const WebRtcStringMap& source, const WebRtcStringMapNode* sourceEnd, WebRtcStringMap& destination, WebRtcStringMapNode* destinationEnd, const char* message) {
    std::vector<ValidatedNode> validated;
    ValidateMapNodes(source, sourceEnd, message, validated);
    ResetMap(destination, destinationEnd);
    if (source.size == 0)
        return;

    NodeGuard guard;
    struct CloneTask {
        const WebRtcStringMapNode* source;
        WebRtcStringMapNode** slot;
        WebRtcStringMapNode* parent;
    };
    std::vector<CloneTask> tasks;
    tasks.push_back({source.root, &destination.root, destinationEnd});
    while (!tasks.empty()) {
        const CloneTask task = tasks.back();
        tasks.pop_back();
        WebRtcStringMapNode sourceNode{};
        CopyInChecked(&sourceNode, task.source, sizeof(sourceNode), message);
        void* storage = _Znwm_nid_postfix(sizeof(WebRtcStringMapNode));
        auto* node = new (storage) WebRtcStringMapNode{};
        node->links.parent = task.parent;
        node->links.isBlack = sourceNode.links.isBlack;
        InitStringEmpty(node->key);
        InitStringEmpty(node->value);
        guard.nodes.push_back(node);
        CopyString(sourceNode.key, node->key, message);
        CopyString(sourceNode.value, node->value, message);
        *task.slot = node;
        if (task.source == source.beginNode)
            destination.beginNode = node;
        if (sourceNode.links.right)
            tasks.push_back({sourceNode.links.right, &node->links.right, node});
        if (sourceNode.links.left)
            tasks.push_back({sourceNode.links.left, &node->links.left, node});
    }
    destination.size = source.size;
    guard.active = false;
}

void ValidateFormat(const WebRtcSdpAudioFormat& format, const WebRtcStringMapNode* end, const char* message, std::vector<ValidatedNode>& outNodes) {
    ValidateString(format.name, message);
    ValidateMapNodes(format.parameters, end, message, outNodes);
}

}

extern "C" {

void WebRtcValidateSdpAudioFormat_nid_no_patch(const WebRtcSdpAudioFormat* format, const char* message) {
    const WebRtcSdpAudioFormat snapshot = ReadFormat(format, message);
    std::vector<ValidatedNode> nodes;
    ValidateFormat(snapshot, GuestEndNode(format), message, nodes);
}

void APS5_VABI _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(WebRtcSdpAudioFormat* format, const WebRtcSdpAudioFormat* other) {
    constexpr const char* message = "SdpAudioFormat copy constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    const WebRtcSdpAudioFormat snapshot = ReadFormat(other, message);
    WebRtcSdpAudioFormat result = ReadFormat(format, message);
    ValidateString(snapshot.name, message);

    StringGuard nameGuard;
    CopyString(snapshot.name, nameGuard.value, message);
    nameGuard.active = true;

    CloneMap(snapshot.parameters, GuestEndNode(other), result.parameters, GuestEndNode(format), message);

    result.name = nameGuard.value;
    result.clockrateHz = snapshot.clockrateHz;
    result.numChannels = snapshot.numChannels;
    CopyOutChecked(format, &result, sizeof(result), message);
    nameGuard.active = false;
}

void APS5_VABI _ZN6webrtc14SdpAudioFormatC1EOS0__nid_postfix(WebRtcSdpAudioFormat* format, WebRtcSdpAudioFormat* other) {
    constexpr const char* message = "SdpAudioFormat move constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    RequireWritable(other, sizeof(*other), message);
    if (format == other)
        throw std::invalid_argument(message);

    WebRtcSdpAudioFormat source = ReadFormat(other, message);
    WebRtcSdpAudioFormat result = ReadFormat(format, message);
    std::vector<ValidatedNode> validated;
    ValidateFormat(source, GuestEndNode(other), message, validated);
    if (source.parameters.size != 0)
        RequireWritable(&source.parameters.root->links.parent, sizeof(source.parameters.root->links.parent), message);

    MoveString(result.name, source.name);
    result.clockrateHz = source.clockrateHz;
    result.numChannels = source.numChannels;
    if (source.parameters.size == 0) {
        ResetMap(result.parameters, GuestEndNode(format));
    } else {
        result.parameters = source.parameters;
        WebRtcStringMapNode* const end = GuestEndNode(format);
        CopyOutChecked(&result.parameters.root->links.parent, &end, sizeof(end), message);
        ResetMap(source.parameters, GuestEndNode(other));
    }
    CopyOutChecked(format, &result, sizeof(result), message);
    CopyOutChecked(other, &source, sizeof(source), message);
}

WebRtcSdpAudioFormat* APS5_VABI _ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(WebRtcSdpAudioFormat* format, const WebRtcSdpAudioFormat* other) {
    constexpr const char* message = "SdpAudioFormat copy assignment requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    const WebRtcSdpAudioFormat snapshot = ReadFormat(other, message);
    WebRtcSdpAudioFormat result = ReadFormat(format, message);

    std::vector<ValidatedNode> existingNodes;
    ValidateFormat(result, GuestEndNode(format), message, existingNodes);
    if (format == other)
        return format;

    ValidateString(snapshot.name, message);

    StringGuard nameGuard;
    CopyString(snapshot.name, nameGuard.value, message);
    nameGuard.active = true;

    WebRtcStringMap clonedMap{};
    CloneMap(snapshot.parameters, GuestEndNode(other), clonedMap, GuestEndNode(format), message);

    DestroyMapNodes(existingNodes, result.parameters, GuestEndNode(format));
    ReleaseString(result.name);

    result.name = nameGuard.value;
    nameGuard.active = false;
    result.clockrateHz = snapshot.clockrateHz;
    result.numChannels = snapshot.numChannels;
    result.parameters = clonedMap;
    CopyOutChecked(format, &result, sizeof(result), message);
    return format;
}

void APS5_VABI _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(WebRtcSdpAudioFormat* format) {
    constexpr const char* message = "SdpAudioFormat destructor requires a valid object";
    RequireWritable(format, sizeof(*format), message);
    WebRtcSdpAudioFormat host = ReadFormat(format, message);
    std::vector<ValidatedNode> nodes;
    ValidateFormat(host, GuestEndNode(format), message, nodes);
    DestroyMapNodes(nodes, host.parameters, GuestEndNode(format));
    ReleaseString(host.name);
    CopyOutChecked(format, &host, sizeof(host), message);
}

}
