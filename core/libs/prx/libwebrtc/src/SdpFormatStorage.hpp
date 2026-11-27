#pragma once

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
#include <unordered_set>
#include <utility>
#include <vector>

namespace WebRtcSdpStorage {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

inline void RequireWritable(void* address, std::size_t bytes, const char* message) {
    if (!address)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(address, bytes))
        throw std::invalid_argument(message);
}

inline void CopyInChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!source)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

inline void ProbeReadableChunks(const void* address, std::size_t totalBytes, const char* message) {
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

inline WebRtcStringMapNode* EndNode(WebRtcStringMap& map) {
    return reinterpret_cast<WebRtcStringMapNode*>(&map.root);
}

inline const WebRtcStringMapNode* EndNode(const WebRtcStringMap& map) {
    return reinterpret_cast<const WebRtcStringMapNode*>(&map.root);
}

inline void InitStringEmpty(WebRtcGuestString& string) {
    string.reserved = 0;
    string.buffer[0] = '\0';
    string.size = 0;
    string.capacity = WebRtcGuestStringInlineCapacity;
}

inline void ReleaseString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
    InitStringEmpty(string);
}

inline void ValidateString(const WebRtcGuestString& string, const char* message) {
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

inline void CopyString(const WebRtcGuestString& source, WebRtcGuestString& destination, const char* message) {
    InitStringEmpty(destination);
    if (source.size <= WebRtcGuestStringInlineCapacity) {
        if (source.size > 0)
            CopyInChecked(destination.buffer, WebRtcGuestStringData(source), source.size, message);
        destination.buffer[source.size] = '\0';
        destination.size = source.size;
        return;
    }
    const std::size_t capacity = source.size | WebRtcGuestStringInlineCapacity;
    if (capacity > std::numeric_limits<std::size_t>::max() - 1)
        throw std::bad_alloc();
    StringGuard copy;
    copy.value.pointer = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    copy.value.capacity = capacity;
    copy.active = true;
    CopyInChecked(copy.value.pointer, source.pointer, source.size, message);
    copy.value.pointer[source.size] = '\0';
    copy.value.size = source.size;
    destination = copy.value;
    copy.active = false;
}

inline void MoveString(WebRtcGuestString& destination, WebRtcGuestString& source) {
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

template <typename Visitor>
void VisitMapNodes(const WebRtcStringMap& map, const WebRtcStringMapNode* end, const char* message, Visitor visit) {
    if (!map.root) {
        if (map.size || map.beginNode != end) throw std::invalid_argument(message);
        return;
    }
    struct Entry {
        WebRtcStringMapNode* address;
        const WebRtcStringMapNode* parent;
        std::size_t parentIndex;
        bool left;
    };
    std::vector<Entry> pending{{map.root, end, 0, false}};
    std::unordered_set<WebRtcStringMapNode*> visited;
    const WebRtcStringMapNode* leftmost = map.root;
    while (!pending.empty()) {
        const auto current = pending.back();
        pending.pop_back();
        const auto index = visited.size();
        if (index >= map.size || !visited.insert(current.address).second)
            throw std::invalid_argument(message);
        WebRtcStringMapNode node{};
        CopyInChecked(&node, current.address, sizeof(node), message);
        if (node.links.parent != current.parent) throw std::invalid_argument(message);
        ValidateString(node.key, message);
        ValidateString(node.value, message);
        if (current.address == leftmost && node.links.left)
            leftmost = node.links.left;
        visit(current.address, node, current.parentIndex, current.left);
        if (node.links.right) pending.push_back({node.links.right, current.address, index, false});
        if (node.links.left) pending.push_back({node.links.left, current.address, index, true});
    }
    if (visited.size() != map.size || leftmost != map.beginNode)
        throw std::invalid_argument(message);
}

inline void ValidateMapNodes(const WebRtcStringMap& map, const char* message, std::vector<WebRtcStringMapNode*>& outNodes) {
    outNodes.clear();
    VisitMapNodes(map, EndNode(map), message, [&](WebRtcStringMapNode* address, const WebRtcStringMapNode&, std::size_t, bool) {
        outNodes.push_back(address);
    });
}

inline void DestroyMapNodes(const std::vector<WebRtcStringMapNode*>& nodes, WebRtcStringMap& map) {
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
            if (!node) continue;
            ReleaseString(node->value);
            ReleaseString(node->key);
            _ZdlPv_nid_postfix(node);
        }
    }
};

inline std::vector<WebRtcStringMapNode*> CloneMap(const WebRtcStringMap& source, const WebRtcStringMapNode* sourceEnd,
                                                 WebRtcStringMap& destination, const char* message) {
    destination = {EndNode(destination), nullptr, 0};
    NodeGuard guard;
    VisitMapNodes(source, sourceEnd, message, [&](const WebRtcStringMapNode* address, const WebRtcStringMapNode& snapshot,
                                                 std::size_t parentIndex, bool left) {
        guard.nodes.push_back(nullptr);
        auto* node = new (_Znwm_nid_postfix(sizeof(WebRtcStringMapNode))) WebRtcStringMapNode{};
        guard.nodes.back() = node;
        InitStringEmpty(node->key);
        InitStringEmpty(node->value);
        CopyString(snapshot.key, node->key, message);
        CopyString(snapshot.value, node->value, message);
        node->links.isBlack = snapshot.links.isBlack;
        if (!destination.root) {
            destination.root = node;
            node->links.parent = EndNode(destination);
        } else {
            auto* parent = guard.nodes[parentIndex];
            (left ? parent->links.left : parent->links.right) = node;
            node->links.parent = parent;
        }
        if (address == source.beginNode) destination.beginNode = node;
    });
    destination.size = guard.nodes.size();
    guard.active = false;
    return std::move(guard.nodes);
}

inline std::uint64_t ModeCount(const WebRtcScalabilityModeVector& modes) {
    return modes.sizeAndIsAllocated >> 1;
}

inline bool ModesAllocated(const WebRtcScalabilityModeVector& modes) {
    return (modes.sizeAndIsAllocated & 1) != 0;
}

inline void ValidateModes(const WebRtcScalabilityModeVector& modes, const char* message) {
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

inline void CopyModes(const WebRtcScalabilityModeVector& source, WebRtcScalabilityModeVector& destination) {
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
