// https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/media/base/codec.cc
/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libwebrtc/include/ModuleStorage.hpp"
#include "prx/libwebrtc/src/SdpFormatStorage.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/libc.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

using namespace WebRtcSdpStorage;

namespace {

constexpr std::size_t LargeVectorBytes = 0x1000;

void APS5_VABI DeleteRtpCodecParameters_nid_no_patch(WebRtcRtpCodecParameters* parameters) {
    DestroyRtpCodecParameters_nid_no_patch(parameters);
    _ZdlPv_nid_postfix(parameters);
}

void* CodecParametersVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&DestroyRtpCodecParameters_nid_no_patch),
    reinterpret_cast<void*>(&DeleteRtpCodecParameters_nid_no_patch),
};

WebRtcRtpCodecParameters* APS5_VABI CodecToCodecParameters_nid_no_patch(WebRtcRtpCodecParameters* result, const WebRtcCodec* codec) {
    constexpr const char* message = "Codec::ToCodecParameters requires a valid codec and output";
    RequireWritable(result, sizeof(*result), message);
    WebRtcCodec snapshot{};
    CopyInChecked(&snapshot, codec, sizeof(snapshot), message);
    if (snapshot.type != 0 && snapshot.type != WebRtcCodecTypeVideo)
        throw std::invalid_argument("Codec::ToCodecParameters invalid codec type");
    ValidateString(snapshot.name, message);
    StringGuard name;
    CopyString(snapshot.name, name.value, message);
    name.active = true;
    WebRtcRtpCodecParameters output{};
    output.codec.vtable = CodecParametersVtable + 2;
    output.codec.name = name.value;
    output.codec.kind = snapshot.type;
    output.codec.clockRate = snapshot.clockrate;
    output.codec.hasClockRate = true;
    if (snapshot.type == 0) {
        output.codec.numChannels = static_cast<std::int32_t>(snapshot.channels);
        output.codec.hasNumChannels = true;
    }
    output.payloadType = snapshot.id;
    NodeGuard map{CloneMap(snapshot.params, EndNode(codec->params), output.codec.parameters, message)};
    if (output.codec.parameters.root)
        output.codec.parameters.root->links.parent = EndNode(result->codec.parameters);
    else
        output.codec.parameters.beginNode = EndNode(result->codec.parameters);
    if (!GuestCopyOut_nid_no_patch(result, &output, sizeof(output)))
        throw std::invalid_argument(message);
    name.active = false;
    map.active = false;
    return result;
}

void APS5_VABI DeleteCodec_nid_no_patch(WebRtcCodec* codec) {
    _ZN7cricket5CodecD1Ev_nid_postfix(codec);
    _ZdlPv_nid_postfix(codec);
}

void* const CodecVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&_ZN7cricket5CodecD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteCodec_nid_no_patch),
    reinterpret_cast<void*>(&CodecToCodecParameters_nid_no_patch),
};

void InitOptionalEmpty(WebRtcOptionalString& optional) {
    reinterpret_cast<unsigned char*>(&optional.value)[0] = 0;
    optional.engaged = false;
}

void ValidateOptional(const WebRtcOptionalString& optional, const char* message) {
    if (optional.engaged)
        ValidateString(optional.value, message);
}

void ReleaseOptional(WebRtcOptionalString& optional) {
    if (optional.engaged)
        ReleaseString(optional.value);
}

std::size_t ValidateFeedback(const WebRtcFeedbackParamVector& params, const char* message) {
    if (!params.begin) {
        if (params.end || params.capacityEnd)
            throw std::invalid_argument(message);
        return 0;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(params.begin);
    const auto end = reinterpret_cast<std::uintptr_t>(params.end);
    const auto capacityEnd = reinterpret_cast<std::uintptr_t>(params.capacityEnd);
    if (end < begin || capacityEnd < end || capacityEnd == begin)
        throw std::invalid_argument(message);
    if ((end - begin) % sizeof(WebRtcFeedbackParam) != 0 || (capacityEnd - begin) % sizeof(WebRtcFeedbackParam) != 0)
        throw std::invalid_argument(message);
    if (capacityEnd - begin >= LargeVectorBytes) {
        void* raw = nullptr;
        CopyInChecked(&raw, static_cast<const void* const*>(static_cast<const void*>(params.begin)) - 1, sizeof(raw), message);
        if (!raw)
            throw std::invalid_argument(message);
    }
    const std::size_t count = (end - begin) / sizeof(WebRtcFeedbackParam);
    for (std::size_t index = 0; index < count; ++index) {
        WebRtcFeedbackParam entry{};
        CopyInChecked(&entry, params.begin + index, sizeof(entry), message);
        ValidateString(entry.id, message);
        ValidateString(entry.param, message);
    }
    return count;
}

}

