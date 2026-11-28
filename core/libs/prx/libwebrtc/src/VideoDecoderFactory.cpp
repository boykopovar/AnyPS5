/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
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
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
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

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t GetSupportedFormatsSlot = 2;
constexpr std::size_t ScalabilityModeCount = 34;
constexpr std::size_t ReadChunk = 4096;

struct ScalabilityModeAllocation {
    std::uint8_t* data;
    std::uint64_t capacity;
};

struct ScalabilityModes {
    std::uint64_t sizeAndIsAllocated;
    union {
        std::uint8_t inlined[ScalabilityModeCount];
        ScalabilityModeAllocation allocated;
    };
};

struct VideoFormat {
    WebRtcGuestString name;
    WebRtcStringMap parameters;
    ScalabilityModes scalabilityModes;
};

static_assert(sizeof(ScalabilityModes) == 48);
static_assert(sizeof(VideoFormat) == 112);
static_assert(offsetof(VideoFormat, parameters) == 40);
static_assert(offsetof(VideoFormat, scalabilityModes) == 64);

struct VideoFormatVector {
    VideoFormat* begin;
    VideoFormat* end;
    VideoFormat* capacityEnd;
};

using GetSupportedFormatsFunction = void(APS5_VABI*)(VideoFormatVector*, const WebRtcVideoDecoderFactory*);

using Parameters = std::vector<std::pair<std::string, std::string>>;

struct Format {
    std::string name;
    Parameters parameters;
};

struct OwnedStorage {
    std::vector<WebRtcGuestString> strings;
    std::vector<void*> blocks;
};

enum class CodecType { Generic, Vp8, Vp9, Av1, H264, H265 };

enum class H264Profile { ConstrainedBaseline, Baseline, Main, ConstrainedHigh, High, PredictiveHigh444 };

void CopyInChecked(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!source)
        throw std::invalid_argument(message);
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

std::string ReadString(const WebRtcGuestString& string, OwnedStorage* owned, const char* message) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity)
        throw std::invalid_argument(message);
    if (string.capacity == WebRtcGuestStringInlineCapacity)
        return std::string(string.buffer, string.size);
    if (!string.pointer || string.capacity == std::numeric_limits<std::uint64_t>::max())
        throw std::invalid_argument(message);
    if (owned && string.capacity + 1 >= GuestStringStorage::LargeAllocation) {
        void* raw = nullptr;
        CopyInChecked(&raw, static_cast<const void* const*>(static_cast<const void*>(string.pointer)) - 1, sizeof(raw), message);
        if (!raw)
            throw std::invalid_argument(message);
    }
    std::string result;
    std::array<char, ReadChunk> chunk{};
    std::size_t offset = 0;
    while (offset < string.size) {
        const std::size_t bytes = std::min<std::size_t>(string.size - offset, chunk.size());
        CopyInChecked(chunk.data(), string.pointer + offset, bytes, message);
        result.append(chunk.data(), bytes);
        offset += bytes;
    }
    if (owned)
        owned->strings.push_back(string);
    return result;
}

Parameters ReadParameters(const WebRtcStringMap& map, const WebRtcStringMapNode* end, OwnedStorage* owned, const char* message) {
    Parameters result;
    if (map.size == 0) {
        if (map.root || map.beginNode != end)
            throw std::invalid_argument(message);
        return result;
    }
    if (!map.root || !map.beginNode || map.beginNode == end)
        throw std::invalid_argument(message);
    struct Frame {
        WebRtcStringMapNode* address;
        WebRtcStringMapNode copy;
    };
    std::vector<Frame> stack;
    std::vector<WebRtcStringMapNode*> nodes;
    std::uint64_t visited = 0;
    WebRtcStringMapNode* current = map.root;
    const WebRtcStringMapNode* parent = end;
    while (current || !stack.empty()) {
        while (current) {
            if (++visited > map.size)
                throw std::invalid_argument(message);
            Frame frame{current, {}};
            CopyInChecked(&frame.copy, current, sizeof(frame.copy), message);
            if (frame.copy.links.parent != parent)
                throw std::invalid_argument(message);
            parent = current;
            current = frame.copy.links.left;
            stack.push_back(frame);
        }
        const Frame frame = stack.back();
        stack.pop_back();
        if (result.empty() && frame.address != map.beginNode)
            throw std::invalid_argument(message);
        std::string key = ReadString(frame.copy.key, owned, message);
        std::string value = ReadString(frame.copy.value, owned, message);
        if (!result.empty() && !(result.back().first < key))
            throw std::invalid_argument(message);
        result.emplace_back(std::move(key), std::move(value));
        nodes.push_back(frame.address);
        parent = frame.address;
        current = frame.copy.links.right;
    }
    if (result.size() != map.size)
        throw std::invalid_argument(message);
    if (owned)
        owned->blocks.insert(owned->blocks.end(), nodes.begin(), nodes.end());
    return result;
}

