/* Derived from call/audio_send_stream.cc and call/audio_receive_stream.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
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

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t ReleaseSlot = 1;
constexpr std::int32_t RtcpModeCompound = 1;
constexpr std::int32_t DefaultRtcpReportIntervalMs = 5000;
constexpr std::uint64_t DefaultJitterBufferMaxPackets = 200;

using ReleaseFunction = std::int32_t(APS5_VABI*)(const void*);

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

void InitEmptyString(WebRtcGuestString& value) {
    value.reserved = 0;
    std::memset(value.buffer, 0, sizeof(value.buffer));
    value.size = 0;
    value.capacity = WebRtcGuestStringInlineCapacity;
}

void InitDefaultCryptoOptions(WebRtcCryptoOptions& options) {
    options.srtp.enableGcmCryptoSuites = true;
    options.srtp.enableAes128Sha1_32CryptoCipher = false;
    options.srtp.enableAes128Sha1_80CryptoCipher = true;
    options.srtp.enableEncryptedRtpHeaderExtensions = true;
    options.sframe.requireFrameEncryption = false;
}

void ValidateString(const WebRtcGuestString& string, const char* message) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity)
        throw std::invalid_argument(message);
    if (string.capacity <= WebRtcGuestStringInlineCapacity)
        return;
    if (!string.pointer)
        throw std::invalid_argument(message);
    if (string.capacity + 1 >= GuestStringStorage::LargeAllocation) {
        void* raw = nullptr;
        CopyInChecked(&raw, static_cast<const void* const*>(static_cast<const void*>(string.pointer)) - 1, sizeof(raw), message);
        if (!raw)
            throw std::invalid_argument(message);
    }
}

template<typename TValue>
TValue ReadChecked(const TValue* object, const char* message) {
    TValue host{};
    CopyInChecked(&host, object, sizeof(host), message);
    return host;
}

template<typename TField, typename TObject>
TField* GuestField(TObject* object, std::size_t offset) {
    return reinterpret_cast<TField*>(reinterpret_cast<std::uintptr_t>(object) + offset);
}

void ReleaseString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
}

template<typename TValue>
std::size_t Count(const TValue* begin, const TValue* end, const TValue* capacityEnd, std::size_t elementSize, const char* message) {
    const auto first = reinterpret_cast<std::uintptr_t>(begin);
    const auto last = reinterpret_cast<std::uintptr_t>(end);
    const auto limit = reinterpret_cast<std::uintptr_t>(capacityEnd);
    if (last < first || limit < last || (!first && limit) || first % alignof(std::uint64_t) ||
        (last - first) % elementSize || (limit - first) % elementSize)
        throw std::invalid_argument(message);
    return (last - first) / elementSize;
}

ReleaseFunction ReadRelease(const void* object, const char* message) {
    const void* const* vtable = nullptr;
    CopyInChecked(&vtable, object, sizeof(vtable), message);
    if (!vtable)
        throw std::invalid_argument(message);
    const void* release = nullptr;
    CopyInChecked(&release, vtable + ReleaseSlot, sizeof(release), message);
    if (!release)
        throw std::invalid_argument(message);
    return reinterpret_cast<ReleaseFunction>(const_cast<void*>(release));
}

void ValidateReference(const void* object, const char* message) {
    if (object)
        ReadRelease(object, message);
}

void ReleaseReference(void* object, const char* message) {
    if (object)
        ReadRelease(object, message)(object);
}

std::size_t ValidateExtensions(const WebRtcRtpExtensionVector& extensions, const char* message) {
    const std::size_t count = Count(extensions.begin, extensions.end, extensions.capacityEnd, sizeof(WebRtcRtpExtension), message);
    for (std::size_t i = 0; i < count; ++i) {
        WebRtcRtpExtension extension{};
        CopyInChecked(&extension, extensions.begin + i, sizeof(extension), message);
        ValidateString(extension.uri, message);
    }
    return count;
}

std::vector<WebRtcAudioDecoderMapNode*> CollectDecoderNodes(const WebRtcAudioDecoderMap& map, WebRtcAudioDecoderMapNode* end, const char* message) {
    std::vector<WebRtcAudioDecoderMapNode*> nodes;
    if (map.size == 0) {
        if (map.root || map.beginNode != end)
            throw std::invalid_argument(message);
        return nodes;
    }
    if (!map.root || !map.beginNode || map.beginNode == end)
        throw std::invalid_argument(message);
    struct Entry {
        WebRtcAudioDecoderMapNode* node;
        WebRtcAudioDecoderMapNode* expectedParent;
    };
    std::vector<Entry> pending{{map.root, end}};
    while (!pending.empty()) {
        const Entry current = pending.back();
        pending.pop_back();
        if (nodes.size() >= map.size)
            throw std::invalid_argument(message);
        const WebRtcAudioDecoderMapLinks links = ReadChecked<WebRtcAudioDecoderMapLinks>(&current.node->links, message);
        if (links.parent != current.expectedParent)
            throw std::invalid_argument(message);
        RequireWritable(current.node, sizeof(WebRtcAudioDecoderMapNode), message);
        WebRtcValidateSdpAudioFormat_nid_no_patch(GuestField<const WebRtcSdpAudioFormat>(current.node, offsetof(WebRtcAudioDecoderMapNode, format)), message);
        nodes.push_back(current.node);
        if (links.right)
            pending.push_back({links.right, current.node});
        if (links.left)
            pending.push_back({links.left, current.node});
    }
    if (nodes.size() != map.size)
        throw std::invalid_argument(message);
    const WebRtcAudioDecoderMapNode* leftmost = map.root;
    for (WebRtcAudioDecoderMapNode* left = ReadChecked(&leftmost->links, message).left; left; left = ReadChecked(&leftmost->links, message).left)
        leftmost = left;
    if (leftmost != map.beginNode)
        throw std::invalid_argument(message);
    return nodes;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(
    WebRtcAudioSendCodecSpec* spec, std::int32_t payloadType, const WebRtcSdpAudioFormat* format) {
    constexpr const char* message = "AudioSendStream::Config::SendCodecSpec constructor requires valid objects";
    RequireWritable(spec, sizeof(*spec), message);
    CopyOutChecked(GuestField<std::int32_t>(spec, offsetof(WebRtcAudioSendCodecSpec, payloadType)), &payloadType, sizeof(payloadType), message);
    _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(GuestField<WebRtcSdpAudioFormat>(spec, offsetof(WebRtcAudioSendCodecSpec, format)), format);
    WebRtcAudioSendCodecSpec host = ReadChecked(spec, message);
    for (bool& flag : host.flags)
        flag = false;
    for (WebRtcAudioStreamOptionalInt* optional : {&host.cngPayloadType, &host.redPayloadType, &host.targetBitrateBps}) {
        reinterpret_cast<unsigned char*>(&optional->value)[0] = 0;
        optional->engaged = false;
    }
    CopyOutChecked(spec, &host, sizeof(host), message);
}

void APS5_VABI _ZN6webrtc15AudioSendStream6Config13SendCodecSpecD1Ev_nid_postfix(WebRtcAudioSendCodecSpec* spec) {
    constexpr const char* message = "AudioSendStream::Config::SendCodecSpec destructor requires a valid object";
    RequireWritable(spec, sizeof(*spec), message);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(GuestField<WebRtcSdpAudioFormat>(spec, offsetof(WebRtcAudioSendCodecSpec, format)));
}

void APS5_VABI _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(WebRtcAudioSendStreamStats* stats) {
    constexpr const char* message = "AudioSendStream::Stats destructor requires a valid object";
    RequireWritable(stats, sizeof(*stats), message);
    WebRtcAudioSendStreamStats host = ReadChecked(stats, message);
    ValidateString(host.codecName, message);
    Count(host.reportBlockDatas.begin, host.reportBlockDatas.end, host.reportBlockDatas.capacityEnd, alignof(std::uint64_t), message);
    if (host.reportBlockDatas.begin)
        _ZdlPv_nid_postfix(host.reportBlockDatas.begin);
    ReleaseString(host.codecName);
}

void APS5_VABI _ZN6webrtc15AudioSendStream6ConfigC1EPNS_9TransportE_nid_postfix(
    WebRtcAudioSendStreamConfig* config, void* sendTransport) {
    constexpr const char* message = "AudioSendStream::Config constructor requires a valid object";
    WebRtcAudioSendStreamConfig hostConfig{};
    InitEmptyString(hostConfig.rtp.rid);
    InitEmptyString(hostConfig.rtp.mid);
    InitEmptyString(hostConfig.rtp.cName);
    hostConfig.rtp.rtcpMode = RtcpModeCompound;
    hostConfig.rtcpReportIntervalMs = DefaultRtcpReportIntervalMs;
    hostConfig.sendTransport = sendTransport;
    hostConfig.minBitrateBps = -1;
    hostConfig.maxBitrateBps = -1;
    hostConfig.bitratePriority = 1.0;
    InitEmptyString(hostConfig.trackId);
    InitDefaultCryptoOptions(hostConfig.cryptoOptions);
    CopyOutChecked(config, &hostConfig, sizeof(hostConfig), message);
}

void APS5_VABI _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(WebRtcAudioSendStreamConfig* config) {
    constexpr const char* message = "AudioSendStream::Config destructor requires a valid object";
    RequireWritable(config, sizeof(*config), message);
    WebRtcAudioSendStreamConfig host = ReadChecked(config, message);
    ValidateString(host.rtp.rid, message);
    ValidateString(host.rtp.mid, message);
    const std::size_t extensionCount = ValidateExtensions(host.rtp.extensions, message);
    ValidateString(host.rtp.cName, message);
    if (host.audioNetworkAdaptorConfig.engaged)
        ValidateString(host.audioNetworkAdaptorConfig.value, message);
    if (host.sendCodecSpec.engaged)
        WebRtcValidateSdpAudioFormat_nid_no_patch(GuestField<const WebRtcSdpAudioFormat>(config, offsetof(WebRtcAudioSendStreamConfig, sendCodecSpec.value.format)), message);
    ValidateReference(host.encoderFactory, message);
    ValidateString(host.trackId, message);
    ValidateReference(host.frameEncryptor, message);
    ValidateReference(host.frameTransformer, message);

    ReleaseReference(host.frameTransformer, message);
    ReleaseReference(host.frameEncryptor, message);
    ReleaseString(host.trackId);
    ReleaseReference(host.encoderFactory, message);
    if (host.sendCodecSpec.engaged)
        _ZN6webrtc15AudioSendStream6Config13SendCodecSpecD1Ev_nid_postfix(GuestField<WebRtcAudioSendCodecSpec>(config, offsetof(WebRtcAudioSendStreamConfig, sendCodecSpec.value)));
    if (host.audioNetworkAdaptorConfig.engaged)
        ReleaseString(host.audioNetworkAdaptorConfig.value);
    ReleaseString(host.rtp.cName);
    for (std::size_t i = extensionCount; i > 0; --i)
        _ZN6webrtc12RtpExtensionD1Ev_nid_postfix(host.rtp.extensions.begin + (i - 1));
    if (host.rtp.extensions.begin)
        _ZdlPv_nid_postfix(host.rtp.extensions.begin);
    ReleaseString(host.rtp.mid);
    ReleaseString(host.rtp.rid);
}

void APS5_VABI _ZN6webrtc27AudioReceiveStreamInterface5StatsD1Ev_nid_postfix(WebRtcAudioReceiveStreamStatsPrefix* stats) {
    constexpr const char* message = "AudioReceiveStreamInterface::Stats destructor requires a valid object";
    RequireWritable(stats, sizeof(*stats), message);
    WebRtcAudioReceiveStreamStatsPrefix host = ReadChecked(stats, message);
    ValidateString(host.codecName, message);
    ReleaseString(host.codecName);
}

void APS5_VABI _ZN6webrtc27AudioReceiveStreamInterface6ConfigC1Ev_nid_postfix(WebRtcAudioReceiveStreamConfig* config) {
    constexpr const char* message = "AudioReceiveStreamInterface::Config constructor requires a valid object";
    WebRtcAudioReceiveStreamConfig hostConfig{};
    hostConfig.rtcpMode = RtcpModeCompound;
    hostConfig.jitterBufferMaxPackets = DefaultJitterBufferMaxPackets;
    InitEmptyString(hostConfig.syncGroup);
    hostConfig.decoderMap.beginNode = reinterpret_cast<WebRtcAudioDecoderMapNode*>(
        reinterpret_cast<std::uintptr_t>(config) +
        offsetof(WebRtcAudioReceiveStreamConfig, decoderMap) +
        offsetof(WebRtcAudioDecoderMap, root));
    InitDefaultCryptoOptions(hostConfig.cryptoOptions);
    CopyOutChecked(config, &hostConfig, sizeof(hostConfig), message);
}

void APS5_VABI _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(WebRtcAudioReceiveStreamConfig* config) {
    constexpr const char* message = "AudioReceiveStreamInterface::Config destructor requires a valid object";
    RequireWritable(config, sizeof(*config), message);
    WebRtcAudioReceiveStreamConfig host = ReadChecked(config, message);
    ValidateString(host.syncGroup, message);
    WebRtcAudioDecoderMapNode* const end = GuestField<WebRtcAudioDecoderMapNode>(config, offsetof(WebRtcAudioReceiveStreamConfig, decoderMap.root));
    const std::vector<WebRtcAudioDecoderMapNode*> nodes = CollectDecoderNodes(host.decoderMap, end, message);
    ValidateReference(host.decoderFactory, message);
    ValidateReference(host.frameDecryptor, message);
    ValidateReference(host.frameTransformer, message);

    ReleaseReference(host.frameTransformer, message);
    ReleaseReference(host.frameDecryptor, message);
    ReleaseReference(host.decoderFactory, message);
    for (WebRtcAudioDecoderMapNode* node : nodes) {
        _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(GuestField<WebRtcSdpAudioFormat>(node, offsetof(WebRtcAudioDecoderMapNode, format)));
        _ZdlPv_nid_postfix(node);
    }
    ReleaseString(host.syncGroup);
}

}