extern "C" {

WebRtcCodec* APS5_VABI _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(WebRtcCodec* result, const WebRtcSdpVideoFormat* format) {
    constexpr const char* message = "CreateVideoCodec requires a valid format and a writable result";
    RequireWritable(result, sizeof(*result), message);
    WebRtcSdpVideoFormat snapshot{};
    CopyInChecked(&snapshot, format, sizeof(snapshot), message);
    ValidateString(snapshot.name, message);
    ValidateModes(snapshot.scalabilityModes, message);

    StringGuard nameGuard;
    CopyString(snapshot.name, nameGuard.value, message);
    nameGuard.active = true;

    WebRtcStringMap clonedMap{};
    CloneMap(snapshot.parameters, EndNode(format->parameters), clonedMap, message);

    const std::uint64_t modeCount = ModeCount(snapshot.scalabilityModes);
    const std::uint8_t* const sourceModes = ModesAllocated(snapshot.scalabilityModes) ? snapshot.scalabilityModes.allocated.data : snapshot.scalabilityModes.inlined;
    std::uint8_t* allocatedModes = nullptr;
    std::uint64_t allocatedCapacity = 0;
    if (modeCount > WebRtcScalabilityModeCount) {
        allocatedCapacity = std::max<std::uint64_t>(2 * WebRtcScalabilityModeCount, modeCount);
        try {
            allocatedModes = static_cast<std::uint8_t*>(GuestStringStorage::Allocate(allocatedCapacity));
        } catch (...) {
            std::vector<WebRtcStringMapNode*> nodes;
            ValidateMapNodes(clonedMap, message, nodes);
            DestroyMapNodes(nodes, clonedMap);
            throw;
        }
        std::memcpy(allocatedModes, sourceModes, modeCount);
    }

    result->vtable = CodecVtable + 2;
    result->type = WebRtcCodecTypeVideo;
    result->id = WebRtcCodecIdNotSet;
    result->name = nameGuard.value;
    nameGuard.active = false;
    result->clockrate = WebRtcVideoCodecClockrate;
    result->bitrate = 0;
    result->channels = 0;
    InitOptionalEmpty(result->packetization);
    if (allocatedModes) {
        result->scalabilityModes.allocated.data = allocatedModes;
        result->scalabilityModes.allocated.capacity = allocatedCapacity;
        result->scalabilityModes.sizeAndIsAllocated = (modeCount << 1) | 1;
    } else {
        if (modeCount > 0)
            std::memcpy(result->scalabilityModes.inlined, sourceModes, modeCount);
        result->scalabilityModes.sizeAndIsAllocated = modeCount << 1;
    }
    InitOptionalEmpty(result->txMode);
    result->params = clonedMap;
    if (result->params.size == 0)
        result->params.beginNode = EndNode(result->params);
    else
        result->params.root->links.parent = EndNode(result->params);
    result->feedbackParams.begin = nullptr;
    result->feedbackParams.end = nullptr;
    result->feedbackParams.capacityEnd = nullptr;
    return result;
}

void APS5_VABI _ZN7cricket5CodecD1Ev_nid_postfix(WebRtcCodec* codec) {
    constexpr const char* message = "Codec destructor requires a valid object";
    RequireWritable(codec, sizeof(*codec), message);
    std::vector<WebRtcStringMapNode*> nodes;
    ValidateString(codec->name, message);
    ValidateOptional(codec->packetization, message);
    ValidateModes(codec->scalabilityModes, message);
    ValidateOptional(codec->txMode, message);
    ValidateMapNodes(codec->params, message, nodes);
    const std::size_t feedbackCount = ValidateFeedback(codec->feedbackParams, message);

    for (std::size_t index = feedbackCount; index > 0; --index) {
        WebRtcFeedbackParam& entry = codec->feedbackParams.begin[index - 1];
        ReleaseString(entry.param);
        ReleaseString(entry.id);
    }
    if (codec->feedbackParams.begin) {
        const auto capacityBytes = static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(codec->feedbackParams.capacityEnd) - reinterpret_cast<std::uintptr_t>(codec->feedbackParams.begin));
        WebRtcModuleStorage::Free(codec->feedbackParams.begin, capacityBytes);
        codec->feedbackParams.begin = nullptr;
        codec->feedbackParams.end = nullptr;
        codec->feedbackParams.capacityEnd = nullptr;
    }
    DestroyMapNodes(nodes, codec->params);
    ReleaseOptional(codec->txMode);
    if (ModesAllocated(codec->scalabilityModes)) {
        GuestStringStorage::Deallocate(codec->scalabilityModes.allocated.data, codec->scalabilityModes.allocated.capacity);
        codec->scalabilityModes.sizeAndIsAllocated = 0;
    }
    ReleaseOptional(codec->packetization);
    ReleaseString(codec->name);
}

}
