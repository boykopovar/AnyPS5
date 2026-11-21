/* Derived from modules/video_coding/include/video_codec_interface.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/libc.hpp"

#include <cstring>
#include <stdexcept>

namespace {

template<std::size_t TInlineBytes>
void Release(WebRtcInlinedVector<TInlineBytes>& vector) {
    if ((vector.metadata & 1) && vector.allocated.data)
        _ZdlPv_nid_postfix(vector.allocated.data);
}

template<typename TValue>
std::size_t Count(TValue* begin, TValue* end, TValue* capacity) {
    const auto first = reinterpret_cast<std::uintptr_t>(begin);
    const auto last = reinterpret_cast<std::uintptr_t>(end);
    const auto limit = reinterpret_cast<std::uintptr_t>(capacity);
    if (last < first || limit < last || (!first && limit) || first % alignof(TValue) ||
        (last - first) % sizeof(TValue) || (limit - first) % sizeof(TValue))
        throw std::invalid_argument("CodecSpecificInfo requires a valid vector");
    return (last - first) / sizeof(TValue);
}

void Release(WebRtcFrameDependencyTemplate& frameDependency) {
    Release(frameDependency.decodeTargetIndications);
    Release(frameDependency.frameDiffs);
    Release(frameDependency.chainDiffs);
}

void Release(WebRtcGenericFrameInfo& info) {
    if (info.partOfChain.begin)
        _ZdlPv_nid_postfix(info.partOfChain.begin);
    Release(info.encoderBuffers);
    Release(info.frameDependency);
}

void Release(WebRtcFrameDependencyStructure& structure, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        Release(structure.templatesBegin[i]);
    if (structure.templatesBegin)
        _ZdlPv_nid_postfix(structure.templatesBegin);
    Release(structure.resolutions);
    Release(structure.decodeTargetProtectedByChain);
}

bool HoldsData(const WebRtcFrameInstrumentationVariant& variant) {
    if (variant.index == WebRtcFrameInstrumentationSyncIndex || variant.index == WebRtcFrameInstrumentationValueless)
        return false;
    if (variant.index != WebRtcFrameInstrumentationDataIndex)
        throw std::invalid_argument("CodecSpecificInfo requires a valid frame instrumentation variant");
    const auto& data = variant.data;
    Count(data.sampleValuesBegin, data.sampleValuesEnd, data.sampleValuesCapacity);
    return true;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc17CodecSpecificInfoC1Ev_nid_postfix(WebRtcCodecSpecificInfo* info) {
    if (!info) throw std::invalid_argument("CodecSpecificInfo requires an object");
    info->codecType = 0;
    std::memset(info->codecSpecific, 0, sizeof(info->codecSpecific));
    info->endOfPicture = true;
    info->hasGenericFrameInfo = false;
    info->hasTemplateStructure = false;
    info->hasScalabilityMode = false;
    info->hasFrameInstrumentationData = false;
}

void APS5_VABI _ZN6webrtc17CodecSpecificInfoD1Ev_nid_postfix(WebRtcCodecSpecificInfo* info) {
    if (!info) throw std::invalid_argument("CodecSpecificInfo destructor requires an object");
    const bool holdsData = info->hasFrameInstrumentationData && HoldsData(info->frameInstrumentationData);
    const auto& structure = info->templateStructure;
    const auto templates = info->hasTemplateStructure ? Count(structure.templatesBegin, structure.templatesEnd, structure.templatesCapacity) : 0;
    if (holdsData && info->frameInstrumentationData.data.sampleValuesBegin)
        _ZdlPv_nid_postfix(info->frameInstrumentationData.data.sampleValuesBegin);
    if (info->hasTemplateStructure)
        Release(info->templateStructure, templates);
    if (info->hasGenericFrameInfo)
        Release(info->genericFrameInfo);
}

}
