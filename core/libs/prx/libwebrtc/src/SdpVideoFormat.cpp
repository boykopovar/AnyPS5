/* Derived from api/video_codecs/sdp_video_format.cc and api/video_codecs/sdp_video_format.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
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

WebRtcStringMapNode* EndNode(WebRtcStringMap& map) {
    return reinterpret_cast<WebRtcStringMapNode*>(&map.root);
}

const WebRtcStringMapNode* EndNode(const WebRtcStringMap& map) {
    return reinterpret_cast<const WebRtcStringMapNode*>(&map.root);
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

void CopyString(const WebRtcGuestString& source, WebRtcGuestString& destination) {
    InitStringEmpty(destination);
    if (source.size <= WebRtcGuestStringInlineCapacity) {
        if (source.size > 0)
            std::memcpy(destination.buffer, WebRtcGuestStringData(source), source.size);
        destination.buffer[source.size] = '\0';
        destination.size = source.size;
        return;
    }
    const std::size_t capacity = source.size | WebRtcGuestStringInlineCapacity;
    if (capacity > std::numeric_limits<std::size_t>::max() - 1)
        throw std::bad_alloc();
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    std::memcpy(data, source.pointer, source.size);
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

void ValidateMapNodes(const WebRtcStringMap& map, const char* message, std::vector<WebRtcStringMapNode*>& outNodes) {
    outNodes.clear();
    const WebRtcStringMapNode* const end = EndNode(map);
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
        outNodes.push_back(current.node);
        if (nodeCopy.links.right)
            pending.push_back({nodeCopy.links.right, current.node});
        if (nodeCopy.links.left)
            pending.push_back({nodeCopy.links.left, current.node});
    }
    if (outNodes.size() != map.size)
        throw std::invalid_argument(message);
    const WebRtcStringMapNode* leftmost = map.root;
    while (leftmost->links.left)
        leftmost = leftmost->links.left;
    if (leftmost != map.beginNode)
        throw std::invalid_argument(message);
}

void DestroyMapNodes(const std::vector<WebRtcStringMapNode*>& nodes, WebRtcStringMap& map) {
    for (WebRtcStringMapNode* node : nodes) {
        ReleaseString(node->value);
        ReleaseString(node->key);
        _ZdlPv_nid_postfix(node);
    }
    map.beginNode = EndNode(map);
    map.root = nullptr;
    map.size = 0;
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

void CloneMap(const WebRtcStringMap& source, WebRtcStringMap& destination, const char* message) {
    std::vector<WebRtcStringMapNode*> validated;
    ValidateMapNodes(source, message, validated);
    destination.beginNode = EndNode(destination);
    destination.root = nullptr;
    destination.size = 0;
    if (source.size == 0)
        return;

    NodeGuard guard;
    struct CloneTask {
        const WebRtcStringMapNode* source;
        WebRtcStringMapNode** slot;
        WebRtcStringMapNode* parent;
    };
    std::vector<CloneTask> tasks;
    tasks.push_back({source.root, &destination.root, EndNode(destination)});
    while (!tasks.empty()) {
        const CloneTask task = tasks.back();
        tasks.pop_back();
        void* storage = _Znwm_nid_postfix(sizeof(WebRtcStringMapNode));
        auto* node = new (storage) WebRtcStringMapNode{};
        node->links.parent = task.parent;
        node->links.isBlack = task.source->links.isBlack;
        InitStringEmpty(node->key);
        InitStringEmpty(node->value);
        guard.nodes.push_back(node);
        CopyString(task.source->key, node->key);
        CopyString(task.source->value, node->value);
        *task.slot = node;
        if (task.source == source.beginNode)
            destination.beginNode = node;
        if (task.source->links.right)
            tasks.push_back({task.source->links.right, &node->links.right, node});
        if (task.source->links.left)
            tasks.push_back({task.source->links.left, &node->links.left, node});
    }
    destination.size = source.size;
    guard.active = false;
}

std::uint64_t ModeCount(const WebRtcScalabilityModeVector& modes) {
    return modes.sizeAndIsAllocated >> 1;
}

bool ModesAllocated(const WebRtcScalabilityModeVector& modes) {
    return (modes.sizeAndIsAllocated & 1) != 0;
}

void ValidateModes(const WebRtcScalabilityModeVector& modes, const char* message) {
    const std::uint64_t count = ModeCount(modes);
    if (!ModesAllocated(modes)) {
        if (count > WebRtcScalabilityModeCount)
            throw std::invalid_argument(message);
        return;
    }
    if (!modes.allocated.data || modes.allocated.capacity < count)
        throw std::invalid_argument(message);
    if (modes.allocated.capacity >= GuestStringStorage::LargeAllocation) {
        void* raw = nullptr;
        CopyInChecked(&raw, static_cast<const void* const*>(static_cast<const void*>(modes.allocated.data)) - 1, sizeof(raw), message);
        if (!raw)
            throw std::invalid_argument(message);
    }
    if (count > 0)
        ProbeReadableChunks(modes.allocated.data, count, message);
}

void CopyModes(const WebRtcScalabilityModeVector& source, WebRtcScalabilityModeVector& destination) {
    if (!ModesAllocated(source)) {
        destination = source;
        return;
    }
    const std::uint64_t count = ModeCount(source);
    const std::uint64_t capacity = std::max<std::uint64_t>(2 * WebRtcScalabilityModeCount, count);
    auto* data = static_cast<std::uint8_t*>(GuestStringStorage::Allocate(capacity));
    std::memcpy(data, source.allocated.data, count);
    destination.allocated.data = data;
    destination.allocated.capacity = capacity;
    destination.sizeAndIsAllocated = source.sizeAndIsAllocated;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(WebRtcSdpVideoFormat* format, const WebRtcSdpVideoFormat* other) {
    constexpr const char* message = "SdpVideoFormat copy constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    WebRtcSdpVideoFormat snapshot{};
    CopyInChecked(&snapshot, other, sizeof(snapshot), message);
    ValidateString(other->name, message);
    ValidateModes(snapshot.scalabilityModes, message);

    StringGuard nameGuard;
    CopyString(other->name, nameGuard.value);
    nameGuard.active = true;

    WebRtcStringMap clonedMap{};
    CloneMap(other->parameters, clonedMap, message);

    const bool hasModes = ModeCount(snapshot.scalabilityModes) != 0;
    WebRtcScalabilityModeVector modes{};
    if (hasModes) {
        try {
            CopyModes(snapshot.scalabilityModes, modes);
        } catch (...) {
            std::vector<WebRtcStringMapNode*> nodes;
            ValidateMapNodes(clonedMap, message, nodes);
            DestroyMapNodes(nodes, clonedMap);
            throw;
        }
    }

    format->name = nameGuard.value;
    nameGuard.active = false;
    format->parameters = clonedMap;
    if (format->parameters.size == 0)
        format->parameters.beginNode = EndNode(format->parameters);
    else
        format->parameters.root->links.parent = EndNode(format->parameters);
    if (hasModes)
        format->scalabilityModes = modes;
    else
        format->scalabilityModes.sizeAndIsAllocated = 0;
}

void APS5_VABI _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(WebRtcSdpVideoFormat* format, WebRtcSdpVideoFormat* other) {
    constexpr const char* message = "SdpVideoFormat move constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    RequireWritable(other, sizeof(*other), message);
    if (format == other)
        throw std::invalid_argument(message);

    std::vector<WebRtcStringMapNode*> validated;
    ValidateString(other->name, message);
    ValidateMapNodes(other->parameters, message, validated);
    ValidateModes(other->scalabilityModes, message);

    MoveString(format->name, other->name);
    if (other->parameters.size == 0) {
        format->parameters.beginNode = EndNode(format->parameters);
        format->parameters.root = nullptr;
        format->parameters.size = 0;
    } else {
        format->parameters.beginNode = other->parameters.beginNode;
        format->parameters.root = other->parameters.root;
        format->parameters.size = other->parameters.size;
        format->parameters.root->links.parent = EndNode(format->parameters);
        other->parameters.beginNode = EndNode(other->parameters);
        other->parameters.root = nullptr;
        other->parameters.size = 0;
    }
    format->scalabilityModes = other->scalabilityModes;
    other->scalabilityModes.sizeAndIsAllocated = 0;
}

void APS5_VABI _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(WebRtcSdpVideoFormat* format) {
    constexpr const char* message = "SdpVideoFormat destructor requires a valid object";
    RequireWritable(format, sizeof(*format), message);
    std::vector<WebRtcStringMapNode*> nodes;
    ValidateString(format->name, message);
    ValidateMapNodes(format->parameters, message, nodes);
    ValidateModes(format->scalabilityModes, message);
    if (ModesAllocated(format->scalabilityModes)) {
        GuestStringStorage::Deallocate(format->scalabilityModes.allocated.data, format->scalabilityModes.allocated.capacity);
        format->scalabilityModes.sizeAndIsAllocated = 0;
    }
    DestroyMapNodes(nodes, format->parameters);
    ReleaseString(format->name);
}

}
