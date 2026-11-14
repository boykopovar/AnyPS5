/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::size_t SampleRateHzSlot = 2;
constexpr std::size_t NumChannelsSlot = 3;
constexpr std::size_t EncodeImplSlot = 29;

using SampleRateHz = int(APS5_VABI*)(const WebRtcAudioEncoder*);
using NumChannels = std::size_t(APS5_VABI*)(const WebRtcAudioEncoder*);
using EncodeImpl = void*(APS5_VABI*)(void*, WebRtcAudioEncoder*, std::uint32_t, const std::int16_t*, std::size_t, WebRtcBuffer*);

template<typename TFunction> TFunction Virtual(const WebRtcAudioEncoder* encoder, std::size_t slot) {
    const void* const* vtable;
    std::memcpy(&vtable, encoder, sizeof(vtable));
    if (!vtable || !vtable[slot])
        throw std::invalid_argument("AudioEncoder::Encode requires an encoder virtual function");
    return reinterpret_cast<TFunction>(const_cast<void*>(vtable[slot]));
}

}

extern "C" {

void* APS5_VABI _ZN6webrtc12AudioEncoder6EncodeEjN3rtc9ArrayViewIKsLln4711EEEPNS1_7BufferTIhLb0EEE_nid_postfix(void* info, WebRtcAudioEncoder* encoder, std::uint32_t rtpTimestamp, const std::int16_t* audio, std::size_t samples, WebRtcBuffer* encoded) {
    if (!info)
        throw std::invalid_argument("AudioEncoder::Encode requires a result");
    if (!encoder)
        throw std::invalid_argument("AudioEncoder::Encode requires an encoder");
    const std::size_t channels = Virtual<NumChannels>(encoder, NumChannelsSlot)(encoder);
    const int sampleRateHz = Virtual<SampleRateHz>(encoder, SampleRateHzSlot)(encoder);
    if (samples != channels * static_cast<std::size_t>(sampleRateHz) / 100)
        throw std::runtime_error("AudioEncoder::Encode: the audio is not 10 ms of every channel");
    if (!encoded)
        throw std::invalid_argument("AudioEncoder::Encode requires an output buffer");
    const std::uint64_t oldSize = encoded->size;
    Virtual<EncodeImpl>(encoder, EncodeImplSlot)(info, encoder, rtpTimestamp, audio, samples, encoded);
    std::uint64_t encodedBytes;
    std::memcpy(&encodedBytes, info, sizeof(encodedBytes));
    if (encoded->size - oldSize != encodedBytes)
        throw std::runtime_error("AudioEncoder::Encode: EncodeImpl wrote a different size than it reported");
    return info;
}

}
