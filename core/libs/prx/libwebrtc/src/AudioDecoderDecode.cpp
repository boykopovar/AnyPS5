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

#include <cstring>
#include <stdexcept>

namespace {

template <typename TCallback>
TCallback Callback(const WebRtcAudioDecoder* decoder, std::size_t slot) {
    if (!decoder) throw std::invalid_argument("null AudioDecoder");
    const unsigned char* vtable;
    std::memcpy(&vtable, decoder, sizeof(vtable));
    if (!vtable) throw std::invalid_argument("null AudioDecoder vtable");
    TCallback callback;
    std::memcpy(&callback, vtable + slot * sizeof(callback), sizeof(callback));
    if (!callback) throw std::invalid_argument("null AudioDecoder callback");
    return callback;
}

using Duration = int (APS5_VABI *)(const WebRtcAudioDecoder*, const std::uint8_t*, std::size_t);
using Channels = std::size_t (APS5_VABI *)(const WebRtcAudioDecoder*);
using Decode = int (APS5_VABI *)(WebRtcAudioDecoder*, const std::uint8_t*, std::size_t, int, std::int16_t*, std::int32_t*);

}

extern "C" {

int APS5_VABI _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz, std::size_t maxDecodedBytes, std::int16_t* decoded, std::int32_t* speechType) {
    int duration = Callback<Duration>(decoder, 8)(decoder, encoded, encodedLen);
    if (duration >= 0 && duration * Callback<Channels>(decoder, 12)(decoder) * sizeof(std::int16_t) > maxDecodedBytes) return -1;
    return Callback<Decode>(decoder, 13)(decoder, encoded, encodedLen, sampleRateHz, decoded, speechType);
}

int APS5_VABI _ZN6webrtc12AudioDecoder15DecodeRedundantEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(WebRtcAudioDecoder* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz, std::size_t maxDecodedBytes, std::int16_t* decoded, std::int32_t* speechType) {
    int duration = Callback<Duration>(decoder, 9)(decoder, encoded, encodedLen);
    if (duration >= 0 && duration * Callback<Channels>(decoder, 12)(decoder) * sizeof(std::int16_t) > maxDecodedBytes) return -1;
    return Callback<Decode>(decoder, 14)(decoder, encoded, encodedLen, sampleRateHz, decoded, speechType);
}

}
