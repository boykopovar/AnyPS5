// Derived from Source/JavaScriptCore/bytecode/Watchpoint.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace NKWebKit::JscAbi {

enum WatchpointState : std::uint8_t {
    ClearWatchpoint = 0,
    IsWatched = 1,
    IsInvalidated = 2
};

constexpr std::size_t PackedPointerSize = 6;

struct WatchpointNode {
    unsigned char next[PackedPointerSize];
    unsigned char prev[PackedPointerSize];
};

struct Watchpoint {
    WatchpointNode node;
    std::uint8_t type;
};

inline void* LoadPackedPointer(const void* storage) {
    std::uint64_t value = 0;
    std::memcpy(&value, storage, PackedPointerSize);
    if (value & (std::uint64_t{1} << 47)) value |= std::uint64_t{0xFFFF} << 48;
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));
}

inline void StorePackedPointer(void* storage, void* pointer) {
    const std::uint64_t value = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    std::memcpy(storage, &value, PackedPointerSize);
}

inline void* NodeNext(const void* node) {
    return LoadPackedPointer(node);
}

inline void* NodePrev(const void* node) {
    return LoadPackedPointer(static_cast<const unsigned char*>(node) + PackedPointerSize);
}

inline void SetNodeNext(void* node, void* value) {
    StorePackedPointer(node, value);
}

inline void SetNodePrev(void* node, void* value) {
    StorePackedPointer(static_cast<unsigned char*>(node) + PackedPointerSize, value);
}

struct WatchpointSet {
    std::uint32_t refCount;
    std::int8_t state;
    std::int8_t setIsNotEmpty;
    WatchpointNode sentinel;
};

inline void* SentinelAddress(WatchpointSet& set) {
    return &set.sentinel;
}

inline void* SentinelNext(WatchpointSet& set) {
    return NodeNext(&set.sentinel);
}

inline void* SentinelPrev(WatchpointSet& set) {
    return NodePrev(&set.sentinel);
}

inline void SetSentinelNext(WatchpointSet& set, void* value) {
    SetNodeNext(&set.sentinel, value);
}

inline void SetSentinelPrev(WatchpointSet& set, void* value) {
    SetNodePrev(&set.sentinel, value);
}

struct DeferredWatchpointFire {
    WatchpointSet watchpointsToFire;
};

static_assert(sizeof(WatchpointNode) == 12);
static_assert(alignof(WatchpointNode) == 1);
static_assert(offsetof(WatchpointNode, next) == 0);
static_assert(offsetof(WatchpointNode, prev) == 6);
static_assert(offsetof(Watchpoint, node) == 0);
static_assert(offsetof(Watchpoint, type) == 12);
static_assert(sizeof(WatchpointSet) == 20);
static_assert(alignof(WatchpointSet) == 4);
static_assert(offsetof(WatchpointSet, refCount) == 0);
static_assert(offsetof(WatchpointSet, state) == 4);
static_assert(offsetof(WatchpointSet, setIsNotEmpty) == 5);
static_assert(offsetof(WatchpointSet, sentinel) == 6);
static_assert(sizeof(DeferredWatchpointFire) == 20);
static_assert(alignof(DeferredWatchpointFire) == 4);
static_assert(offsetof(DeferredWatchpointFire, watchpointsToFire) == 0);

}
