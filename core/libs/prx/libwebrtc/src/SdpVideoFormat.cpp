/* Derived from api/video_codecs/sdp_video_format.cc, api/video_codecs/video_codec.cc, api/video_codecs/h264_profile_level_id.cc, api/video_codecs/vp9_profile.cc and api/video_codecs/av1_profile.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
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

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint64_t AllocatedBit = 1;

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
    if (!GuestBytes(source).CopyIn(destination, bytes))
        throw std::invalid_argument(message);
}

void CopyOutChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!destination)
        throw std::invalid_argument(message);
    ErrnoGuard guard;
    if (!GuestBytes(destination).CopyOut(source, bytes))
        throw std::invalid_argument(message);
}

void CopyInChunks(void* destination, const void* source, std::size_t totalBytes, const char* message) {
    auto* dst = static_cast<char*>(destination);
    const auto* src = static_cast<const char*>(source);
    std::size_t remaining = totalBytes;
    while (remaining > 0) {
        const std::size_t chunk = std::min<std::size_t>(remaining, 4096);
        CopyInChecked(dst, src, chunk, message);
        dst += chunk;
        src += chunk;
        remaining -= chunk;
    }
}

void CopyOutChunks(void* destination, const void* source, std::size_t totalBytes, const char* message) {
    auto* dst = static_cast<char*>(destination);
    const auto* src = static_cast<const char*>(source);
    std::size_t remaining = totalBytes;
    while (remaining > 0) {
        const std::size_t chunk = std::min<std::size_t>(remaining, 4096);
        CopyOutChecked(dst, src, chunk, message);
        dst += chunk;
        src += chunk;
        remaining -= chunk;
    }
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

WebRtcVideoStringMapNode* EndNode(const WebRtcSdpVideoFormat* format) {
    const auto base = reinterpret_cast<std::uintptr_t>(format);
    return reinterpret_cast<WebRtcVideoStringMapNode*>(
        base + offsetof(WebRtcSdpVideoFormat, parameters) + offsetof(WebRtcVideoStringMap, root));
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

constexpr std::size_t MaxSdpStringBytes = 1024 * 1024;
constexpr std::size_t CopyChunkBytes = 4096;

std::string ReadGuestString(const WebRtcGuestString& string, const char* message) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity)
        throw std::invalid_argument(message);
    if (string.capacity <= WebRtcGuestStringInlineCapacity)
        return std::string(string.buffer, string.size);
    if (!string.pointer || string.size > MaxSdpStringBytes || string.size > std::numeric_limits<std::size_t>::max() - 1)
        throw std::invalid_argument(message);
    if (string.capacity + 1 >= GuestStringStorage::LargeAllocation) {
        void* raw = nullptr;
        CopyInChecked(&raw, static_cast<const void* const*>(static_cast<const void*>(string.pointer)) - 1, sizeof(raw), message);
    }
    std::string result;
    char chunk[CopyChunkBytes];
    std::size_t offset = 0;
    while (offset < string.size) {
        const std::size_t step = std::min(CopyChunkBytes, string.size - offset);
        CopyInChecked(chunk, string.pointer + offset, step, message);
        result.append(chunk, step);
        offset += step;
    }
    char nul = '\0';
    CopyInChecked(&nul, string.pointer + string.size, sizeof(nul), message);
    return result;
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

void WriteHostStringToGuest(const std::string& text, WebRtcGuestString& destination, const char* message) {
    InitStringEmpty(destination);
    if (text.size() <= WebRtcGuestStringInlineCapacity) {
        if (!text.empty())
            std::memcpy(destination.buffer, text.data(), text.size());
        destination.buffer[text.size()] = '\0';
        destination.size = text.size();
        return;
    }
    const std::size_t capacity = text.size() | WebRtcGuestStringInlineCapacity;
    if (capacity > std::numeric_limits<std::size_t>::max() - 1)
        throw std::bad_alloc();
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    CopyOutChunks(data, text.data(), text.size(), message);
    const char nul = '\0';
    CopyOutChecked(data + text.size(), &nul, sizeof(nul), message);
    destination.pointer = data;
    destination.size = text.size();
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

struct CollectedNode {
    WebRtcVideoStringMapNode* address;
    WebRtcVideoStringMapNode copy;
    std::string key;
    std::string value;
};

void ValidateAndCollectMap(const WebRtcVideoStringMap& map, const WebRtcVideoStringMapNode* guestEnd, const char* message,
    std::vector<CollectedNode>& outNodes) {
    outNodes.clear();
    if (map.size == 0) {
        if (map.root != nullptr || map.beginNode != guestEnd)
            throw std::invalid_argument(message);
        return;
    }
    if (!map.root || !map.beginNode || map.beginNode == guestEnd)
        throw std::invalid_argument(message);

    struct Entry {
        WebRtcVideoStringMapNode* node;
        const WebRtcVideoStringMapNode* expectedParent;
    };
    std::vector<Entry> pending;
    pending.push_back({map.root, guestEnd});
    while (!pending.empty()) {
        const Entry current = pending.back();
        pending.pop_back();
        if (current.node == guestEnd || outNodes.size() >= map.size)
            throw std::invalid_argument(message);
        for (const auto& existing : outNodes) {
            if (existing.address == current.node)
                throw std::invalid_argument(message);
        }
        WebRtcVideoStringMapNode nodeCopy{};
        CopyInChecked(&nodeCopy, current.node, sizeof(nodeCopy), message);
        if (nodeCopy.links.parent != current.expectedParent)
            throw std::invalid_argument(message);
        if (nodeCopy.links.left && nodeCopy.links.left == nodeCopy.links.right)
            throw std::invalid_argument(message);
        std::string key = ReadGuestString(nodeCopy.key, message);
        std::string value = ReadGuestString(nodeCopy.value, message);
        outNodes.push_back({current.node, nodeCopy, std::move(key), std::move(value)});
        if (nodeCopy.links.right)
            pending.push_back({nodeCopy.links.right, current.node});
        if (nodeCopy.links.left)
            pending.push_back({nodeCopy.links.left, current.node});
    }
    if (outNodes.size() != map.size)
        throw std::invalid_argument(message);

    WebRtcVideoStringMapNode* leftmost = map.root;
    WebRtcVideoStringMapNode leftmostCopy{};
    CopyInChecked(&leftmostCopy, leftmost, sizeof(leftmostCopy), message);
    std::uint64_t steps = 0;
    while (leftmostCopy.links.left) {
        if (++steps >= map.size)
            throw std::invalid_argument(message);
        leftmost = leftmostCopy.links.left;
        CopyInChecked(&leftmostCopy, leftmost, sizeof(leftmostCopy), message);
    }
    if (leftmost != map.beginNode)
        throw std::invalid_argument(message);
}

std::map<std::string, std::string> ReadGuestMap(
    const WebRtcVideoStringMap& map, const WebRtcVideoStringMapNode* guestEnd, const char* message) {
    std::vector<CollectedNode> collected;
    ValidateAndCollectMap(map, guestEnd, message, collected);
    std::map<std::string, std::string> result;
    for (auto& entry : collected)
        result.emplace(std::move(entry.key), std::move(entry.value));
    return result;
}

struct AllocatedNodeRecord {
    WebRtcVideoStringMapNode* address;
    WebRtcGuestString key;
    WebRtcGuestString value;
};

struct NodeGuard {
    std::vector<AllocatedNodeRecord> nodes;
    bool active = true;

    ~NodeGuard() {
        if (!active)
            return;
        for (auto& record : nodes) {
            ReleaseString(record.value);
            ReleaseString(record.key);
            _ZdlPv_nid_postfix(record.address);
        }
    }
};

void DestroyMapNodes(std::vector<CollectedNode>& nodes, WebRtcVideoStringMap& map, WebRtcVideoStringMapNode* guestEnd) {
    for (auto& entry : nodes) {
        ReleaseString(entry.copy.value);
        ReleaseString(entry.copy.key);
        _ZdlPv_nid_postfix(entry.address);
    }
    map.beginNode = guestEnd;
    map.root = nullptr;
    map.size = 0;
}

void CloneMap(const WebRtcVideoStringMap& source, const WebRtcVideoStringMapNode* sourceEnd,
    WebRtcVideoStringMap& destination, WebRtcVideoStringMapNode* destinationEnd, NodeGuard& guard, const char* message) {
    std::vector<CollectedNode> validated;
    ValidateAndCollectMap(source, sourceEnd, message, validated);
    destination.beginNode = destinationEnd;
    destination.root = nullptr;
    destination.size = 0;
    if (source.size == 0)
        return;

    std::map<const WebRtcVideoStringMapNode*, const CollectedNode*> byAddress;
    for (const auto& entry : validated)
        byAddress.emplace(entry.address, &entry);

    struct CloneTask {
        const WebRtcVideoStringMapNode* sourceNode;
        WebRtcVideoStringMapNode* parentAddress;
        bool isLeft;
        bool isRoot;
    };

    struct CreatedNode {
        WebRtcVideoStringMapNode* address;
        WebRtcVideoStringMapNode hostNode;
    };

    std::vector<CreatedNode> created;
    created.reserve(validated.size());
    std::vector<CloneTask> tasks;
    tasks.push_back({source.root, destinationEnd, false, true});

    std::map<const WebRtcVideoStringMapNode*, std::size_t> createdIndexBySource;

    while (!tasks.empty()) {
        const CloneTask task = tasks.back();
        tasks.pop_back();
        const CollectedNode* sourceInfo = byAddress.at(task.sourceNode);
        void* storage = _Znwm_nid_postfix(sizeof(WebRtcVideoStringMapNode));
        if (!storage)
            throw std::bad_alloc();
        auto* nodeAddr = static_cast<WebRtcVideoStringMapNode*>(storage);

        guard.nodes.push_back({nodeAddr, {}, {}});
        auto& record = guard.nodes.back();
        InitStringEmpty(record.key);
        InitStringEmpty(record.value);

        WriteHostStringToGuest(sourceInfo->key, record.key, message);
        WriteHostStringToGuest(sourceInfo->value, record.value, message);

        WebRtcVideoStringMapNode hostNode{};
        hostNode.links.left = nullptr;
        hostNode.links.right = nullptr;
        hostNode.links.parent = task.parentAddress;
        hostNode.links.isBlack = sourceInfo->copy.links.isBlack;
        hostNode.key = record.key;
        hostNode.value = record.value;

        const std::size_t idx = created.size();
        created.push_back({nodeAddr, hostNode});
        createdIndexBySource.emplace(task.sourceNode, idx);

        if (task.isRoot) {
            destination.root = nodeAddr;
        } else {
            const std::size_t parentIdx = createdIndexBySource.at(sourceInfo->copy.links.parent);
            if (task.isLeft)
                created[parentIdx].hostNode.links.left = nodeAddr;
            else
                created[parentIdx].hostNode.links.right = nodeAddr;
        }

        if (task.sourceNode == source.beginNode)
            destination.beginNode = nodeAddr;
        if (sourceInfo->copy.links.right)
            tasks.push_back({sourceInfo->copy.links.right, nodeAddr, false, false});
        if (sourceInfo->copy.links.left)
            tasks.push_back({sourceInfo->copy.links.left, nodeAddr, true, false});
    }

    for (const auto& item : created)
        CopyOutChecked(item.address, &item.hostNode, sizeof(item.hostNode), message);

    destination.size = source.size;
}

void ValidateInlined(const WebRtcInlinedVector<WebRtcScalabilityModeCount>& vector, const char* message) {
    const std::uint64_t size = vector.metadata >> 1;
    if (vector.metadata & AllocatedBit) {
        if (!vector.allocated.data || vector.allocated.capacity <= WebRtcScalabilityModeCount || size > vector.allocated.capacity)
            throw std::invalid_argument(message);
        ProbeReadableChunks(vector.allocated.data, size, message);
    } else if (size > WebRtcScalabilityModeCount) {
        throw std::invalid_argument(message);
    }
}

void InitInlinedEmpty(WebRtcInlinedVector<WebRtcScalabilityModeCount>& vector) {
    vector.metadata = 0;
    std::memset(vector.inlined, 0, sizeof(vector.inlined));
}

struct InlinedGuard {
    void* ptr = nullptr;
    ~InlinedGuard() {
        if (ptr)
            _ZdlPv_nid_postfix(ptr);
    }
};

void CloneInlined(const WebRtcInlinedVector<WebRtcScalabilityModeCount>& source,
    WebRtcInlinedVector<WebRtcScalabilityModeCount>& destination, InlinedGuard& guard, const char* message) {
    ValidateInlined(source, message);
    InitInlinedEmpty(destination);
    const std::size_t size = static_cast<std::size_t>(source.metadata >> 1);
    if (size == 0)
        return;

    if (source.metadata & AllocatedBit) {
        std::vector<std::uint8_t> bytes(size);
        CopyInChunks(bytes.data(), source.allocated.data, size, message);
        if (size <= WebRtcScalabilityModeCount) {
            std::memcpy(destination.inlined, bytes.data(), size);
            destination.metadata = static_cast<std::uint64_t>(size) << 1;
        } else {
            void* storage = _Znwm_nid_postfix(size);
            if (!storage)
                throw std::bad_alloc();
            guard.ptr = storage;
            CopyOutChunks(storage, bytes.data(), size, message);
            destination.allocated.data = storage;
            destination.allocated.capacity = size;
            destination.metadata = (static_cast<std::uint64_t>(size) << 1) | AllocatedBit;
        }
        return;
    }

    std::memcpy(destination.inlined, source.inlined, size);
    destination.metadata = static_cast<std::uint64_t>(size) << 1;
}

void MoveInlined(WebRtcInlinedVector<WebRtcScalabilityModeCount>& destination,
    WebRtcInlinedVector<WebRtcScalabilityModeCount>& source) {
    InitInlinedEmpty(destination);
    if (source.metadata & AllocatedBit) {
        destination.metadata = source.metadata;
        destination.allocated = source.allocated;
    } else {
        const std::size_t size = static_cast<std::size_t>(source.metadata >> 1);
        destination.metadata = source.metadata;
        if (size > 0)
            std::memcpy(destination.inlined, source.inlined, size);
    }
    InitInlinedEmpty(source);
}

char ToAsciiLower(char ch) {
    if (ch >= 'A' && ch <= 'Z')
        return static_cast<char>(ch - 'A' + 'a');
    return ch;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (ToAsciiLower(a[i]) != ToAsciiLower(b[i]))
            return false;
    }
    return true;
}

std::optional<int> ParseStrictInt(std::string_view str) {
    if (str.empty())
        return std::nullopt;
    int value = 0;
    const auto* begin = str.data();
    const auto* end = begin + str.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
        return std::nullopt;
    return value;
}

enum class VideoCodecKind {
    Generic = 0,
    VP8,
    VP9,
    AV1,
    H264,
};

VideoCodecKind PayloadStringToCodecKind(const std::string& name) {
    if (EqualsIgnoreCase(name, "VP8"))
        return VideoCodecKind::VP8;
    if (EqualsIgnoreCase(name, "VP9"))
        return VideoCodecKind::VP9;
    if (EqualsIgnoreCase(name, "AV1") || EqualsIgnoreCase(name, "AV1X"))
        return VideoCodecKind::AV1;
    if (EqualsIgnoreCase(name, "H264"))
        return VideoCodecKind::H264;
    return VideoCodecKind::Generic;
}

std::string GetFmtpParameterOrDefault(
    const std::map<std::string, std::string>& params, const std::string& key, const std::string& defaultValue) {
    const auto it = params.find(key);
    if (it != params.end())
        return it->second;
    return defaultValue;
}

enum class H264Profile {
    ConstrainedBaseline,
    Baseline,
    Main,
    ConstrainedHigh,
    High,
    PredictiveHigh444,
};

enum class H264Level {
    Level1_b = 0,
    Level1 = 10,
    Level1_1 = 11,
    Level1_2 = 12,
    Level1_3 = 13,
    Level2 = 20,
    Level2_1 = 21,
    Level2_2 = 22,
    Level3 = 30,
    Level3_1 = 31,
    Level3_2 = 32,
    Level4 = 40,
    Level4_1 = 41,
    Level4_2 = 42,
    Level5 = 50,
    Level5_1 = 51,
    Level5_2 = 52,
};

struct H264ProfileLevelId {
    H264Profile profile;
    H264Level level;
};

constexpr std::uint8_t ByteMaskString(char c, const char (&str)[9]) {
    return static_cast<std::uint8_t>(((str[0] == c) << 7) | ((str[1] == c) << 6) | ((str[2] == c) << 5) |
        ((str[3] == c) << 4) | ((str[4] == c) << 3) | ((str[5] == c) << 2) | ((str[6] == c) << 1) | ((str[7] == c) << 0));
}

struct BitPattern {
    std::uint8_t mask;
    std::uint8_t maskedValue;

    explicit constexpr BitPattern(const char (&str)[9])
        : mask(static_cast<std::uint8_t>(~ByteMaskString('x', str)))
        , maskedValue(ByteMaskString('1', str)) {}

    bool IsMatch(std::uint8_t value) const { return maskedValue == (value & mask); }
};

struct ProfilePattern {
    std::uint8_t profileIdc;
    BitPattern profileIop;
    H264Profile profile;
};

constexpr ProfilePattern H264ProfilePatterns[] = {
    {0x42, BitPattern("x1xx0000"), H264Profile::ConstrainedBaseline},
    {0x4D, BitPattern("1xxx0000"), H264Profile::ConstrainedBaseline},
    {0x58, BitPattern("11xx0000"), H264Profile::ConstrainedBaseline},
    {0x42, BitPattern("x0xx0000"), H264Profile::Baseline},
    {0x58, BitPattern("10xx0000"), H264Profile::Baseline},
    {0x4D, BitPattern("0x0x0000"), H264Profile::Main},
    {0x64, BitPattern("00000000"), H264Profile::High},
    {0x64, BitPattern("00001100"), H264Profile::ConstrainedHigh},
    {0xF4, BitPattern("00000000"), H264Profile::PredictiveHigh444},
};

std::optional<H264ProfileLevelId> ParseH264ProfileLevelId(const char* str) {
    constexpr std::uint8_t constraintSet3Flag = 0x10;
    if (!str || std::strlen(str) != 6u)
        return std::nullopt;
    std::uint32_t numeric = 0;
    const auto [ptr, ec] = std::from_chars(str, str + 6, numeric, 16);
    if (ec != std::errc{} || ptr != str + 6 || numeric == 0)
        return std::nullopt;

    const std::uint8_t levelIdc = static_cast<std::uint8_t>(numeric & 0xFF);
    const std::uint8_t profileIop = static_cast<std::uint8_t>((numeric >> 8) & 0xFF);
    const std::uint8_t profileIdc = static_cast<std::uint8_t>((numeric >> 16) & 0xFF);

    const H264Level levelCasted = static_cast<H264Level>(levelIdc);
    H264Level level{};
    switch (levelCasted) {
        case H264Level::Level1_1:
            level = (profileIop & constraintSet3Flag) != 0 ? H264Level::Level1_b : H264Level::Level1_1;
            break;
        case H264Level::Level1:
        case H264Level::Level1_2:
        case H264Level::Level1_3:
        case H264Level::Level2:
        case H264Level::Level2_1:
        case H264Level::Level2_2:
        case H264Level::Level3:
        case H264Level::Level3_1:
        case H264Level::Level3_2:
        case H264Level::Level4:
        case H264Level::Level4_1:
        case H264Level::Level4_2:
        case H264Level::Level5:
        case H264Level::Level5_1:
        case H264Level::Level5_2:
            level = levelCasted;
            break;
        default:
            return std::nullopt;
    }

    for (const ProfilePattern& pattern : H264ProfilePatterns) {
        if (profileIdc == pattern.profileIdc && pattern.profileIop.IsMatch(profileIop))
            return H264ProfileLevelId{pattern.profile, level};
    }
    return std::nullopt;
}

std::optional<H264ProfileLevelId> ParseSdpForH264ProfileLevelId(const std::map<std::string, std::string>& params) {
    static const H264ProfileLevelId defaultProfileLevelId{H264Profile::ConstrainedBaseline, H264Level::Level3_1};
    const auto it = params.find("profile-level-id");
    return it == params.end() ? defaultProfileLevelId : ParseH264ProfileLevelId(it->second.c_str());
}

bool H264IsSameProfile(
    const std::map<std::string, std::string>& left, const std::map<std::string, std::string>& right) {
    const auto p1 = ParseSdpForH264ProfileLevelId(left);
    const auto p2 = ParseSdpForH264ProfileLevelId(right);
    return p1 && p2 && p1->profile == p2->profile;
}

bool H264IsSamePacketizationMode(
    const std::map<std::string, std::string>& left, const std::map<std::string, std::string>& right) {
    return GetFmtpParameterOrDefault(left, "packetization-mode", "0") ==
        GetFmtpParameterOrDefault(right, "packetization-mode", "0");
}

std::optional<int> ParseSdpForVP9Profile(const std::map<std::string, std::string>& params) {
    const auto it = params.find("profile-id");
    if (it == params.end())
        return 0;
    const auto parsed = ParseStrictInt(it->second);
    if (!parsed || *parsed < 0 || *parsed > 3)
        return std::nullopt;
    return parsed;
}

bool VP9IsSameProfile(const std::map<std::string, std::string>& left, const std::map<std::string, std::string>& right) {
    const auto p1 = ParseSdpForVP9Profile(left);
    const auto p2 = ParseSdpForVP9Profile(right);
    return p1 && p2 && *p1 == *p2;
}

std::optional<int> ParseSdpForAV1Profile(const std::map<std::string, std::string>& params) {
    const auto it = params.find("profile");
    if (it == params.end())
        return 0;
    const auto parsed = ParseStrictInt(it->second);
    if (!parsed || *parsed < 0 || *parsed > 2)
        return std::nullopt;
    return parsed;
}

bool AV1IsSameProfile(const std::map<std::string, std::string>& left, const std::map<std::string, std::string>& right) {
    const auto p1 = ParseSdpForAV1Profile(left);
    const auto p2 = ParseSdpForAV1Profile(right);
    return p1 && p2 && *p1 == *p2;
}

bool AV1IsSameTier(const std::map<std::string, std::string>& left, const std::map<std::string, std::string>& right) {
    return GetFmtpParameterOrDefault(left, "tier", "0") == GetFmtpParameterOrDefault(right, "tier", "0");
}

bool AV1IsSameLevelIdx(const std::map<std::string, std::string>& left, const std::map<std::string, std::string>& right) {
    return GetFmtpParameterOrDefault(left, "level-idx", "5") == GetFmtpParameterOrDefault(right, "level-idx", "5");
}

bool IsSameCodecSpecific(const std::string& name, const std::map<std::string, std::string>& params1,
    const std::map<std::string, std::string>& params2) {
    switch (PayloadStringToCodecKind(name)) {
        case VideoCodecKind::H264:
            return H264IsSameProfile(params1, params2) && H264IsSamePacketizationMode(params1, params2);
        case VideoCodecKind::VP9:
            return VP9IsSameProfile(params1, params2);
        case VideoCodecKind::AV1:
            return AV1IsSameProfile(params1, params2) && AV1IsSameTier(params1, params2) && AV1IsSameLevelIdx(params1, params2);
        case VideoCodecKind::VP8:
        case VideoCodecKind::Generic:
        default:
            return true;
    }
}

}

extern "C" {

void APS5_VABI _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(WebRtcSdpVideoFormat* format, const WebRtcSdpVideoFormat* other) {
    constexpr const char* message = "SdpVideoFormat copy constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    WebRtcSdpVideoFormat snapshot{};
    CopyInChecked(&snapshot, other, sizeof(snapshot), message);

    const std::string hostName = ReadGuestString(snapshot.name, message);

    StringGuard nameGuard;
    WriteHostStringToGuest(hostName, nameGuard.value, message);
    nameGuard.active = true;

    NodeGuard nodeGuard;
    WebRtcVideoStringMap clonedMap{};
    CloneMap(snapshot.parameters, EndNode(other), clonedMap, EndNode(format), nodeGuard, message);

    InlinedGuard inlinedGuard;
    WebRtcInlinedVector<WebRtcScalabilityModeCount> clonedModes{};
    CloneInlined(snapshot.scalabilityModes, clonedModes, inlinedGuard, message);

    WebRtcSdpVideoFormat output{};
    output.name = nameGuard.value;
    output.parameters = clonedMap;
    output.scalabilityModes = clonedModes;
    CopyOutChecked(format, &output, sizeof(output), message);

    nameGuard.active = false;
    nodeGuard.active = false;
    inlinedGuard.ptr = nullptr;
}

void APS5_VABI _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(WebRtcSdpVideoFormat* format, WebRtcSdpVideoFormat* other) {
    constexpr const char* message = "SdpVideoFormat move constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    RequireWritable(other, sizeof(*other), message);
    if (format == other)
        throw std::invalid_argument(message);

    WebRtcSdpVideoFormat snapshot{};
    CopyInChecked(&snapshot, other, sizeof(snapshot), message);
    ReadGuestString(snapshot.name, message);
    std::vector<CollectedNode> nodes;
    ValidateAndCollectMap(snapshot.parameters, EndNode(other), message, nodes);
    ValidateInlined(snapshot.scalabilityModes, message);

    WebRtcSdpVideoFormat dst{};
    WebRtcSdpVideoFormat movedFrom = snapshot;
    MoveString(dst.name, movedFrom.name);
    MoveInlined(dst.scalabilityModes, movedFrom.scalabilityModes);

    if (snapshot.parameters.size == 0) {
        dst.parameters.beginNode = EndNode(format);
        dst.parameters.root = nullptr;
        dst.parameters.size = 0;
        movedFrom.parameters.beginNode = EndNode(other);
        movedFrom.parameters.root = nullptr;
        movedFrom.parameters.size = 0;
    } else {
        WebRtcVideoStringMapNode rootNode{};
        CopyInChecked(&rootNode, snapshot.parameters.root, sizeof(rootNode), message);
        rootNode.links.parent = EndNode(format);
        CopyOutChecked(snapshot.parameters.root, &rootNode, sizeof(rootNode), message);
        dst.parameters = snapshot.parameters;
        movedFrom.parameters.beginNode = EndNode(other);
        movedFrom.parameters.root = nullptr;
        movedFrom.parameters.size = 0;
    }

    CopyOutChecked(format, &dst, sizeof(dst), message);
    CopyOutChecked(other, &movedFrom, sizeof(movedFrom), message);
}

void APS5_VABI _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(WebRtcSdpVideoFormat* format) {
    constexpr const char* message = "SdpVideoFormat destructor requires a valid object";
    RequireWritable(format, sizeof(*format), message);
    WebRtcSdpVideoFormat current{};
    CopyInChecked(&current, format, sizeof(current), message);

    ReadGuestString(current.name, message);
    std::vector<CollectedNode> nodes;
    ValidateAndCollectMap(current.parameters, EndNode(format), message, nodes);
    ValidateInlined(current.scalabilityModes, message);

    if (current.scalabilityModes.metadata & AllocatedBit)
        _ZdlPv_nid_postfix(current.scalabilityModes.allocated.data);
    InitInlinedEmpty(current.scalabilityModes);
    DestroyMapNodes(nodes, current.parameters, EndNode(format));
    ReleaseString(current.name);
    CopyOutChecked(format, &current, sizeof(current), message);
}

bool APS5_VABI _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(
    const WebRtcSdpVideoFormat* format, WebRtcSdpVideoFormatView formats) {
    constexpr const char* message = "SdpVideoFormat::IsCodecInList requires valid objects";
    WebRtcSdpVideoFormat self{};
    CopyInChecked(&self, format, sizeof(self), message);
    const std::string selfName = ReadGuestString(self.name, message);
    const auto selfParams = ReadGuestMap(self.parameters, EndNode(format), message);

    if (formats.size == 0)
        return false;
    if (!formats.data || formats.size > std::numeric_limits<std::size_t>::max() / sizeof(WebRtcSdpVideoFormat))
        throw std::invalid_argument(message);
    const auto baseAddress = reinterpret_cast<std::uintptr_t>(formats.data);
    const std::size_t totalBytes = formats.size * sizeof(WebRtcSdpVideoFormat);
    if (totalBytes > std::numeric_limits<std::uintptr_t>::max() - baseAddress)
        throw std::invalid_argument(message);

    for (std::size_t i = 0; i < formats.size; ++i) {
        const auto* candidatePtr = reinterpret_cast<const WebRtcSdpVideoFormat*>(
            baseAddress + i * sizeof(WebRtcSdpVideoFormat));
        WebRtcSdpVideoFormat candidate{};
        CopyInChecked(&candidate, candidatePtr, sizeof(candidate), message);
        const std::string candidateName = ReadGuestString(candidate.name, message);
        if (!EqualsIgnoreCase(selfName, candidateName))
            continue;
        const auto candidateParams = ReadGuestMap(candidate.parameters, EndNode(candidatePtr), message);
        if (IsSameCodecSpecific(selfName, selfParams, candidateParams))
            return true;
    }
    return false;
}

}
