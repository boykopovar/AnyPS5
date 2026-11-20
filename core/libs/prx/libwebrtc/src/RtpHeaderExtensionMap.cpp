/* Derived from modules/rtp_rtcp/source/rtp_header_extension_map.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

constexpr std::uint8_t InvalidId = 0;
constexpr std::int32_t MinExtensionId = 1;
constexpr std::int32_t MaxExtensionId = 255;
constexpr std::size_t MaxKnownUriLength = 96;
constexpr std::size_t ValidationChunkSize = 4096;

struct ExtensionEntry {
    std::size_t type;
    std::string_view uri;
};

constexpr ExtensionEntry KnownExtensions[] = {
    {1, "urn:ietf:params:rtp-hdrext:toffset"},
    {2, "urn:ietf:params:rtp-hdrext:ssrc-audio-level"},
    {3, "urn:ietf:params:rtp-hdrext:csrc-audio-level"},
    {4, "http://www.webrtc.org/experiments/rtp-hdrext/inband-cn"},
    {5, "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time"},
    {6, "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time"},
    {7, "urn:3gpp:video-orientation"},
    {8, "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01"},
    {9, "http://www.webrtc.org/experiments/rtp-hdrext/transport-wide-cc-02"},
    {10, "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay"},
    {11, "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type"},
    {12, "http://www.webrtc.org/experiments/rtp-hdrext/video-layers-allocation00"},
    {13, "http://www.webrtc.org/experiments/rtp-hdrext/video-timing"},
    {14, "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"},
    {15, "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id"},
    {16, "urn:ietf:params:rtp-hdrext:sdes:mid"},
    {17, "http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-00"},
    {18, "https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension"},
    {19, "http://www.webrtc.org/experiments/rtp-hdrext/color-space"},
    {20, "http://www.webrtc.org/experiments/rtp-hdrext/video-frame-tracking-id"},
    {21, "http://www.webrtc.org/experiments/rtp-hdrext/corruption-detection"},
};

static_assert(sizeof(KnownExtensions) / sizeof(KnownExtensions[0]) == WebRtcRtpExtensionNumberOfExtensions - 1);

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

std::size_t FindTypeByUri(std::string_view uri) {
    for (const ExtensionEntry& entry : KnownExtensions) {
        if (entry.uri == uri)
            return entry.type;
    }
    return 0;
}

std::size_t FindTypeById(const WebRtcRtpHeaderExtensionMap& map, std::int32_t id) {
    for (std::size_t type = 1; type < WebRtcRtpExtensionNumberOfExtensions; ++type) {
        if (map.ids[type] == static_cast<std::uint8_t>(id))
            return type;
    }
    return 0;
}

void RegisterExtension(WebRtcRtpHeaderExtensionMap& map, std::int32_t id, std::size_t type) {
    if (type == 0 || id < MinExtensionId || id > MaxExtensionId)
        return;
    const std::size_t registeredType = FindTypeById(map, id);
    if (registeredType == type)
        return;
    if (registeredType != 0)
        return;
    if (map.ids[type] != InvalidId)
        return;
    map.ids[type] = static_cast<std::uint8_t>(id);
}

std::size_t ReadExtensionType(const WebRtcGuestString& uri, const char* message) {
    if (uri.capacity < WebRtcGuestStringInlineCapacity || uri.size > uri.capacity)
        throw std::invalid_argument(message);
    if (uri.capacity <= WebRtcGuestStringInlineCapacity)
        return FindTypeByUri(std::string_view(uri.buffer, static_cast<std::size_t>(uri.size)));
    if (!uri.pointer)
        throw std::invalid_argument(message);
    if (uri.size <= MaxKnownUriLength) {
        char buffer[MaxKnownUriLength];
        if (uri.size > 0)
            CopyInChecked(buffer, uri.pointer, static_cast<std::size_t>(uri.size), message);
        return FindTypeByUri(std::string_view(buffer, static_cast<std::size_t>(uri.size)));
    }
    char chunk[ValidationChunkSize];
    std::uint64_t remaining = uri.size;
    auto address = reinterpret_cast<std::uintptr_t>(uri.pointer);
    while (remaining > 0) {
        const std::size_t step = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, ValidationChunkSize));
        CopyInChecked(chunk, reinterpret_cast<const void*>(address), step, message);
        address += step;
        remaining -= step;
    }
    return 0;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
    WebRtcRtpHeaderExtensionMap* map, WebRtcRtpExtensionView extensions) {
    constexpr const char* message = "RtpHeaderExtensionMap constructor requires valid arguments";
    RequireWritable(map, sizeof(*map), message);
    if (extensions.size > 0) {
        if (!extensions.data || extensions.size > std::numeric_limits<std::size_t>::max() / sizeof(WebRtcRtpExtension))
            throw std::invalid_argument(message);
    }
    WebRtcRtpHeaderExtensionMap hostMap{};
    for (std::size_t index = 0; index < extensions.size; ++index) {
        WebRtcRtpExtension extension{};
        CopyInChecked(&extension, extensions.data + index, sizeof(extension), message);
        const std::size_t type = ReadExtensionType(extension.uri, message);
        RegisterExtension(hostMap, extension.id, type);
    }
    CopyOutChecked(map, &hostMap, sizeof(hostMap), message);
}

}
