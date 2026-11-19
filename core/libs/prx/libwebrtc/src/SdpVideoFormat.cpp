/* Derived from api/video_codecs/sdp_video_format.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 * Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS. All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/libc.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

using ScalabilityModes = WebRtcInlinedVector<WebRtcScalabilityModeCount>;

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
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

std::uint64_t ModeCount(const ScalabilityModes& modes) {
    return modes.metadata >> 1;
}

bool IsAllocated(const ScalabilityModes& modes) {
    return (modes.metadata & AllocatedBit) != 0;
}

void ValidateModes(const ScalabilityModes& modes, const char* message) {
    const std::uint64_t count = ModeCount(modes);
    if (!IsAllocated(modes)) {
        if (count > WebRtcScalabilityModeCount)
            throw std::invalid_argument(message);
        return;
    }
    if (!modes.allocated.data || modes.allocated.capacity == 0 || count > modes.allocated.capacity)
        throw std::invalid_argument(message);
}

void CopyModes(ScalabilityModes& destination, const ScalabilityModes& source, const char* message) {
    const std::uint64_t count = ModeCount(source);
    if (count == 0) {
        destination.metadata = 0;
        return;
    }
    if (!IsAllocated(source)) {
        destination = source;
        return;
    }
    std::vector<std::uint8_t> staged(count);
    CopyInChecked(staged.data(), source.allocated.data, count, message);
    const std::uint64_t capacity = std::max<std::uint64_t>(WebRtcScalabilityModeCount * 2, count);
    void* data = _Znwm_nid_postfix(capacity);
    std::memcpy(data, staged.data(), count);
    destination.allocated.data = data;
    destination.allocated.capacity = capacity;
    destination.metadata = source.metadata;
}

void ReleaseModes(ScalabilityModes& modes) {
    if (IsAllocated(modes))
        _ZdlPv_nid_postfix(modes.allocated.data);
    modes.metadata = 0;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(WebRtcSdpVideoFormat* format, const WebRtcSdpVideoFormat* other) {
    constexpr const char* message = "SdpVideoFormat copy constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    WebRtcSdpVideoFormat snapshot{};
    CopyInChecked(&snapshot, other, sizeof(snapshot), message);
    ValidateModes(snapshot.scalabilityModes, message);

    ScalabilityModes modes{};
    CopyModes(modes, snapshot.scalabilityModes, message);
    try {
        WebRtcCopySdpNameAndParameters_nid_no_patch(&format->name, &format->parameters, &other->name, &other->parameters, message);
    } catch (...) {
        ReleaseModes(modes);
        throw;
    }
    format->scalabilityModes = modes;
}

void APS5_VABI _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(WebRtcSdpVideoFormat* format, WebRtcSdpVideoFormat* other) {
    constexpr const char* message = "SdpVideoFormat move constructor requires valid objects";
    RequireWritable(format, sizeof(*format), message);
    RequireWritable(other, sizeof(*other), message);
    if (format == other)
        throw std::invalid_argument(message);
    ValidateModes(other->scalabilityModes, message);

    WebRtcMoveSdpNameAndParameters_nid_no_patch(&format->name, &format->parameters, &other->name, &other->parameters, message);
    format->scalabilityModes = other->scalabilityModes;
    other->scalabilityModes.metadata = 0;
}

void APS5_VABI _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(WebRtcSdpVideoFormat* format) {
    constexpr const char* message = "SdpVideoFormat destructor requires a valid object";
    RequireWritable(format, sizeof(*format), message);
    ValidateModes(format->scalabilityModes, message);
    WebRtcDestroySdpNameAndParameters_nid_no_patch(&format->name, &format->parameters, message);
    ReleaseModes(format->scalabilityModes);
}

}