const WebRtcStringMapNode* EndNode(const VideoFormat* guest) {
    const auto* address = reinterpret_cast<const unsigned char*>(guest) + offsetof(VideoFormat, parameters) + offsetof(WebRtcStringMap, root);
    return reinterpret_cast<const WebRtcStringMapNode*>(address);
}

Format ReadFormat(const VideoFormat* guest, OwnedStorage* owned, const char* message) {
    VideoFormat snapshot;
    CopyInChecked(&snapshot, guest, sizeof(snapshot), message);
    Format format;
    format.name = ReadString(snapshot.name, owned, message);
    format.parameters = ReadParameters(snapshot.parameters, EndNode(guest), owned, message);
    if (owned) {
        const std::uint64_t count = snapshot.scalabilityModes.sizeAndIsAllocated >> 1;
        if (snapshot.scalabilityModes.sizeAndIsAllocated & 1) {
            if (!snapshot.scalabilityModes.allocated.data || count > snapshot.scalabilityModes.allocated.capacity)
                throw std::invalid_argument(message);
            owned->blocks.push_back(snapshot.scalabilityModes.allocated.data);
        } else if (count > ScalabilityModeCount) {
            throw std::invalid_argument(message);
        }
    }
    return format;
}

struct OwnedRelease {
    OwnedStorage storage;
    bool active = false;

    ~OwnedRelease() {
        if (!active)
            return;
        for (const WebRtcGuestString& string : storage.strings)
            GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
        for (void* block : storage.blocks)
            _ZdlPv_nid_postfix(block);
    }
};

char AsciiLower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool EqualsIgnoreCase(const std::string& left, const std::string& right) {
    if (left.size() != right.size())
        return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (AsciiLower(left[i]) != AsciiLower(right[i]))
            return false;
    }
    return true;
}

CodecType PayloadStringToCodecType(const std::string& name) {
    if (EqualsIgnoreCase(name, "VP8"))
        return CodecType::Vp8;
    if (EqualsIgnoreCase(name, "VP9"))
        return CodecType::Vp9;
    if (EqualsIgnoreCase(name, "AV1") || EqualsIgnoreCase(name, "AV1X"))
        return CodecType::Av1;
    if (EqualsIgnoreCase(name, "H264"))
        return CodecType::H264;
    if (EqualsIgnoreCase(name, "H265"))
        return CodecType::H265;
    return CodecType::Generic;
}

const std::string* FindParameter(const Parameters& parameters, const std::string& key) {
    for (const auto& [name, value] : parameters) {
        if (name == key)
            return &value;
    }
    return nullptr;
}

std::string GetParameterOrDefault(const Parameters& parameters, const std::string& key, const std::string& fallback) {
    const std::string* value = FindParameter(parameters, key);
    return value ? *value : fallback;
}

std::optional<int> StringToInt(const std::string& text) {
    if (text.empty())
        return std::nullopt;
    std::size_t index = 0;
    const bool negative = text[0] == '-';
    if (negative)
        index = 1;
    if (index == text.size())
        return std::nullopt;
    const std::int64_t limit = negative ? -static_cast<std::int64_t>(std::numeric_limits<int>::min()) : std::numeric_limits<int>::max();
    std::int64_t magnitude = 0;
    bool outOfRange = false;
    for (; index < text.size(); ++index) {
        const char digit = text[index];
        if (digit < '0' || digit > '9')
            return std::nullopt;
        if (!outOfRange) {
            magnitude = magnitude * 10 + (digit - '0');
            outOfRange = magnitude > limit;
        }
    }
    if (outOfRange)
        return std::nullopt;
    return static_cast<int>(negative ? -magnitude : magnitude);
}

std::optional<int> ParseProfile(const Parameters& parameters, const std::string& key, int highestProfile) {
    const std::string* value = FindParameter(parameters, key);
    if (!value)
        return 0;
    const std::optional<int> number = StringToInt(*value);
    if (!number || *number < 0 || *number > highestProfile)
        return std::nullopt;
    return number;
}

