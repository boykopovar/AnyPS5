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
#include "prx/libwebrtc/src/SdpFormatStorage.hpp"
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

using namespace WebRtcSdpStorage;

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
