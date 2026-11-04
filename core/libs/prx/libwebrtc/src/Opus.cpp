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

#include <cstddef>
#include <cstdint>
#include <opus.h>
#include <stdexcept>

struct WebRtcOpusEncInst {
    OpusEncoder* encoder;
    std::size_t channels;
    int inDtxMode;
    int sampleRateHz;
};

namespace {

constexpr std::size_t SamplesPerMillisecondAt48Khz = 48;
constexpr std::size_t MaxFrameMillisecondsWithout120Ms = 60;
constexpr std::size_t MaxFrameMillisecondsWith120Ms = 120;

}

extern "C" {

std::int16_t APS5_VABI WebRtcOpus_EncoderFree_nid_postfix(WebRtcOpusEncInst* inst) {
    if (!inst)
        return -1;
    if (inst->encoder)
        opus_encoder_destroy(inst->encoder);
    delete inst;
    return 0;
}

std::int16_t APS5_VABI WebRtcOpus_EncoderCreate_nid_postfix(WebRtcOpusEncInst** inst, std::size_t channels, std::int32_t application, int sampleRateHz) {
    if (!inst)
        return -1;
    int opusApplication = 0;
    switch (application) {
    case 0:
        opusApplication = OPUS_APPLICATION_VOIP;
        break;
    case 1:
        opusApplication = OPUS_APPLICATION_AUDIO;
        break;
    default:
        return -1;
    }
    auto* state = new WebRtcOpusEncInst {};
    int error = OPUS_OK;
    state->encoder = opus_encoder_create(sampleRateHz, static_cast<int>(channels), opusApplication, &error);
    if (error != OPUS_OK || !state->encoder) {
        WebRtcOpus_EncoderFree_nid_postfix(state);
        return -1;
    }
    state->inDtxMode = 0;
    state->channels = channels;
    state->sampleRateHz = sampleRateHz;
    *inst = state;
    return 0;
}

int APS5_VABI WebRtcOpus_Encode_nid_postfix(WebRtcOpusEncInst* inst, const std::int16_t* audioIn, std::size_t samples, std::size_t lengthEncodedBuffer, std::uint8_t* encoded) {
    if (!inst)
        throw std::invalid_argument("WebRtcOpus_Encode requires an encoder");
    if (samples > SamplesPerMillisecondAt48Khz * MaxFrameMillisecondsWith120Ms)
        return -1;
    if (samples > SamplesPerMillisecondAt48Khz * MaxFrameMillisecondsWithout120Ms)
        throw std::runtime_error("WebRtcOpus_Encode: frames over 60 ms depend on WEBRTC_OPUS_SUPPORT_120MS_PTIME, which is unknown for this module");
    if (!audioIn || !encoded)
        throw std::invalid_argument("WebRtcOpus_Encode requires input and output buffers");
    const int result = opus_encode(inst->encoder, audioIn, static_cast<int>(samples), encoded, static_cast<opus_int32>(lengthEncodedBuffer));
    if (result <= 0)
        return -1;
    if (result <= 2) {
        if (inst->inDtxMode)
            return 0;
        inst->inDtxMode = 1;
        return result;
    }
    inst->inDtxMode = 0;
    return result;
}

std::int16_t APS5_VABI WebRtcOpus_SetBitRate_nid_postfix(WebRtcOpusEncInst* inst, std::int32_t rate) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_BITRATE(rate)));
}

std::int16_t APS5_VABI WebRtcOpus_SetPacketLossRate_nid_postfix(WebRtcOpusEncInst* inst, std::int32_t lossRate) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_PACKET_LOSS_PERC(lossRate)));
}

std::int16_t APS5_VABI WebRtcOpus_SetMaxPlaybackRate_nid_postfix(WebRtcOpusEncInst* inst, std::int32_t frequencyHz) {
    if (!inst)
        return -1;
    opus_int32 bandwidth = OPUS_BANDWIDTH_FULLBAND;
    if (frequencyHz <= 8000)
        bandwidth = OPUS_BANDWIDTH_NARROWBAND;
    else if (frequencyHz <= 12000)
        bandwidth = OPUS_BANDWIDTH_MEDIUMBAND;
    else if (frequencyHz <= 16000)
        bandwidth = OPUS_BANDWIDTH_WIDEBAND;
    else if (frequencyHz <= 24000)
        bandwidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_MAX_BANDWIDTH(bandwidth)));
}

std::int16_t APS5_VABI WebRtcOpus_EnableFec_nid_postfix(WebRtcOpusEncInst* inst) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_INBAND_FEC(1)));
}

std::int16_t APS5_VABI WebRtcOpus_DisableFec_nid_postfix(WebRtcOpusEncInst* inst) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_INBAND_FEC(0)));
}

std::int16_t APS5_VABI WebRtcOpus_EnableDtx_nid_postfix(WebRtcOpusEncInst* inst) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_DTX(1)));
}

std::int16_t APS5_VABI WebRtcOpus_DisableDtx_nid_postfix(WebRtcOpusEncInst* inst) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_DTX(0)));
}

std::int16_t APS5_VABI WebRtcOpus_EnableCbr_nid_postfix(WebRtcOpusEncInst* inst) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_VBR(0)));
}

std::int16_t APS5_VABI WebRtcOpus_DisableCbr_nid_postfix(WebRtcOpusEncInst* inst) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_VBR(1)));
}

std::int16_t APS5_VABI WebRtcOpus_SetComplexity_nid_postfix(WebRtcOpusEncInst* inst, std::int32_t complexity) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_COMPLEXITY(complexity)));
}

std::int32_t APS5_VABI WebRtcOpus_GetBandwidth_nid_postfix(WebRtcOpusEncInst* inst) {
    if (!inst)
        return -1;
    opus_int32 bandwidth = 0;
    if (opus_encoder_ctl(inst->encoder, OPUS_GET_BANDWIDTH(&bandwidth)) == OPUS_OK)
        return bandwidth;
    return -1;
}

std::int16_t APS5_VABI WebRtcOpus_SetBandwidth_nid_postfix(WebRtcOpusEncInst* inst, std::int32_t bandwidth) {
    if (!inst)
        return -1;
    return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_BANDWIDTH(bandwidth)));
}

std::int16_t APS5_VABI WebRtcOpus_SetForceChannels_nid_postfix(WebRtcOpusEncInst* inst, std::size_t numChannels) {
    if (!inst)
        return -1;
    if (numChannels == 0)
        return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_FORCE_CHANNELS(OPUS_AUTO)));
    if (numChannels == 1 || numChannels == 2)
        return static_cast<std::int16_t>(opus_encoder_ctl(inst->encoder, OPUS_SET_FORCE_CHANNELS(static_cast<opus_int32>(numChannels))));
    return -1;
}

}
