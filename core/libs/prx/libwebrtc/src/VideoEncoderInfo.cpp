/* Derived from api/video_codecs/video_encoder.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/include/GuestPtr.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::int32_t DefaultMinPixelsPerFrame = 320 * 180;
constexpr std::uint8_t MaxFramerateFraction = 255;
constexpr std::int32_t VideoFrameBufferTypeI420 = 1;
constexpr char DefaultImplementationName[] = "unknown";
constexpr std::uint64_t AllocatedBit = 1;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

template<std::size_t TInlineBytes>
void InitInlined(WebRtcInlinedVector<TInlineBytes>& vector) {
    vector.metadata = 0;
    std::memset(vector.inlined, 0, sizeof(vector.inlined));
}

template<std::size_t TInlineBytes>
void ValidateInlined(const WebRtcInlinedVector<TInlineBytes>& vector, std::size_t elementSize, std::size_t inlineCapacity) {
    const std::uint64_t size = vector.metadata >> 1;
    if (vector.metadata & AllocatedBit) {
        if (!vector.allocated.data || vector.allocated.capacity <= inlineCapacity || size > vector.allocated.capacity ||
            reinterpret_cast<std::uintptr_t>(vector.allocated.data) % elementSize ||
            vector.allocated.capacity > SIZE_MAX / elementSize)
            throw std::invalid_argument("EncoderInfo requires valid inlined vectors");
    } else if (size > inlineCapacity) {
        throw std::invalid_argument("EncoderInfo requires valid inlined vectors");
    }
}

template<std::size_t TInlineBytes>
void ReleaseInlined(WebRtcInlinedVector<TInlineBytes>& vector, std::size_t elementSize) {
    if (vector.metadata & AllocatedBit)
        GuestStringStorage::Deallocate(vector.allocated.data, vector.allocated.capacity * elementSize);
    InitInlined(vector);
}

void ValidateString(const WebRtcGuestString& string) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity ||
        (string.capacity > WebRtcGuestStringInlineCapacity && !string.pointer))
        throw std::invalid_argument("EncoderInfo requires a valid implementation name");
}

void ValidateLimits(const WebRtcVideoEncoderInfo& info) {
    const auto begin = reinterpret_cast<std::uintptr_t>(info.resolutionBitrateLimitsBegin);
    const auto end = reinterpret_cast<std::uintptr_t>(info.resolutionBitrateLimitsEnd);
    const auto capacity = reinterpret_cast<std::uintptr_t>(info.resolutionBitrateLimitsCapacity);
    constexpr std::size_t size = sizeof(WebRtcResolutionBitrateLimits);
    if (end < begin || capacity < end || (!begin && capacity) || begin % alignof(WebRtcResolutionBitrateLimits) ||
        (end - begin) % size || (capacity - begin) % size)
        throw std::invalid_argument("EncoderInfo requires a valid resolution_bitrate_limits vector");
}

}

extern "C" {

void APS5_VABI _ZN6webrtc12VideoEncoder11EncoderInfoC1Ev_nid_postfix(WebRtcVideoEncoderInfo* info) {
    if (!info)
        throw std::invalid_argument("EncoderInfo constructor requires an object");
    WebRtcVideoEncoderInfo host{};
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(info, sizeof(*info)) ||
            !GuestPtr<const WebRtcVideoEncoderInfo>(info).CopyIn(host))
            throw std::invalid_argument("EncoderInfo constructor requires a writable object");
    }
    host.qpThresholdLow = 0;
    host.qpThresholdHigh = 0;
    host.hasQpThresholds = false;
    host.minPixelsPerFrame = DefaultMinPixelsPerFrame;
    host.requestedResolutionAlignment = 1;
    host.applyAlignmentToAllSimulcastLayers = false;
    host.supportsNativeHandle = false;
    WebRtcGuestString& name = host.implementationName;
    name.reserved = 0;
    std::memcpy(name.buffer, DefaultImplementationName, sizeof(DefaultImplementationName));
    name.size = sizeof(DefaultImplementationName) - 1;
    name.capacity = WebRtcGuestStringInlineCapacity;
    host.hasTrustedRateController = false;
    host.isHardwareAccelerated = true;
    for (auto& fractions : host.fpsAllocation)
        InitInlined(fractions);
    host.fpsAllocation[0].metadata = 1 << 1;
    host.fpsAllocation[0].inlined[0] = MaxFramerateFraction;
    host.resolutionBitrateLimitsBegin = nullptr;
    host.resolutionBitrateLimitsEnd = nullptr;
    host.resolutionBitrateLimitsCapacity = nullptr;
    host.supportsSimulcast = false;
    InitInlined(host.preferredPixelFormats);
    host.preferredPixelFormats.metadata = 1 << 1;
    std::memcpy(host.preferredPixelFormats.inlined, &VideoFrameBufferTypeI420, sizeof(VideoFrameBufferTypeI420));
    host.isQpTrusted = false;
    host.hasIsQpTrusted = false;
    host.minQp = 0;
    host.hasMinQp = false;
    ErrnoGuard guard;
    if (!GuestPtr<WebRtcVideoEncoderInfo>(info).CopyOut(host))
        throw std::invalid_argument("EncoderInfo constructor requires a writable object");
}

void APS5_VABI _ZN6webrtc12VideoEncoder11EncoderInfoD1Ev_nid_postfix(WebRtcVideoEncoderInfo* info) {
    if (!info)
        throw std::invalid_argument("EncoderInfo destructor requires an object");
    WebRtcVideoEncoderInfo host{};
    {
        ErrnoGuard guard;
        if (!GuestWritable_nid_no_patch(info, sizeof(*info)) ||
            !GuestPtr<const WebRtcVideoEncoderInfo>(info).CopyIn(host))
            throw std::invalid_argument("EncoderInfo destructor requires a writable object");
    }
    ValidateString(host.implementationName);
    for (const auto& fractions : host.fpsAllocation)
        ValidateInlined(fractions, sizeof(std::uint8_t), WebRtcFpsAllocationInlineCapacity);
    ValidateLimits(host);
    ValidateInlined(host.preferredPixelFormats, sizeof(std::int32_t), WebRtcPreferredPixelFormatsInlineCapacity);
    ReleaseInlined(host.preferredPixelFormats, sizeof(std::int32_t));
    if (host.resolutionBitrateLimitsBegin)
        GuestStringStorage::Deallocate(host.resolutionBitrateLimitsBegin,
            static_cast<std::size_t>(host.resolutionBitrateLimitsCapacity - host.resolutionBitrateLimitsBegin) * sizeof(WebRtcResolutionBitrateLimits));
    host.resolutionBitrateLimitsBegin = nullptr;
    host.resolutionBitrateLimitsEnd = nullptr;
    host.resolutionBitrateLimitsCapacity = nullptr;
    for (std::size_t index = WebRtcMaxSpatialLayers; index-- > 0;)
        ReleaseInlined(host.fpsAllocation[index], sizeof(std::uint8_t));
    WebRtcGuestString& name = host.implementationName;
    if (name.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(name.pointer, name.capacity + 1);
    name.buffer[0] = '\0';
    name.size = 0;
    name.capacity = WebRtcGuestStringInlineCapacity;
    ErrnoGuard guard;
    if (!GuestPtr<WebRtcVideoEncoderInfo>(info).CopyOut(host))
        throw std::invalid_argument("EncoderInfo destructor requires a writable object");
}

}
