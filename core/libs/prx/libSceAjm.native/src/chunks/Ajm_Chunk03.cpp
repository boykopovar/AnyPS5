#include "prx/libSceAjm.native/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "libatrac9.h"
#include "atrac9_dec.h"
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/mem.h>
}
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <span>

// The batch buffer and its job records are private to this library: the guest only reserves the
// memory and reads the "used bytes" field of AjmBatchInfo. Sideband results written back to guest
// memory follow the SDK layouts.
namespace {
void OpenOpus(Instance& instance) {
    static const bool quiet = (av_log_set_level(TraceEnabled() ? AV_LOG_WARNING : AV_LOG_QUIET), true);
    (void)quiet;
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    if (instance.opus) avcodec_free_context(&instance.opus);
    instance.opus = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (!instance.opus) throw std::runtime_error("AJM: cannot allocate the FFmpeg Opus decoder");
    av_channel_layout_default(&instance.opus->ch_layout, static_cast<int>(instance.opusChannels));
    instance.opus->sample_rate = 48000;
    if (instance.opusMappingFamily == 255 && instance.opusChannels > 1) {
        instance.opus->extradata_size = static_cast<int>(21 + instance.opusChannels);
        instance.opus->extradata = static_cast<std::uint8_t*>(av_mallocz(instance.opus->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!instance.opus->extradata) throw std::bad_alloc();
        auto* header = instance.opus->extradata;
        std::memcpy(header, "OpusHead", 8);
        header[8] = 1;
        header[9] = static_cast<std::uint8_t>(instance.opusChannels);
        header[12] = 0x80;
        header[13] = 0xbb;
        header[18] = 255;
        header[19] = static_cast<std::uint8_t>(instance.opusStreams);
        header[20] = static_cast<std::uint8_t>(instance.opusCoupled);
        for (std::uint32_t channel = 0; channel < instance.opusChannels; ++channel) header[21 + channel] = static_cast<std::uint8_t>(channel);
    }
    if (instance.opusMappingFamily == 1) {
        static constexpr std::uint8_t streams[8] = {1, 1, 2, 2, 3, 4, 4, 5};
        static constexpr std::uint8_t coupled[8] = {0, 1, 1, 2, 2, 2, 3, 3};
        static constexpr std::uint8_t mapping[8][8] = {
            {0}, {0, 1}, {0, 2, 1}, {0, 1, 2, 3},
            {0, 4, 1, 2, 3}, {0, 4, 1, 2, 3, 5},
            {0, 4, 1, 2, 3, 5, 6}, {0, 6, 1, 2, 3, 4, 5, 7}
        };
        const std::size_t index = instance.opusChannels - 1;
        instance.opus->extradata_size = static_cast<int>(21 + instance.opusChannels);
        instance.opus->extradata = static_cast<std::uint8_t*>(av_mallocz(instance.opus->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!instance.opus->extradata) throw std::bad_alloc();
        auto* header = instance.opus->extradata;
        std::memcpy(header, "OpusHead", 8);
        header[8] = 1;
        header[9] = static_cast<std::uint8_t>(instance.opusChannels);
        header[12] = 0x80;
        header[13] = 0xbb;
        header[18] = 1;
        header[19] = streams[index];
        header[20] = coupled[index];
        std::memcpy(header + 21, mapping[index], instance.opusChannels);
    }
    if (avcodec_open2(instance.opus, codec, nullptr) < 0) throw std::runtime_error("AJM: cannot open the FFmpeg Opus decoder");
}

std::size_t OpusPacketSamples(const std::uint8_t* packet, std::size_t size) {
    if (size < 1) return 0;
    const std::uint32_t config = packet[0] >> 3u;
    std::size_t frameSamples = 0;
    if (config < 12) frameSamples = (config & 3u) == 0 ? 480 : (config & 3u) == 1 ? 960 : (config & 3u) == 2 ? 1920 : 2880;
    else if (config < 16) frameSamples = (config & 1u) == 0 ? 480 : 960;
    else frameSamples = std::size_t{120} << (config & 3u);
    std::size_t frames = 0;
    switch (packet[0] & 3u) {
    case 0: frames = 1; break;
    case 1:
    case 2: frames = 2; break;
    default:
        if (size < 2) return 0;
        frames = packet[1] & 0x3fu;
        break;
    }
    return frames * frameSamples;
}

std::size_t OpusLength(const std::uint8_t* data, std::size_t size, std::size_t& at) {
    if (at >= size) return SIZE_MAX;
    const std::size_t first = data[at++];
    if (first < 252) return first;
    if (at >= size) return SIZE_MAX;
    return first + 4u * data[at++];
}

std::size_t OpusSelfDelimitedBytes(const std::uint8_t* data, std::size_t size) {
    if (size < 1) return SIZE_MAX;
    const std::uint32_t code = data[0] & 3u;
    std::size_t at = 1;
    if (code == 0) {
        const std::size_t frame = OpusLength(data, size, at);
        return frame == SIZE_MAX ? SIZE_MAX : at + frame;
    }
    if (code == 1) {
        const std::size_t frame = OpusLength(data, size, at);
        return frame == SIZE_MAX ? SIZE_MAX : at + 2 * frame;
    }
    if (code == 2) {
        const std::size_t first = OpusLength(data, size, at);
        const std::size_t second = OpusLength(data, size, at);
        return first == SIZE_MAX || second == SIZE_MAX ? SIZE_MAX : at + first + second;
    }
    if (at >= size) return SIZE_MAX;
    const std::uint8_t count = data[at++];
    const std::size_t frames = count & 0x3fu;
    if (frames == 0) return SIZE_MAX;
    std::size_t padding = 0;
    if ((count & 0x40u) != 0) {
        for (;;) {
            if (at >= size) return SIZE_MAX;
            const std::uint8_t value = data[at++];
            padding += value == 255 ? 254 : value;
            if (value != 255) break;
        }
    }
    std::size_t total = 0;
    if ((count & 0x80u) != 0) {
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const std::size_t bytes = OpusLength(data, size, at);
            if (bytes == SIZE_MAX) return SIZE_MAX;
            total += bytes;
        }
    } else {
        const std::size_t bytes = OpusLength(data, size, at);
        if (bytes == SIZE_MAX) return SIZE_MAX;
        total = bytes * frames;
    }
    return at + total + padding;
}

void OpenOpusFromPacket(Instance& instance, const std::uint8_t* data, std::size_t size) {
    std::uint32_t streams = 0;
    std::uint32_t coupled = 0;
    std::size_t offset = 0;
    while (offset < size) {
        if ((data[offset] & 0x04u) != 0) ++coupled;
        ++streams;
        const std::uint32_t channels = coupled * 2 + (streams - coupled);
        if (channels >= instance.opusChannels) break;
        const std::size_t bytes = OpusSelfDelimitedBytes(data + offset, size - offset);
        if (bytes == SIZE_MAX || bytes >= size - offset) break;
        offset += bytes;
    }
    if (coupled * 2 + (streams - coupled) != instance.opusChannels) throw std::runtime_error("AJM Opus: a mapping family 255 packet's " + std::to_string(streams) + " streams (" + std::to_string(coupled) + " stereo) do not make its " + std::to_string(instance.opusChannels) + " channels");
    instance.opusStreams = streams;
    instance.opusCoupled = coupled;
    instance.opusLayoutPending = false;
    OpenOpus(instance);
}

void RunPcm(Instance& instance, const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    if (job.flags & ~SIDEBAND_STREAM) NotImplemented_nid_no_patch("AJM codec 23 run with flags other than SIDEBAND_STREAM");
    const auto encoding = PcmEncoding(instance);
    if (encoding > 2) throw std::runtime_error("AJM codec 23: PCM encoding " + std::to_string(encoding) + " is not implemented");
    if (instance.resampler.change != 0.0f) NotImplemented_nid_no_patch("AJM codec 23 run with a resample ratio change per sample");
    if (instance.gapless.totalSamples != 0 || instance.gapless.skipSamples != 0) NotImplemented_nid_no_patch("AJM codec 23 run with gapless decode counts");
    if (instance.pcmResampled) NotImplemented_nid_no_patch("AJM codec 23 run after a resampled run without an initialize or context clear in between");
    const JobInput input(job, inputs);
    PcmOutputs pcmOutputs(outputs, job.outputCount);
    const std::size_t channels = instance.pcmChannels;
    const std::size_t inputFrameBytes = channels * sizeof(std::int16_t);
    const std::size_t sampleBytes = encoding == 0 ? sizeof(std::int16_t) : sizeof(std::int32_t);
    if (input.empty()) NotImplemented_nid_no_patch("AJM codec 23 run with an empty input");
    if (input.size() % inputFrameBytes != 0) NotImplemented_nid_no_patch("AJM codec 23 run with an input that is not a whole number of frames");
    const std::size_t frames = input.size() / inputFrameBytes;
    const double ratio = instance.resampler.ratio;
    const std::size_t room = pcmOutputs.Room() / (channels * sampleBytes);
    std::size_t outputFrames = 0;
    while (outputFrames < room && static_cast<double>(outputFrames) * ratio < static_cast<double>(frames)) ++outputFrames;
    if (static_cast<double>(outputFrames) * ratio < static_cast<double>(frames)) NotImplemented_nid_no_patch("AJM codec 23 run whose output cannot hold the whole resampled input");
    const auto sample = [&](std::size_t index, std::size_t channel) { return PcmValue(input.data() + (std::min(index, frames - 1) * channels + channel) * sizeof(std::int16_t), 0); };
    std::vector<std::uint8_t> frame(channels * sampleBytes);
    for (std::size_t output = 0; output < outputFrames; ++output) {
        const double position = static_cast<double>(output) * ratio;
        const auto index = static_cast<std::size_t>(position);
        const double t = position - static_cast<double>(index);
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const double value = CatmullRom(sample(index == 0 ? 0 : index - 1, channel), sample(index, channel), sample(index + 1, channel), sample(index + 2, channel), t);
            StorePcm(value, frame.data() + channel * sampleBytes, encoding);
        }
        pcmOutputs.Emit(frame.data(), frame.size());
    }
    if (ratio != 1.0) instance.pcmResampled = true;
    instance.totalDecodedSamples += frames;
    AJM_TRACE("[ajm] instance %u pcm run flags 0x%llx: %zu input bytes consumed, %zu output bytes, ratio %f -> produced %zu, %zu channels, total samples %llu\n", job.instance, static_cast<unsigned long long>(job.flags), input.size(), pcmOutputs.capacity, ratio, pcmOutputs.produced, channels, static_cast<unsigned long long>(instance.totalDecodedSamples));
    WriteRunSideband(job, instance, 0, input.size(), pcmOutputs.produced, 1, CurrentFormat(instance));
}

void RunOpus(Instance& instance, const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    const JobInput input(job, inputs);
    PcmOutputs pcmOutputs(outputs, job.outputCount);
    const auto encoding = PcmEncoding(instance);
    if (encoding > 2) throw std::runtime_error("AJM Opus: PCM encoding " + std::to_string(encoding) + " is not implemented");
    const std::size_t sampleBytes = encoding == 0 ? sizeof(std::int16_t) : sizeof(std::int32_t);
    std::unique_ptr<AVPacket, void (*)(AVPacket*)> packet(av_packet_alloc(), [](AVPacket* p) { av_packet_free(&p); });
    std::unique_ptr<AVFrame, void (*)(AVFrame*)> decoded(av_frame_alloc(), [](AVFrame* f) { av_frame_free(&f); });
    if (!packet || !decoded) throw std::bad_alloc();
    std::vector<std::uint8_t> pcm;
    const std::size_t channels = instance.opusChannels;

    std::int32_t result = 0;
    std::size_t consumed = 0;
    std::uint32_t frames = 0;
    const std::size_t frameBytes = channels * sampleBytes;
    if (Resampling(instance) && !instance.opusPending.empty()) {
        std::vector<double> values(instance.opusPending.size() / sampleBytes);
        for (std::size_t index = 0; index < values.size(); ++index) values[index] = PcmValue(instance.opusPending.data() + index * sampleBytes, encoding);
        ResamplerPush(instance, values.data(), values.size() / channels, channels);
        instance.opusPending.clear();
    }
    if (Resampling(instance)) ResamplerProduce(instance, pcmOutputs, channels, encoding, sampleBytes);
    if (!instance.opusPending.empty()) {
        const std::size_t bytes = std::min(instance.opusPending.size(), pcmOutputs.Room());
        pcmOutputs.Emit(instance.opusPending.data(), bytes);
        instance.opusPending.erase(instance.opusPending.begin(), instance.opusPending.begin() + static_cast<std::ptrdiff_t>(bytes));
    }
    while (consumed < input.size() && instance.opusPending.empty()) {
        if (frames != 0 && pcmOutputs.Room() < frameBytes) break;
        if (input.size() - consumed < 2) {
            result |= AJM_RESULT_PARTIAL_INPUT;
            break;
        }
        const std::size_t bytes = input[consumed] | (std::size_t{input[consumed + 1]} << 8u);
        if (input.size() - consumed - 2 < bytes) {
            result |= AJM_RESULT_PARTIAL_INPUT;
            break;
        }
        const auto* data = input.data() + consumed + 2;
        const std::size_t samples = OpusPacketSamples(data, bytes);
        if (bytes == 0 || samples == 0) {
            result |= AJM_RESULT_INVALID_DATA;
            break;
        }
        if (pcmOutputs.Room() < frameBytes) {
            if (frames == 0 && pcmOutputs.produced == 0) result |= AJM_RESULT_NOT_ENOUGH_ROOM;
            break;
        }
        if (instance.opusLayoutPending) OpenOpusFromPacket(instance, data, bytes);
        packet->data = const_cast<std::uint8_t*>(data);
        packet->size = static_cast<int>(bytes);
        if (avcodec_send_packet(instance.opus, packet.get()) < 0) {
            result |= AJM_RESULT_INVALID_DATA;
            break;
        }
        consumed += 2 + bytes;
        ++frames;
        int status = 0;
        while ((status = avcodec_receive_frame(instance.opus, decoded.get())) == 0) {
            const auto format = static_cast<AVSampleFormat>(decoded->format);
            if (format != AV_SAMPLE_FMT_FLTP && format != AV_SAMPLE_FMT_FLT) throw std::runtime_error("AJM Opus: FFmpeg sample format " + std::to_string(decoded->format) + " is not converted");
            const auto frameChannels = static_cast<std::size_t>(decoded->ch_layout.nb_channels);
            const auto frameSamples = static_cast<std::size_t>(decoded->nb_samples);
            const auto value = [&](std::size_t sample, std::size_t channel) {
                const std::size_t sourceChannel = std::min(channel, frameChannels - 1);
                return format == AV_SAMPLE_FMT_FLTP ? reinterpret_cast<const float*>(decoded->extended_data[sourceChannel])[sample]
                                                    : reinterpret_cast<const float*>(decoded->data[0])[sample * frameChannels + sourceChannel];
            };
            std::size_t first = 0;
            std::size_t count = frameSamples;
            if (instance.gapless.skippedSamples < instance.gapless.skipSamples) {
                const std::size_t skip = std::min<std::size_t>(count, instance.gapless.skipSamples - instance.gapless.skippedSamples);
                instance.gapless.skippedSamples = static_cast<std::uint16_t>(instance.gapless.skippedSamples + skip);
                first += skip;
                count -= skip;
            }
            if (instance.gapless.totalSamples != 0) {
                const std::uint64_t remaining = instance.gapless.totalSamples > instance.segmentSamples ? instance.gapless.totalSamples - instance.segmentSamples : 0;
                count = static_cast<std::size_t>(std::min<std::uint64_t>(count, remaining));
            }
            instance.segmentSamples += count;
            instance.totalDecodedSamples += count;
            if (Resampling(instance)) {
                std::vector<double> values(count * channels);
                for (std::size_t sample = 0; sample < count; ++sample) {
                    for (std::size_t channel = 0; channel < channels; ++channel) values[sample * channels + channel] = value(first + sample, channel);
                }
                ResamplerPush(instance, values.data(), count, channels);
                ResamplerProduce(instance, pcmOutputs, channels, encoding, sampleBytes);
            } else {
                pcm.assign(count * frameBytes, 0);
                for (std::size_t sample = 0; sample < count; ++sample) {
                    for (std::size_t channel = 0; channel < channels; ++channel) StorePcm(value(first + sample, channel), pcm.data() + (sample * channels + channel) * sampleBytes, encoding);
                }
                const std::size_t fits = std::min(pcm.size(), pcmOutputs.Room() / frameBytes * frameBytes);
                pcmOutputs.Emit(pcm.data(), fits);
                instance.opusPending.insert(instance.opusPending.end(), pcm.begin() + static_cast<std::ptrdiff_t>(fits), pcm.end());
            }
            av_frame_unref(decoded.get());
        }
        if (status != AVERROR(EAGAIN)) {
            result |= AJM_RESULT_INVALID_DATA;
            break;
        }
        if ((job.flags & RUN_MULTIPLE_FRAMES) == 0) break;
    }
    if (consumed == input.size() && frames == 0 && pcmOutputs.produced == 0 && result == 0) result |= AJM_RESULT_PARTIAL_INPUT;

    AJM_TRACE("[ajm] instance %u opus run flags 0x%llx: %zu input bytes, %zu output bytes -> result 0x%x, %u frames, consumed %zu, produced %zu, total samples %llu\n", job.instance, static_cast<unsigned long long>(job.flags), input.size(), pcmOutputs.capacity, static_cast<unsigned>(result), frames, consumed, pcmOutputs.produced, static_cast<unsigned long long>(instance.totalDecodedSamples));
    WriteRunSideband(job, instance, result, consumed, pcmOutputs.produced, frames, CurrentFormat(instance));
}

std::uint32_t InstanceChannels(const Instance& instance) {
    return static_cast<std::uint32_t>((instance.flags >> 3u) & 0xFu);
}

std::uint32_t AacRawChannels(const Instance& instance) {
    const std::uint32_t channels = InstanceChannels(instance);
    return channels == 0 ? 2 : channels;
}

std::uint32_t AacDelayFrames(const Instance& instance) {
    return (instance.flags & M4AAC_ENABLE_NONDELAY_OUTPUT) ? 0 : M4AAC_DELAY_FRAMES;
}

void OpenAac(Instance& instance) {
    static const bool quiet = (av_log_set_level(TraceEnabled() ? AV_LOG_WARNING : AV_LOG_QUIET), true);
    (void)quiet;
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (instance.aac) avcodec_free_context(&instance.aac);
    instance.aac = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (!instance.aac) throw std::runtime_error("AJM: cannot allocate the FFmpeg AAC decoder");
    if (instance.aacConfig == M4AAC_CONFIG_RAW) {
        const std::uint32_t config = (2u << 11u) | (instance.aacFrequencyIndex << 7u) | (AacRawChannels(instance) << 3u);
        instance.aac->extradata = static_cast<std::uint8_t*>(av_mallocz(2 + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!instance.aac->extradata) throw std::bad_alloc();
        instance.aac->extradata[0] = static_cast<std::uint8_t>(config >> 8u);
        instance.aac->extradata[1] = static_cast<std::uint8_t>(config);
        instance.aac->extradata_size = 2;
    }
    if (avcodec_open2(instance.aac, codec, nullptr) < 0) throw std::runtime_error("AJM: cannot open the FFmpeg AAC decoder");
}

std::int32_t InitializeAac(Instance& instance, const std::uint8_t* parameters, std::uint64_t size) {
    if (size < 8) return AJM_RESULT_INVALID_PARAMETER;
    std::uint32_t config = 0;
    std::uint32_t frequencyIndex = 0;
    std::memcpy(&config, parameters, sizeof(config));
    std::memcpy(&frequencyIndex, parameters + 4, sizeof(frequencyIndex));
    if (config != M4AAC_CONFIG_ADTS && config != M4AAC_CONFIG_RAW) return AJM_RESULT_INVALID_PARAMETER;
    if (config == M4AAC_CONFIG_RAW && frequencyIndex >= 12) return AJM_RESULT_INVALID_PARAMETER;
    const std::uint32_t rawChannels = AacRawChannels(instance);
    if (config == M4AAC_CONFIG_RAW && (rawChannels < 1 || rawChannels > 6)) throw std::runtime_error("AJM M4AAC: raw initialization of an instance created for " + std::to_string(rawChannels) + " channels is not implemented (only 1 through 6)");
    instance.aacConfig = config;
    instance.aacFrequencyIndex = frequencyIndex;
    OpenAac(instance);
    instance.aacChannels = 0;
    instance.aacSampleRate = 0;
    instance.aacBitrate = 0;
    instance.aacFrameSamples = 0;
    instance.aacHeaac = false;
    instance.aacDelayFrames = AacDelayFrames(instance);
    instance.initialized = true;
    instance.totalDecodedSamples = 0;
    instance.segmentSamples = 0;
    instance.gapless = {};
    return 0;
}

struct AdtsHeader {
    std::size_t frameBytes;
    std::uint32_t profile;
    std::uint32_t frequencyIndex;
    std::uint32_t channelConfig;
    std::uint32_t rawBlocks;
};

constexpr std::size_t ADTS_HEADER_BYTES = 7;

bool ParseAdtsHeader(const std::uint8_t* data, AdtsHeader& header) {
    if (data[0] != 0xFFu || (data[1] & 0xF6u) != 0xF0u) return false;
    header.profile = data[2] >> 6u;
    header.frequencyIndex = (data[2] >> 2u) & 0xFu;
    header.channelConfig = ((data[2] & 1u) << 2u) | (data[3] >> 6u);
    header.frameBytes = ((std::size_t{data[3]} & 3u) << 11u) | (std::size_t{data[4]} << 3u) | (data[5] >> 5u);
    header.rawBlocks = data[6] & 3u;
    return header.frameBytes >= ((data[1] & 1u) ? ADTS_HEADER_BYTES : ADTS_HEADER_BYTES + 2);
}

std::size_t EmittedSamples(const Instance& instance, std::size_t samples) {
    if (instance.gapless.skippedSamples < instance.gapless.skipSamples) samples -= std::min<std::size_t>(samples, instance.gapless.skipSamples - instance.gapless.skippedSamples);
    if (instance.gapless.totalSamples != 0) {
        const std::uint64_t remaining = instance.gapless.totalSamples > instance.segmentSamples ? instance.gapless.totalSamples - instance.segmentSamples : 0;
        samples = static_cast<std::size_t>(std::min<std::uint64_t>(samples, remaining));
    }
    return samples;
}

