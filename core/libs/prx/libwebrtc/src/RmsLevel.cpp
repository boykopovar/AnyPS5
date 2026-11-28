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
#include "prx/libc/include/GuestPtr.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int MinLevelDb = 127;
constexpr int InaudibleButNotMuted = 126;
constexpr float MaxSquaredLevel = 32768 * 32768;
constexpr float MinLevel = 1.995262314968883e-13f;
constexpr std::size_t ChunkSamples = 4096;

WebRtcRmsLevel ReadWritable(WebRtcRmsLevel* level, const char* function) {
    if (!level || !GuestWritable_nid_no_patch(level, sizeof(*level)))
        throw std::invalid_argument(std::string(function) + " requires a level");
    WebRtcRmsLevel host{};
    if (!GuestPtr<WebRtcRmsLevel>(level).CopyIn(host))
        throw std::invalid_argument(std::string(function) + " requires a level");
    return host;
}

void WriteLevel(WebRtcRmsLevel* level, const WebRtcRmsLevel& host, const char* function) {
    if (!GuestPtr<WebRtcRmsLevel>(level).CopyOut(host))
        throw std::invalid_argument(std::string(function) + " requires a level");
}

int ComputeRms(float meanSquare) {
    if (meanSquare <= MinLevel * MaxSquaredLevel)
        return MinLevelDb;
    const float meanSquareNorm = meanSquare / MaxSquaredLevel;
    const float rms = 10.f * std::log10(meanSquareNorm);
    return static_cast<int>(-rms + 0.5f);
}

void Reset(WebRtcRmsLevel& level) {
    level.sumSquare = 0.f;
    level.sampleCount = 0;
    level.maxSumSquare = 0.f;
    level.hasBlockSize = false;
}

void CheckBlockSize(WebRtcRmsLevel& level, std::size_t blockSize) {
    if (!level.hasBlockSize || level.blockSize != blockSize) {
        Reset(level);
        level.blockSize = blockSize;
        level.hasBlockSize = true;
    }
}

}

extern "C" {

void APS5_VABI _ZN6webrtc8RmsLevelC1Ev_nid_postfix(WebRtcRmsLevel* level) {
    auto host = ReadWritable(level, "RmsLevel::RmsLevel");
    Reset(host);
    WriteLevel(level, host, "RmsLevel::RmsLevel");
}

void APS5_VABI _ZN6webrtc8RmsLevelD1Ev_nid_postfix(WebRtcRmsLevel* level) {
    WebRtcRmsLevel host{};
    if (!GuestPtr<const WebRtcRmsLevel>(level).CopyIn(host))
        throw std::invalid_argument("RmsLevel::~RmsLevel requires a level");
}

void APS5_VABI _ZN6webrtc8RmsLevel7AnalyzeEN3rtc9ArrayViewIKsLln4711EEE_nid_postfix(WebRtcRmsLevel* level, const std::int16_t* data, std::size_t size) {
    auto rms = ReadWritable(level, "RmsLevel::Analyze");
    if (size == 0)
        return;
    if (!data)
        throw std::invalid_argument("RmsLevel::Analyze requires samples");
    std::array<std::int16_t, ChunkSamples> chunk{};
    float sumSquare = 0.f;
    for (std::size_t offset = 0; offset < size;) {
        const std::size_t batch = std::min(ChunkSamples, size - offset);
        if (!GuestBytes(data + offset).CopyIn(chunk.data(), batch * sizeof(std::int16_t)))
            throw std::invalid_argument("RmsLevel::Analyze requires readable samples");
        for (std::size_t index = 0; index < batch; ++index)
            sumSquare = sumSquare + chunk[index] * chunk[index];
        offset += batch;
    }
    CheckBlockSize(rms, size);
    rms.sumSquare += sumSquare;
    rms.sampleCount += size;
    rms.maxSumSquare = std::max(rms.maxSumSquare, sumSquare);
    WriteLevel(level, rms, "RmsLevel::Analyze");
}

int APS5_VABI _ZN6webrtc8RmsLevel7AverageEv_nid_postfix(WebRtcRmsLevel* level) {
    auto rms = ReadWritable(level, "RmsLevel::Average");
    const bool haveSamples = rms.sampleCount != 0;
    int result = haveSamples ? ComputeRms(rms.sumSquare / rms.sampleCount) : MinLevelDb;
    if (haveSamples && result == MinLevelDb && rms.sumSquare != 0.0f)
        result = InaudibleButNotMuted;
    Reset(rms);
    WriteLevel(level, rms, "RmsLevel::Average");
    return result;
}

}