bool IsSameProfile(const Parameters& left, const Parameters& right, const std::string& key, int highestProfile) {
    const std::optional<int> profile = ParseProfile(left, key, highestProfile);
    const std::optional<int> otherProfile = ParseProfile(right, key, highestProfile);
    return profile && otherProfile && *profile == *otherProfile;
}

bool IsSpace(char value) {
    return value == ' ' || (value >= '\t' && value <= '\r');
}

int HexDigit(char value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

std::int64_t ParseHexPrefix(const std::string& text) {
    std::size_t index = 0;
    while (index < text.size() && IsSpace(text[index]))
        ++index;
    bool negative = false;
    if (index < text.size() && (text[index] == '+' || text[index] == '-')) {
        negative = text[index] == '-';
        ++index;
    }
    if (index + 2 < text.size() && text[index] == '0' && (text[index + 1] == 'x' || text[index + 1] == 'X') && HexDigit(text[index + 2]) >= 0)
        index += 2;
    std::int64_t value = 0;
    while (index < text.size() && HexDigit(text[index]) >= 0) {
        value = value * 16 + HexDigit(text[index]);
        ++index;
    }
    return negative ? -value : value;
}

std::uint8_t ByteMask(char wanted, const char (&pattern)[9]) {
    std::uint8_t mask = 0;
    for (int i = 0; i < 8; ++i) {
        if (pattern[i] == wanted)
            mask = static_cast<std::uint8_t>(mask | (1u << (7 - i)));
    }
    return mask;
}

bool MatchesPattern(std::uint8_t value, const char (&pattern)[9]) {
    const auto mask = static_cast<std::uint8_t>(~ByteMask('x', pattern));
    return ByteMask('1', pattern) == (value & mask);
}

std::optional<H264Profile> ParseH264Profile(const std::string& value) {
    const std::string text = value.substr(0, std::strlen(value.c_str()));
    if (text.size() != 6)
        return std::nullopt;
    const auto numeric = static_cast<std::uint32_t>(ParseHexPrefix(text));
    if (numeric == 0)
        return std::nullopt;
    const auto levelIdc = static_cast<std::uint8_t>(numeric & 0xFF);
    const auto profileIop = static_cast<std::uint8_t>((numeric >> 8) & 0xFF);
    const auto profileIdc = static_cast<std::uint8_t>((numeric >> 16) & 0xFF);
    switch (levelIdc) {
    case 10:
    case 11:
    case 12:
    case 13:
    case 20:
    case 21:
    case 22:
    case 30:
    case 31:
    case 32:
    case 40:
    case 41:
    case 42:
    case 50:
    case 51:
    case 52:
        break;
    default:
        return std::nullopt;
    }
    struct Pattern {
        std::uint8_t profileIdc;
        const char (&profileIop)[9];
        H264Profile profile;
    };
    static const Pattern patterns[] = {
        {0x42, "x1xx0000", H264Profile::ConstrainedBaseline},
        {0x4D, "1xxx0000", H264Profile::ConstrainedBaseline},
        {0x58, "11xx0000", H264Profile::ConstrainedBaseline},
        {0x42, "x0xx0000", H264Profile::Baseline},
        {0x58, "10xx0000", H264Profile::Baseline},
        {0x4D, "0x0x0000", H264Profile::Main},
        {0x64, "00000000", H264Profile::High},
        {0x64, "00001100", H264Profile::ConstrainedHigh},
        {0xF4, "00000000", H264Profile::PredictiveHigh444},
    };
    for (const Pattern& pattern : patterns) {
        if (profileIdc == pattern.profileIdc && MatchesPattern(profileIop, pattern.profileIop))
            return pattern.profile;
    }
    return std::nullopt;
}

std::optional<H264Profile> ParseSdpForH264Profile(const Parameters& parameters) {
    const std::string* value = FindParameter(parameters, "profile-level-id");
    if (!value)
        return H264Profile::ConstrainedBaseline;
    return ParseH264Profile(*value);
}

bool IsSameCodecSpecific(const Format& left, const Format& right) {
    switch (PayloadStringToCodecType(left.name)) {
    case CodecType::H264: {
        const std::optional<H264Profile> profile = ParseSdpForH264Profile(left.parameters);
        const std::optional<H264Profile> otherProfile = ParseSdpForH264Profile(right.parameters);
        return profile && otherProfile && *profile == *otherProfile &&
            GetParameterOrDefault(left.parameters, "packetization-mode", "0") == GetParameterOrDefault(right.parameters, "packetization-mode", "0");
    }
    case CodecType::Vp9:
        return IsSameProfile(left.parameters, right.parameters, "profile-id", 3);
    case CodecType::Av1:
        return IsSameProfile(left.parameters, right.parameters, "profile", 2) &&
            GetParameterOrDefault(left.parameters, "tier", "0") == GetParameterOrDefault(right.parameters, "tier", "0") &&
            GetParameterOrDefault(left.parameters, "level-idx", "5") == GetParameterOrDefault(right.parameters, "level-idx", "5");
    case CodecType::H265:
        NotImplemented_nid_no_patch("webrtc::SdpVideoFormat::IsSameCodec(H265): depends on RTC_ENABLE_H265 and differs between M127 and M134");
    default:
        return true;
    }
}

bool IsSameCodec(const Format& left, const Format& right) {
    return EqualsIgnoreCase(left.name, right.name) && IsSameCodecSpecific(left, right);
}

std::size_t CountFormats(const VideoFormatVector& vector, const char* message) {
    const auto first = reinterpret_cast<std::uintptr_t>(vector.begin);
    const auto last = reinterpret_cast<std::uintptr_t>(vector.end);
    const auto limit = reinterpret_cast<std::uintptr_t>(vector.capacityEnd);
    if (last < first || limit < last || (!first && limit) || first % alignof(std::uint64_t) ||
        (last - first) % sizeof(VideoFormat) || (limit - first) % sizeof(VideoFormat))
        throw std::invalid_argument(message);
    return (last - first) / sizeof(VideoFormat);
}

}

