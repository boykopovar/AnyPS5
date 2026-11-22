/* Derived from common_audio/wav_file.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from common_audio/include/audio_util.h@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from rtc_base/system/file_wrapper.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

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
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr std::size_t MaxChunkSize = 4096;
constexpr std::int32_t FormatPcm = 1;
constexpr std::int32_t FormatIeeeFloat = 3;
constexpr int SeekSet = 0;

std::int16_t FloatToS16(float value) {
    value *= 32768.f;
    value = std::min(value, 32767.f);
    value = std::max(value, -32768.f);
    return static_cast<std::int16_t>(value + std::copysign(0.5f, value));
}

FileStream* Stream(WebRtcWavReader* reader, const char* function) {
    if (!reader || !reader->file.file)
        throw std::invalid_argument(function);
    return reader->file.file;
}

}

extern "C" {

void APS5_VABI _ZN6webrtc9WavReader5ResetEv_nid_postfix(WebRtcWavReader* reader) {
    FileStream* const stream = Stream(reader, "WavReader::Reset requires an open file");
    if (fseek_nid_postfix(stream, reader->dataStartPos, SeekSet) != 0)
        throw std::runtime_error("WavReader::Reset: Failed to set position in the file to WAV data start position");
    reader->numUnreadSamples = reader->numSamplesInFile;
}

std::size_t APS5_VABI _ZN6webrtc9WavReader11ReadSamplesEmPs_nid_postfix(WebRtcWavReader* reader, std::size_t numSamples, std::int16_t* samples) {
    std::size_t numSamplesLeftToRead = numSamples;
    std::size_t nextChunkStart = 0;
    while (numSamplesLeftToRead > 0 && reader->numUnreadSamples > 0) {
        FileStream* const stream = Stream(reader, "WavReader::ReadSamples requires an open file");
        if (!samples)
            throw std::invalid_argument("WavReader::ReadSamples requires a sample buffer");
        const std::size_t chunkSize = std::min(std::min(MaxChunkSize, numSamplesLeftToRead), reader->numUnreadSamples);
        std::size_t numBytesRead;
        std::size_t numSamplesRead;
        if (reader->format == FormatIeeeFloat) {
            std::array<float, MaxChunkSize> samplesToConvert;
            numBytesRead = fread_nid_postfix(samplesToConvert.data(), 1, chunkSize * sizeof(samplesToConvert[0]), stream);
            numSamplesRead = numBytesRead / sizeof(samplesToConvert[0]);
            for (std::size_t j = 0; j < numSamplesRead; ++j)
                samples[nextChunkStart + j] = FloatToS16(samplesToConvert[j]);
        } else {
            if (reader->format != FormatPcm)
                throw std::runtime_error("WavReader::ReadSamples: the WAV format is neither PCM nor IEEE float");
            numBytesRead = fread_nid_postfix(&samples[nextChunkStart], 1, chunkSize * sizeof(samples[0]), stream);
            numSamplesRead = numBytesRead / sizeof(samples[0]);
        }
        if (numSamplesRead != 0 && (numBytesRead % numSamplesRead) != 0)
            throw std::runtime_error("WavReader::ReadSamples: Corrupt file: file ended in the middle of a sample.");
        if (numSamplesRead != chunkSize && !feof_nid_postfix(stream))
            throw std::runtime_error("WavReader::ReadSamples: Corrupt file: payload size does not match header.");
        nextChunkStart += numSamplesRead;
        reader->numUnreadSamples -= numSamplesRead;
        numSamplesLeftToRead -= numSamplesRead;
    }
    return numSamples - numSamplesLeftToRead;
}

void APS5_VABI _ZN6webrtc9WavReader5CloseEv_nid_postfix(WebRtcWavReader* reader) {
    if (!reader)
        throw std::invalid_argument("WavReader::Close requires a reader");
    _ZN6webrtc11FileWrapper5CloseEv_nid_postfix(&reader->file);
}

}
