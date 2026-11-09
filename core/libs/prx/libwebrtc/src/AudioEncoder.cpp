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
#include <stdexcept>

namespace {

void ClearOptional(WebRtcOptionalUint32& optional) {
    optional.storage.nullState = 0;
    optional.engaged = false;
}

void ConstructAnaStats(WebRtcAnaStats& stats) {
    ClearOptional(stats.bitrateActionCounter);
    ClearOptional(stats.channelActionCounter);
    ClearOptional(stats.dtxActionCounter);
    ClearOptional(stats.fecActionCounter);
    ClearOptional(stats.frameLengthIncreaseCounter);
    ClearOptional(stats.frameLengthDecreaseCounter);
    stats.uplinkPacketLossFraction.storage.nullState = 0;
    stats.uplinkPacketLossFraction.engaged = false;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc8ANAStatsC1Ev_nid_postfix(WebRtcAnaStats* stats) {
    if (!stats)
        throw std::invalid_argument("ANAStats::ANAStats requires stats");
    ConstructAnaStats(*stats);
}

WebRtcAnaStats* APS5_VABI _ZNK6webrtc12AudioEncoder11GetANAStatsEv_nid_postfix(WebRtcAnaStats* result, const WebRtcAudioEncoder*) {
    if (!result)
        throw std::invalid_argument("AudioEncoder::GetANAStats requires a result");
    ConstructAnaStats(*result);
    return result;
}

bool APS5_VABI _ZNK6webrtc12AudioEncoder6GetDtxEv_nid_postfix(const WebRtcAudioEncoder*) {
    return false;
}

void APS5_VABI _ZN6webrtc12AudioEncoder16SetTargetBitrateEi_nid_postfix(WebRtcAudioEncoder*, int) {}

void APS5_VABI _ZN6webrtc12AudioEncoder26DisableAudioNetworkAdaptorEv_nid_postfix(WebRtcAudioEncoder*) {}

void APS5_VABI _ZN6webrtc12AudioEncoder13OnReceivedRttEi_nid_postfix(WebRtcAudioEncoder*, int) {}

void APS5_VABI _ZN6webrtc12AudioEncoder18OnReceivedOverheadEm_nid_postfix(WebRtcAudioEncoder*, std::size_t) {}

void APS5_VABI _ZN6webrtc12AudioEncoder27SetReceiverFrameLengthRangeEii_nid_postfix(WebRtcAudioEncoder*, int, int) {}

}