extern "C" {

bool APS5_VABI _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(const WebRtcSdpVideoFormat* format, const WebRtcSdpVideoFormat* formats, std::size_t count) {
    constexpr const char* message = "SdpVideoFormat::IsCodecInList requires valid formats";
    if (count > 0 && (!formats || count > std::numeric_limits<std::uintptr_t>::max() / sizeof(VideoFormat) ||
                      reinterpret_cast<std::uintptr_t>(formats) > std::numeric_limits<std::uintptr_t>::max() - count * sizeof(VideoFormat)))
        throw std::invalid_argument(message);
    const Format self = ReadFormat(reinterpret_cast<const VideoFormat*>(format), nullptr, message);
    const auto* list = reinterpret_cast<const VideoFormat*>(formats);
    for (std::size_t i = 0; i < count; ++i) {
        if (IsSameCodec(self, ReadFormat(list + i, nullptr, message)))
            return true;
    }
    return false;
}

WebRtcVideoDecoderFactoryCodecSupport APS5_VABI _ZNK6webrtc19VideoDecoderFactory17QueryCodecSupportERKNS_14SdpVideoFormatEb_nid_postfix(const WebRtcVideoDecoderFactory* factory, const WebRtcSdpVideoFormat* format, bool referenceScaling) {
    constexpr const char* message = "VideoDecoderFactory::QueryCodecSupport requires a valid factory and formats";
    WebRtcVideoDecoderFactoryCodecSupport support{false, false};
    if (referenceScaling)
        return support;
    const void* const* vtable = nullptr;
    CopyInChecked(&vtable, factory, sizeof(vtable), message);
    if (!vtable)
        throw std::invalid_argument(message);
    void* function = nullptr;
    CopyInChecked(&function, vtable + GetSupportedFormatsSlot, sizeof(function), message);
    if (!function)
        throw std::invalid_argument(message);
    const Format query = ReadFormat(reinterpret_cast<const VideoFormat*>(format), nullptr, message);
    VideoFormatVector supported{};
    reinterpret_cast<GetSupportedFormatsFunction>(function)(&supported, factory);
    const std::size_t count = CountFormats(supported, message);
    OwnedRelease release;
    std::vector<Format> formats;
    formats.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        formats.push_back(ReadFormat(supported.begin + i, &release.storage, message));
    if (supported.begin)
        release.storage.blocks.push_back(supported.begin);
    release.active = true;
    for (const Format& candidate : formats) {
        if (IsSameCodec(query, candidate)) {
            support.isSupported = true;
            break;
        }
    }
    return support;
}

}
