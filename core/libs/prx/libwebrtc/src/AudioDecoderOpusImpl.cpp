/* Derived from modules/audio_coding/codecs/opus/audio_decoder_opus.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* Derived from modules/audio_coding/codecs/opus/opus_interface.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libc/GuestStringStorage.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <opus.h>
#include <stdexcept>
#include <vector>

struct WebRtcOpusDecInst {
    OpusDecoder* decoder;
    std::size_t channels;
    int sampleRateHz;
    int inDtxMode;
    int lastPacketNumChannels;
};

namespace {

constexpr std::size_t FieldTrialsLookupSlot = 2;
constexpr std::size_t PacketDurationSlot = 8;
constexpr std::size_t PacketDurationRedundantSlot = 9;
constexpr std::size_t SpeechSpeech = 1;
constexpr std::size_t SpeechComfortNoise = 2;
constexpr int PlcFrameSizeMilliseconds = 20;
constexpr int MaxPacketMilliseconds = 120;
constexpr std::uint64_t GuestStringLargeAllocation = 0x1000;
constexpr char GeneratePlcFieldTrial[] = "WebRTC-Audio-OpusGeneratePlc";
constexpr char EnabledPrefix[] = "Enabled";

struct WebRtcDecodeResult {
    std::size_t numDecodedSamples;
    std::int32_t speechType;
};

struct WebRtcDecodeResultOptional {
    WebRtcDecodeResult value;
    bool engaged;
};

static_assert(sizeof(WebRtcDecodeResult) == 16);
static_assert(sizeof(WebRtcDecodeResultOptional) == 24);
static_assert(offsetof(WebRtcDecodeResultOptional, engaged) == 16);

struct WebRtcAudioInt16Buffer {
    std::uint64_t size;
    std::uint64_t capacity;
    std::int16_t* data;
};

static_assert(sizeof(WebRtcAudioInt16Buffer) == 24);
static_assert(sizeof(WebRtcAudioInt16Buffer) == sizeof(WebRtcBuffer));

struct WebRtcParseResultVector {
    WebRtcAudioDecoderParseResult* begin;
    WebRtcAudioDecoderParseResult* end;
    WebRtcAudioDecoderParseResult* capacityEnd;
};

struct WebRtcOpusFrame {
    void* const* vtable;
    WebRtcAudioDecoder* decoder;
    WebRtcBuffer payload;
    bool isPrimaryPayload;
};

template<typename T>
T ReadGuest(const T* address, const char* message) {
    T value{};
    if (!GuestPtr<const T>(address).CopyIn(value))
        throw std::invalid_argument(message);
    return value;
}

template<typename T>
void WriteGuest(T* address, const T& value, const char* message) {
    if (!GuestPtr<T>(address).CopyOut(value))
        throw std::invalid_argument(message);
}

void* ReadVtableSlot(const void* const* vtable, std::size_t slot, const char* message) {
    if (!vtable)
        throw std::invalid_argument(message);
    void* fn = const_cast<void*>(ReadGuest(vtable + slot, message));
    if (!fn)
        throw std::invalid_argument(message);
    return fn;
}

void ReleaseGuestString(const WebRtcGuestString& string) {
    if (string.capacity <= WebRtcGuestStringInlineCapacity)
        return;
    void* storage = string.pointer;
    if (string.capacity + 1 >= GuestStringLargeAllocation)
        storage = ReadGuest(static_cast<void* const*>(storage) - 1, "AudioDecoderOpusImpl invalid field trial string header");
    _ZdlPv_nid_postfix(storage);
}

bool IsGeneratePlcEnabled(const WebRtcFieldTrialsView* fieldTrials) {
    const auto hostTrials = ReadGuest(fieldTrials, "AudioDecoderOpusImpl requires a FieldTrialsView");
    using LookupFn = WebRtcGuestString*(APS5_VABI*)(WebRtcGuestString*, const WebRtcFieldTrialsView*, const char*, std::size_t);
    auto* lookup = reinterpret_cast<LookupFn>(ReadVtableSlot(hostTrials.vtable, FieldTrialsLookupSlot, "AudioDecoderOpusImpl requires FieldTrialsView::Lookup"));
    WebRtcGuestString result{};
    lookup(&result, fieldTrials, GeneratePlcFieldTrial, sizeof(GeneratePlcFieldTrial) - 1);
    const std::size_t prefixLength = sizeof(EnabledPrefix) - 1;
    bool enabled = false;
    if (result.size >= prefixLength) {
        std::array<char, prefixLength> prefix{};
        if (result.capacity <= WebRtcGuestStringInlineCapacity) {
            std::memcpy(prefix.data(), result.buffer, prefixLength);
            enabled = std::memcmp(prefix.data(), EnabledPrefix, prefixLength) == 0;
        } else if (GuestBytes(result.pointer).CopyIn(prefix.data(), prefixLength)) {
            enabled = std::memcmp(prefix.data(), EnabledPrefix, prefixLength) == 0;
        } else {
            ReleaseGuestString(result);
            throw std::invalid_argument("AudioDecoderOpusImpl requires a readable field trial string");
        }
    }
    ReleaseGuestString(result);
    return enabled;
}

int FrameSizePerChannel(int frameSizeMilliseconds, int sampleRateHz) {
    return (sampleRateHz / 1000) * frameSizeMilliseconds;
}

int MaxFrameSizePerChannel(int sampleRateHz) {
    return FrameSizePerChannel(MaxPacketMilliseconds, sampleRateHz);
}

int PlcDuration(const WebRtcOpusDecInst* inst) {
    return FrameSizePerChannel(PlcFrameSizeMilliseconds, inst->sampleRateHz);
}

std::int16_t DetermineAudioType(WebRtcOpusDecInst* inst, std::size_t encodedBytes) {
    if (encodedBytes == 0 && inst->inDtxMode)
        return 2;
    if (encodedBytes == 1 || encodedBytes == 2) {
        inst->inDtxMode = 1;
        return 2;
    }
    inst->inDtxMode = 0;
    return 0;
}

std::int32_t ConvertSpeechType(std::int16_t type) {
    if (type == 2)
        return static_cast<std::int32_t>(SpeechComfortNoise);
    if (type == 0 || type == 1)
        return static_cast<std::int32_t>(SpeechSpeech);
    throw std::invalid_argument("AudioDecoder::ConvertSpeechType invalid speech type");
}

int DecodeNative(WebRtcOpusDecInst* inst, const std::uint8_t* encoded, std::size_t encodedBytes, int frameSize, std::int16_t* decoded, std::int16_t* audioType, int decodeFec) {
    const int res = opus_decode(inst->decoder, encoded, static_cast<opus_int32>(encodedBytes), reinterpret_cast<opus_int16*>(decoded), frameSize, decodeFec);
    if (res <= 0)
        return -1;
    *audioType = DetermineAudioType(inst, encodedBytes);
    return res;
}

int OpusDecode(WebRtcOpusDecInst* inst, const std::uint8_t* encoded, std::size_t encodedBytes, int expectedDuration, std::int16_t* decoded, std::int16_t* audioType) {
    int decodedSamplesPerChannel;
    if (encodedBytes == 0) {
        *audioType = DetermineAudioType(inst, encodedBytes);
        std::int16_t plcAudioType = 0;
        decodedSamplesPerChannel = DecodeNative(inst, nullptr, 0, expectedDuration, decoded, &plcAudioType, 0);
    } else {
        decodedSamplesPerChannel = DecodeNative(inst, encoded, encodedBytes, expectedDuration, decoded, audioType, 0);
        inst->lastPacketNumChannels = opus_packet_get_nb_channels(encoded);
    }
    if (decodedSamplesPerChannel < 0 || decodedSamplesPerChannel > expectedDuration)
        return -1;
    if (inst->channels == 2 && inst->lastPacketNumChannels == 1) {
        for (int i = 0; i < (decodedSamplesPerChannel << 1); i += 2)
            decoded[i + 1] = decoded[i];
    }
    return decodedSamplesPerChannel;
}

int NumSilkFrames(const std::uint8_t* payload) {
    int payloadLengthMs = opus_packet_get_samples_per_frame(payload, 48000) / 48;
    if (payloadLengthMs < 10)
        payloadLengthMs = 10;
    switch (payloadLengthMs) {
    case 10:
    case 20:
        return 1;
    case 40:
        return 2;
    case 60:
        return 3;
    default:
        return 0;
    }
}

bool PacketHasFecBytes(const std::uint8_t* payload, std::size_t payloadLengthBytes) {
    if (!payload || payloadLengthBytes == 0 || payloadLengthBytes > static_cast<std::size_t>(std::numeric_limits<opus_int32>::max()))
        return false;
    if (payload[0] & 0x80)
        return false;
    const int silkFrames = NumSilkFrames(payload);
    if (silkFrames == 0)
        return false;
    const int channels = opus_packet_get_nb_channels(payload);
    if (channels != 1 && channels != 2)
        return false;
    opus_int16 frameSizes[48];
    const unsigned char* frameData[48];
    if (opus_packet_parse(payload, static_cast<opus_int32>(payloadLengthBytes), nullptr, frameData, frameSizes, nullptr) < 0)
        return false;
    if (frameSizes[0] < 1)
        return false;
    for (int n = 0; n < channels; ++n) {
        if (frameData[0][0] & (0x80 >> ((n + 1) * (silkFrames + 1) - 1)))
            return true;
    }
    return false;
}

std::vector<std::uint8_t> CopyEncodedBytes(const std::uint8_t* encoded, std::size_t encodedLen) {
    if (encodedLen == 0)
        return {};
    if (!encoded)
        throw std::invalid_argument("AudioDecoderOpusImpl requires an encoded buffer");
    std::vector<std::uint8_t> bytes(encodedLen);
    if (!GuestBytes(encoded).CopyIn(bytes.data(), bytes.size()))
        throw std::invalid_argument("AudioDecoderOpusImpl requires a readable encoded buffer");
    return bytes;
}

int DecodeRedundantBytes(WebRtcOpusDecInst* inst, const std::uint8_t* encoded, std::size_t encodedBytes, int expectedDuration, std::int16_t* decoded, std::int16_t* audioType) {
    if (!PacketHasFecBytes(encoded, encodedBytes))
        return 0;
    const int decodedSamples = DecodeNative(inst, encoded, encodedBytes, expectedDuration, decoded, audioType, 1);
    return (decodedSamples < 0 || decodedSamples > expectedDuration) ? -1 : decodedSamples;
}

int DurationEstBytes(const WebRtcOpusDecInst* inst, const std::uint8_t* payload, std::size_t payloadLengthBytes) {
    if (payloadLengthBytes == 0)
        return PlcDuration(inst);
    if (payloadLengthBytes > static_cast<std::size_t>(std::numeric_limits<opus_int32>::max()))
        return 0;
    const int frames = opus_packet_get_nb_frames(payload, static_cast<opus_int32>(payloadLengthBytes));
    if (frames < 0)
        return 0;
    const int samples = frames * opus_packet_get_samples_per_frame(payload, inst->sampleRateHz);
    if (samples > MaxPacketMilliseconds * inst->sampleRateHz / 1000)
        return 0;
    return samples;
}

int FecDurationEstBytes(const std::uint8_t* payload, std::size_t payloadLengthBytes, int sampleRateHz) {
    if (!PacketHasFecBytes(payload, payloadLengthBytes))
        return 0;
    const int samples = opus_packet_get_samples_per_frame(payload, sampleRateHz);
    const int samplesPerMs = sampleRateHz / 1000;
    if (samples < 10 * samplesPerMs || samples > MaxPacketMilliseconds * samplesPerMs)
        return 0;
    return samples;
}

WebRtcOpusDecInst* RequireDecoderState(const WebRtcAudioDecoderOpusImpl* decoder) {
    const auto host = ReadGuest(decoder, "AudioDecoderOpusImpl requires a decoder");
    if (!host.decState)
        throw std::invalid_argument("AudioDecoderOpusImpl has no decoder state");
    return host.decState;
}

void APS5_VABI OpusFrameD1_nid_no_patch(WebRtcOpusFrame* frame) {
    const auto host = ReadGuest(frame, "OpusFrame destructor requires a frame");
    if (host.payload.data)
        _ZdaPv_nid_postfix(host.payload.data);
}

void APS5_VABI OpusFrameD0_nid_no_patch(WebRtcOpusFrame* frame) {
    OpusFrameD1_nid_no_patch(frame);
    _ZdlPv_nid_postfix(frame);
}

std::size_t APS5_VABI OpusFrameDuration_nid_no_patch(const WebRtcOpusFrame* frame) {
    const auto hostFrame = ReadGuest(frame, "OpusFrame::Duration requires a frame");
    const auto* vtable = ReadGuest(reinterpret_cast<void* const* const*>(hostFrame.decoder), "OpusFrame::Duration requires a decoder");
    const std::size_t slot = hostFrame.isPrimaryPayload ? PacketDurationSlot : PacketDurationRedundantSlot;
    using DurationFn = int(APS5_VABI*)(const WebRtcAudioDecoder*, const std::uint8_t*, std::size_t);
    auto* durationFn = reinterpret_cast<DurationFn>(ReadVtableSlot(vtable, slot, "OpusFrame::Duration requires PacketDuration"));
    const int ret = durationFn(hostFrame.decoder, hostFrame.payload.data, hostFrame.payload.size);
    return ret < 0 ? 0 : static_cast<std::size_t>(ret);
}

bool APS5_VABI OpusFrameIsDtxPacket_nid_no_patch(const WebRtcOpusFrame* frame) {
    const auto hostFrame = ReadGuest(frame, "OpusFrame::IsDtxPacket requires a frame");
    return hostFrame.payload.size <= 2;
}

WebRtcDecodeResultOptional* APS5_VABI OpusFrameDecode_nid_no_patch(WebRtcDecodeResultOptional* result, const WebRtcOpusFrame* frame, std::int16_t* decoded, std::size_t decodedSamples) {
    if (!result || !GuestWritable_nid_no_patch(result, sizeof(*result)))
        throw std::invalid_argument("OpusFrame::Decode requires a writable result");
    const auto hostFrame = ReadGuest(frame, "OpusFrame::Decode requires a frame");
    const auto* vtable = ReadGuest(reinterpret_cast<void* const* const*>(hostFrame.decoder), "OpusFrame::Decode requires a decoder");
    using SampleRateFn = int(APS5_VABI*)(const WebRtcAudioDecoder*);
    auto* sampleRateFn = reinterpret_cast<SampleRateFn>(ReadVtableSlot(vtable, 11, "OpusFrame::Decode requires SampleRateHz"));
    const int sampleRateHz = sampleRateFn(hostFrame.decoder);
    std::int32_t speechType = static_cast<std::int32_t>(SpeechSpeech);
    WebRtcDecodeResultOptional out{};
    if (decodedSamples <= std::numeric_limits<std::size_t>::max() / sizeof(std::int16_t)) {
        const std::size_t maxDecodedBytes = decodedSamples * sizeof(std::int16_t);
        const int ret = hostFrame.isPrimaryPayload
            ? _ZN6webrtc12AudioDecoder6DecodeEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(hostFrame.decoder, hostFrame.payload.data, hostFrame.payload.size, sampleRateHz, maxDecodedBytes, decoded, &speechType)
            : _ZN6webrtc12AudioDecoder15DecodeRedundantEPKhmimPsPNS0_10SpeechTypeE_nid_postfix(hostFrame.decoder, hostFrame.payload.data, hostFrame.payload.size, sampleRateHz, maxDecodedBytes, decoded, &speechType);
        if (ret >= 0 && static_cast<std::size_t>(ret) <= decodedSamples) {
            out.value.numDecodedSamples = static_cast<std::size_t>(ret);
            out.value.speechType = speechType;
            out.engaged = true;
        }
    }
    WriteGuest(result, out, "OpusFrame::Decode requires a writable result");
    return result;
}

void* const OpusFrameVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&OpusFrameD1_nid_no_patch),
    reinterpret_cast<void*>(&OpusFrameD0_nid_no_patch),
    reinterpret_cast<void*>(&OpusFrameDuration_nid_no_patch),
    reinterpret_cast<void*>(&OpusFrameIsDtxPacket_nid_no_patch),
    reinterpret_cast<void*>(&OpusFrameDecode_nid_no_patch),
};

WebRtcOpusFrame* CreateOpusFrame(WebRtcAudioDecoder* decoder, const WebRtcBuffer& payload, bool isPrimaryPayload) {
    auto* frame = static_cast<WebRtcOpusFrame*>(_Znwm_nid_postfix(sizeof(WebRtcOpusFrame)));
    WebRtcOpusFrame host{};
    host.vtable = OpusFrameVtable + 2;
    host.decoder = decoder;
    host.payload = payload;
    host.isPrimaryPayload = isPrimaryPayload;
    if (!GuestPtr<WebRtcOpusFrame>(frame).CopyOut(host)) {
        _ZdlPv_nid_postfix(frame);
        throw std::invalid_argument("OpusFrame allocation is not writable");
    }
    return frame;
}

int APS5_VABI AudioDecoderOpusPacketDuration_nid_no_patch(const WebRtcAudioDecoderOpusImpl* decoder, const std::uint8_t* encoded, std::size_t encodedLen) {
    const auto* inst = RequireDecoderState(decoder);
    const auto bytes = CopyEncodedBytes(encoded, encodedLen);
    return DurationEstBytes(inst, bytes.data(), bytes.size());
}

bool APS5_VABI AudioDecoderOpusPacketHasFec_nid_no_patch(const WebRtcAudioDecoderOpusImpl*, const std::uint8_t* encoded, std::size_t encodedLen) {
    if (!encoded || encodedLen == 0)
        return false;
    const auto bytes = CopyEncodedBytes(encoded, encodedLen);
    return PacketHasFecBytes(bytes.data(), bytes.size());
}

int APS5_VABI AudioDecoderOpusPacketDurationRedundant_nid_no_patch(const WebRtcAudioDecoderOpusImpl* decoder, const std::uint8_t* encoded, std::size_t encodedLen) {
    const auto host = ReadGuest(decoder, "AudioDecoderOpusImpl requires a decoder");
    if (!host.decState)
        throw std::invalid_argument("AudioDecoderOpusImpl has no decoder state");
    if (!encoded || encodedLen == 0)
        return DurationEstBytes(host.decState, nullptr, 0);
    const auto bytes = CopyEncodedBytes(encoded, encodedLen);
    if (!PacketHasFecBytes(bytes.data(), bytes.size()))
        return DurationEstBytes(host.decState, bytes.data(), bytes.size());
    return FecDurationEstBytes(bytes.data(), bytes.size(), host.sampleRateHz);
}

WebRtcParseResultVector* APS5_VABI AudioDecoderOpusParsePayload_nid_no_patch(WebRtcParseResultVector* result, WebRtcAudioDecoderOpusImpl* decoder, WebRtcBuffer* payload, std::uint32_t timestamp) {
    if (!result || !GuestWritable_nid_no_patch(result, sizeof(*result)))
        throw std::invalid_argument("AudioDecoderOpusImpl::ParsePayload requires a writable result");
    if (!payload || !GuestWritable_nid_no_patch(payload, sizeof(*payload)))
        throw std::invalid_argument("AudioDecoderOpusImpl::ParsePayload requires a writable payload");
    const auto hostDecoder = ReadGuest(decoder, "AudioDecoderOpusImpl::ParsePayload requires a decoder");
    if (!hostDecoder.decState)
        throw std::invalid_argument("AudioDecoderOpusImpl has no decoder state");
    const auto hostPayload = ReadGuest(payload, "AudioDecoderOpusImpl::ParsePayload requires a payload");
    if (hostPayload.size > hostPayload.capacity || (hostPayload.size != 0 && !hostPayload.data))
        throw std::invalid_argument("AudioDecoderOpusImpl::ParsePayload received an inconsistent buffer");
    const auto payloadBytes = CopyEncodedBytes(hostPayload.data, hostPayload.size);
    const bool hasFec = !payloadBytes.empty() && PacketHasFecBytes(payloadBytes.data(), payloadBytes.size());
    std::array<WebRtcAudioDecoderParseResult, 2> results{};
    std::size_t count = 0;
    std::uint8_t* fecData = nullptr;
    WebRtcOpusFrame* fecFrame = nullptr;
    WebRtcOpusFrame* primaryFrame = nullptr;
    WebRtcAudioDecoderParseResult* storage = nullptr;
    try {
        if (hasFec) {
            const auto* vtable = hostDecoder.vtable;
            using DurationFn = int(APS5_VABI*)(const WebRtcAudioDecoderOpusImpl*, const std::uint8_t*, std::size_t);
            auto* redundantDurationFn = reinterpret_cast<DurationFn>(ReadVtableSlot(vtable, PacketDurationRedundantSlot, "AudioDecoderOpusImpl::ParsePayload requires PacketDurationRedundant"));
            const int duration = redundantDurationFn(decoder, hostPayload.data, hostPayload.size);
            if (duration < 0)
                throw std::runtime_error("AudioDecoderOpusImpl::ParsePayload redundant duration cannot be negative");
            fecData = static_cast<std::uint8_t*>(_Znam_nid_postfix(payloadBytes.size()));
            if (!GuestBytes(fecData).CopyOut(payloadBytes.data(), payloadBytes.size()))
                throw std::invalid_argument("AudioDecoderOpusImpl::ParsePayload failed to copy FEC payload");
            const WebRtcBuffer fecBuffer{payloadBytes.size(), payloadBytes.size(), fecData};
            fecFrame = CreateOpusFrame(reinterpret_cast<WebRtcAudioDecoder*>(decoder), fecBuffer, false);
            fecData = nullptr;
            results[count++] = WebRtcAudioDecoderParseResult{timestamp - static_cast<std::uint32_t>(duration), 1, fecFrame};
        }
        primaryFrame = CreateOpusFrame(reinterpret_cast<WebRtcAudioDecoder*>(decoder), hostPayload, true);
        results[count++] = WebRtcAudioDecoderParseResult{timestamp, 0, primaryFrame};
        storage = static_cast<WebRtcAudioDecoderParseResult*>(_Znwm_nid_postfix(count * sizeof(WebRtcAudioDecoderParseResult)));
        if (!GuestPtr<WebRtcAudioDecoderParseResult>(storage).CopyOut(results.data(), count))
            throw std::invalid_argument("AudioDecoderOpusImpl::ParsePayload storage is not writable");
        const WebRtcParseResultVector hostResult{storage, storage + count, storage + count};
        WriteGuest(result, hostResult, "AudioDecoderOpusImpl::ParsePayload requires a writable result");
        WriteGuest(payload, WebRtcBuffer{}, "AudioDecoderOpusImpl::ParsePayload requires a writable payload");
        return result;
    } catch (...) {
        if (storage)
            _ZdlPv_nid_postfix(storage);
        if (primaryFrame)
            _ZdlPv_nid_postfix(primaryFrame);
        if (fecFrame)
            OpusFrameD0_nid_no_patch(fecFrame);
        else if (fecData)
            _ZdaPv_nid_postfix(fecData);
        throw;
    }
}

int DecodeInternalImpl(WebRtcAudioDecoderOpusImpl* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz, std::int16_t* decoded, std::int32_t* speechType, bool redundant) {
    const auto host = ReadGuest(decoder, "AudioDecoderOpusImpl::Decode requires a decoder");
    if (!host.decState)
        throw std::invalid_argument("AudioDecoderOpusImpl has no decoder state");
    if (sampleRateHz != host.sampleRateHz)
        throw std::invalid_argument("AudioDecoderOpusImpl sample rate mismatch");
    if (!decoded || !speechType || !GuestWritable_nid_no_patch(speechType, sizeof(*speechType)))
        throw std::invalid_argument("AudioDecoderOpusImpl::Decode requires writable output pointers");
    const auto bytes = CopyEncodedBytes(encoded, encodedLen);
    if (redundant && (bytes.empty() || !PacketHasFecBytes(bytes.data(), bytes.size()))) {
        WriteGuest(speechType, ConvertSpeechType(1), "AudioDecoderOpusImpl::Decode requires a writable speech type");
        return 0;
    }
    const int maxSamplesPerChannel = MaxFrameSizePerChannel(host.sampleRateHz);
    const int expectedDuration = redundant
        ? FecDurationEstBytes(bytes.data(), bytes.size(), host.sampleRateHz)
        : DurationEstBytes(host.decState, bytes.empty() ? nullptr : bytes.data(), bytes.size());
    if (expectedDuration <= 0 || expectedDuration > maxSamplesPerChannel) {
        WriteGuest(speechType, ConvertSpeechType(1), "AudioDecoderOpusImpl::Decode requires a writable speech type");
        return -1;
    }
    const std::size_t maxTotalSamples = static_cast<std::size_t>(expectedDuration) * host.channels;
    if (!GuestWritable_nid_no_patch(decoded, maxTotalSamples * sizeof(std::int16_t)))
        throw std::invalid_argument("AudioDecoderOpusImpl::Decode requires a writable decoded buffer");
    std::vector<std::int16_t> hostDecoded(maxTotalSamples);
    std::int16_t tempType = 1;
    int ret = redundant
        ? DecodeRedundantBytes(host.decState, bytes.data(), bytes.size(), expectedDuration, hostDecoded.data(), &tempType)
        : OpusDecode(host.decState, bytes.empty() ? nullptr : bytes.data(), bytes.size(), expectedDuration, hostDecoded.data(), &tempType);
    if (ret > 0) {
        if (ret > expectedDuration) {
            WriteGuest(speechType, ConvertSpeechType(1), "AudioDecoderOpusImpl::Decode requires a writable speech type");
            return -1;
        }
        ret *= static_cast<int>(host.channels);
        if (!GuestBytes(decoded).CopyOut(hostDecoded.data(), static_cast<std::size_t>(ret) * sizeof(std::int16_t)))
            throw std::invalid_argument("AudioDecoderOpusImpl::Decode requires a writable decoded buffer");
    }
    WriteGuest(speechType, ConvertSpeechType(tempType), "AudioDecoderOpusImpl::Decode requires a writable speech type");
    return ret;
}

int APS5_VABI AudioDecoderOpusDecodeInternal_nid_no_patch(WebRtcAudioDecoderOpusImpl* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz, std::int16_t* decoded, std::int32_t* speechType) {
    return DecodeInternalImpl(decoder, encoded, encodedLen, sampleRateHz, decoded, speechType, false);
}

int APS5_VABI AudioDecoderOpusDecodeRedundantInternal_nid_no_patch(WebRtcAudioDecoderOpusImpl* decoder, const std::uint8_t* encoded, std::size_t encodedLen, int sampleRateHz, std::int16_t* decoded, std::int32_t* speechType) {
    const auto bytes = CopyEncodedBytes(encoded, encodedLen);
    if (!PacketHasFecBytes(bytes.data(), bytes.size()))
        return DecodeInternalImpl(decoder, encoded, encodedLen, sampleRateHz, decoded, speechType, false);
    return DecodeInternalImpl(decoder, encoded, encodedLen, sampleRateHz, decoded, speechType, true);
}

void APS5_VABI AudioDecoderOpusReset_nid_no_patch(WebRtcAudioDecoderOpusImpl* decoder) {
    auto* inst = RequireDecoderState(decoder);
    opus_decoder_ctl(inst->decoder, OPUS_RESET_STATE);
    inst->inDtxMode = 0;
}

int APS5_VABI AudioDecoderOpusSampleRateHz_nid_no_patch(const WebRtcAudioDecoderOpusImpl* decoder) {
    return ReadGuest(decoder, "AudioDecoderOpusImpl::SampleRateHz requires a decoder").sampleRateHz;
}

std::size_t APS5_VABI AudioDecoderOpusChannels_nid_no_patch(const WebRtcAudioDecoderOpusImpl* decoder) {
    return ReadGuest(decoder, "AudioDecoderOpusImpl::Channels requires a decoder").channels;
}

void APS5_VABI AudioDecoderOpusGeneratePlc_nid_no_patch(WebRtcAudioDecoderOpusImpl* decoder, std::size_t, WebRtcInt16Buffer* concealmentAudio) {
    const auto host = ReadGuest(decoder, "AudioDecoderOpusImpl::GeneratePlc requires a decoder");
    if (!host.decState)
        throw std::invalid_argument("AudioDecoderOpusImpl has no decoder state");
    if (!host.generatePlc)
        return;
    auto* guestBuffer = reinterpret_cast<WebRtcAudioInt16Buffer*>(concealmentAudio);
    if (!guestBuffer || !GuestWritable_nid_no_patch(guestBuffer, sizeof(*guestBuffer)))
        throw std::invalid_argument("AudioDecoderOpusImpl::GeneratePlc requires a writable concealment buffer");
    auto hostBuffer = ReadGuest(guestBuffer, "AudioDecoderOpusImpl::GeneratePlc requires a concealment buffer");
    if (hostBuffer.size > hostBuffer.capacity || (hostBuffer.capacity != 0 && !hostBuffer.data))
        throw std::invalid_argument("AudioDecoderOpusImpl::GeneratePlc received an inconsistent buffer");
    const int plcPerChannel = PlcDuration(host.decState);
    const std::size_t plcSize = static_cast<std::size_t>(plcPerChannel) * host.channels;
    std::vector<std::int16_t> plcSamples(plcSize);
    std::int16_t tempType = 1;
    const int ret = OpusDecode(host.decState, nullptr, 0, plcPerChannel, plcSamples.data(), &tempType);
    const std::size_t appended = ret < 0 ? 0 : std::min(static_cast<std::size_t>(ret) * host.channels, plcSamples.size());
    if (appended == 0)
        return;
    const std::size_t newSize = hostBuffer.size + appended;
    if (newSize > hostBuffer.capacity) {
        const std::size_t newCapacity = std::max(newSize, hostBuffer.capacity + hostBuffer.capacity / 2);
        auto* grown = static_cast<std::int16_t*>(_Znam_nid_postfix(newCapacity * sizeof(std::int16_t)));
        if (hostBuffer.size != 0) {
            std::vector<std::int16_t> existing(hostBuffer.size);
            if (!GuestBytes(hostBuffer.data).CopyIn(existing.data(), existing.size() * sizeof(std::int16_t))) {
                _ZdaPv_nid_postfix(grown);
                throw std::invalid_argument("AudioDecoderOpusImpl::GeneratePlc failed to read existing buffer");
            }
            if (!GuestBytes(grown).CopyOut(existing.data(), existing.size() * sizeof(std::int16_t))) {
                _ZdaPv_nid_postfix(grown);
                throw std::invalid_argument("AudioDecoderOpusImpl::GeneratePlc failed to write grown buffer");
            }
        }
        if (!GuestBytes(grown + hostBuffer.size).CopyOut(plcSamples.data(), appended * sizeof(std::int16_t))) {
            _ZdaPv_nid_postfix(grown);
            throw std::invalid_argument("AudioDecoderOpusImpl::GeneratePlc failed to append samples");
        }
        if (hostBuffer.data)
            _ZdaPv_nid_postfix(hostBuffer.data);
        hostBuffer.data = grown;
        hostBuffer.capacity = newCapacity;
    } else {
        if (!GuestBytes(hostBuffer.data + hostBuffer.size).CopyOut(plcSamples.data(), appended * sizeof(std::int16_t)))
            throw std::invalid_argument("AudioDecoderOpusImpl::GeneratePlc requires a writable concealment data pointer");
    }
    hostBuffer.size = newSize;
    WriteGuest(guestBuffer, hostBuffer, "AudioDecoderOpusImpl::GeneratePlc requires a writable concealment buffer");
}

void APS5_VABI DeleteAudioDecoderOpus_nid_no_patch(WebRtcAudioDecoderOpusImpl* decoder) {
    _ZN6webrtc20AudioDecoderOpusImplD1Ev_nid_postfix(decoder);
    _ZdlPv_nid_postfix(decoder);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN6webrtc20AudioDecoderOpusImplD1Ev_nid_postfix),
    reinterpret_cast<void*>(&DeleteAudioDecoderOpus_nid_no_patch),
    reinterpret_cast<void*>(&AudioDecoderOpusParsePayload_nid_no_patch),
    reinterpret_cast<void*>(&_ZNK6webrtc12AudioDecoder12HasDecodePlcEv_nid_postfix),
    reinterpret_cast<void*>(&_ZN6webrtc12AudioDecoder9DecodePlcEmPs_nid_postfix),
    reinterpret_cast<void*>(&AudioDecoderOpusGeneratePlc_nid_no_patch),
    reinterpret_cast<void*>(&AudioDecoderOpusReset_nid_no_patch),
    reinterpret_cast<void*>(&_ZN6webrtc12AudioDecoder9ErrorCodeEv_nid_postfix),
    reinterpret_cast<void*>(&AudioDecoderOpusPacketDuration_nid_no_patch),
    reinterpret_cast<void*>(&AudioDecoderOpusPacketDurationRedundant_nid_no_patch),
    reinterpret_cast<void*>(&AudioDecoderOpusPacketHasFec_nid_no_patch),
    reinterpret_cast<void*>(&AudioDecoderOpusSampleRateHz_nid_no_patch),
    reinterpret_cast<void*>(&AudioDecoderOpusChannels_nid_no_patch),
    reinterpret_cast<void*>(&AudioDecoderOpusDecodeInternal_nid_no_patch),
    reinterpret_cast<void*>(&AudioDecoderOpusDecodeRedundantInternal_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN6webrtc20AudioDecoderOpusImplC1ERKNS_15FieldTrialsViewEmi_nid_postfix(WebRtcAudioDecoderOpusImpl* decoder, const WebRtcFieldTrialsView* fieldTrials, std::size_t numChannels, int sampleRateHz) {
    if (!decoder || !GuestWritable_nid_no_patch(decoder, sizeof(*decoder)))
        throw std::invalid_argument("AudioDecoderOpusImpl constructor requires a writable decoder");
    if (!fieldTrials)
        throw std::invalid_argument("AudioDecoderOpusImpl constructor requires FieldTrialsView");
    if (numChannels != 1 && numChannels != 2)
        throw std::invalid_argument("AudioDecoderOpusImpl constructor requires 1 or 2 channels");
    if (sampleRateHz != 16000 && sampleRateHz != 48000)
        throw std::invalid_argument("AudioDecoderOpusImpl constructor requires a 16000 or 48000 Hz sample rate");
    const bool generatePlc = IsGeneratePlcEnabled(fieldTrials);
    int error = OPUS_OK;
    OpusDecoder* opusDecoder = opus_decoder_create(sampleRateHz, static_cast<int>(numChannels), &error);
    if (error != OPUS_OK || !opusDecoder) {
        if (opusDecoder)
            opus_decoder_destroy(opusDecoder);
        throw std::runtime_error("AudioDecoderOpusImpl failed to create Opus decoder");
    }
    opus_decoder_ctl(opusDecoder, OPUS_RESET_STATE);
    auto* state = new WebRtcOpusDecInst{opusDecoder, numChannels, sampleRateHz, 0, static_cast<int>(numChannels)};
    WebRtcAudioDecoderOpusImpl host{Vtable + 2, state, numChannels, sampleRateHz, generatePlc};
    if (!GuestPtr<WebRtcAudioDecoderOpusImpl>(decoder).CopyOut(host)) {
        opus_decoder_destroy(opusDecoder);
        delete state;
        throw std::invalid_argument("AudioDecoderOpusImpl constructor requires a writable decoder");
    }
}

void APS5_VABI _ZN6webrtc20AudioDecoderOpusImplD1Ev_nid_postfix(WebRtcAudioDecoderOpusImpl* decoder) {
    const auto host = ReadGuest(decoder, "AudioDecoderOpusImpl destructor requires a decoder");
    if (host.decState) {
        if (host.decState->decoder)
            opus_decoder_destroy(host.decState->decoder);
        delete host.decState;
    }
}

}
