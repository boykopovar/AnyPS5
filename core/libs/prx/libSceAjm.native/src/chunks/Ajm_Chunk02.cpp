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
std::uint32_t At9Bitrate(const Instance& instance) {
    const auto samples = static_cast<std::uint64_t>(instance.info.framesInSuperframe) * static_cast<std::uint64_t>(instance.info.frameSamples);
    if (samples == 0) return 0;
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(instance.info.superframeSize) * 8u * static_cast<std::uint64_t>(instance.info.samplingRate) / samples);
}

SidebandFormat CurrentFormat(const Instance& instance) {
    switch (instance.codec) {
    case CODEC_AT9: return {static_cast<std::uint32_t>(instance.info.channels), ChannelMask(static_cast<std::size_t>(instance.info.channels)), static_cast<std::uint32_t>(instance.info.samplingRate), PcmEncoding(instance), At9Bitrate(instance), 0};
    case CODEC_MP3: return {instance.mp3Channels, ChannelMask(instance.mp3Channels), instance.mp3SampleRate, PcmEncoding(instance), instance.mp3Bitrate, 0};
    case CODEC_OPUS: return {instance.opusChannels, ChannelMask(instance.opusChannels), instance.opusSampleRate, PcmEncoding(instance), 0, 0};
    case CODEC_M4AAC:
        if (instance.aacChannels == 3 || instance.aacChannels == 5) NotImplemented_nid_no_patch("AJM M4AAC format with an unknown three- or five-channel speaker mask");
        return {instance.aacChannels, ChannelMask(instance.aacChannels), instance.aacSampleRate, PcmEncoding(instance), instance.aacBitrate, 0};
    case CODEC_PCM: return {instance.pcmChannels, ChannelMask(instance.pcmChannels), instance.pcmSampleRate, PcmEncoding(instance), 0, 0};
    default: throw std::runtime_error("AJM: the format of codec " + std::to_string(instance.codec) + " is not implemented");
    }
}

SidebandAt9CodecInfo At9CodecInfo(const Instance& instance) {
    const auto superframeSize = static_cast<std::uint32_t>(instance.info.superframeSize);
    return {superframeSize, static_cast<std::uint32_t>(instance.info.framesInSuperframe), instance.superframeRemaining == 0 ? superframeSize : instance.superframeRemaining, static_cast<std::uint32_t>(instance.info.frameSamples)};
}

void WriteRunSideband(const JobHeader& job, const Instance& instance, std::int32_t result, std::size_t consumed, std::size_t produced, std::uint32_t frames, const SidebandFormat& format) {
    auto* sideband = static_cast<std::uint8_t*>(job.sideband);
    std::size_t offset = 0;
    bool stopped = false;
    const auto write = [&](const void* value, std::size_t size) {
        if (stopped || !sideband) return;
        if (offset + size > job.sidebandSize) {
            stopped = true;
            return;
        }
        std::memcpy(sideband + offset, value, size);
        offset += size;
    };
    const SidebandResult status{result, 0};
    write(&status, sizeof(status));
    if (job.flags & SIDEBAND_STREAM) {
        const SidebandStream stream{static_cast<std::int32_t>(consumed), static_cast<std::int32_t>(produced), instance.totalDecodedSamples};
        write(&stream, sizeof(stream));
    }
    if (job.flags & SIDEBAND_FORMAT) write(&format, sizeof(format));
    if (job.flags & SIDEBAND_GAPLESS_DECODE) write(&instance.gapless, sizeof(instance.gapless));
    if ((job.flags & RUN_GET_CODEC_INFO) && instance.codec == CODEC_M4AAC) {
        const SidebandM4aacCodecInfo codecInfo{instance.aacHeaac ? 1u : 0u, 0};
        write(&codecInfo, sizeof(codecInfo));
    } else if (job.flags & RUN_GET_CODEC_INFO) {
        const SidebandAt9CodecInfo codecInfo = At9CodecInfo(instance);
        write(&codecInfo, sizeof(codecInfo));
    }
    if (job.flags & RUN_MULTIPLE_FRAMES) {
        const SidebandMultipleFrames multiple{frames, 0};
        write(&multiple, sizeof(multiple));
    }
}

void RunAt9(Instance& instance, const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    const JobInput input(job, inputs);
    PcmOutputs pcmOutputs(outputs, job.outputCount);

    const auto channels = static_cast<std::size_t>(instance.info.channels);
    const auto frameSamples = static_cast<std::size_t>(instance.info.frameSamples);
    const auto encoding = PcmEncoding(instance);
    const std::size_t sampleBytes = encoding == 0 ? sizeof(std::int16_t) : sizeof(std::int32_t);
    const std::size_t frameBytes = frameSamples * channels * sampleBytes;
    std::vector<std::uint8_t> pcm(frameBytes);
    static const bool traceFlags = std::getenv("APS5_TRACE_AJM") != nullptr;
    if (traceFlags && instance.totalDecodedSamples == 0 && consumedFirstRun(instance)) std::fprintf(stderr, "[ajm] instance %u flags 0x%llx (encoding %u)\n", job.instance, static_cast<unsigned long long>(instance.flags), encoding);

    std::int32_t result = 0;
    int decodeStatus = 0;
    std::size_t consumed = 0;
    std::uint32_t frames = 0;

    // A superframe is a fixed superframeSize bytes holding framesInSuperframe frames packed back to back,
    // each byte aligned but of its own size (the encoder moves bits between the frames of a superframe),
    // with padding after the last one. The decoder reports each frame's size, so frames are walked with
    // it and the padding is skipped after the last frame. Like the console's decoder, a superframe is
    // decoded only once it is wholly present: a trailing partial superframe stays unconsumed and is
    // reported as partial input, and the title resubmits it from the consumed offset.
    const auto superframeSize = static_cast<std::size_t>(instance.info.superframeSize);
    const auto framesInSuperframe = static_cast<std::uint32_t>(std::max(1, instance.info.framesInSuperframe));
    if (Resampling(instance)) ResamplerProduce(instance, pcmOutputs, channels, encoding, sampleBytes);
    const auto segmentEnded = [&] { return instance.gapless.totalSamples != 0 && instance.segmentSamples >= instance.gapless.totalSamples; };
    for (;;) {
        if (instance.superframeRemaining == 0) consumed += RiffDataOffset(input.data() + consumed, input.size() - consumed);
        const std::size_t needed = instance.superframeRemaining == 0 ? superframeSize : instance.superframeRemaining;
        const bool roomForFrame = segmentEnded() || (Resampling(instance) ? pcmOutputs.Room() >= channels * sampleBytes : pcmOutputs.Room() >= frameBytes);
        // A PS4 reports which side ran out (probe runs of a title's ATRAC9 stream through libSceAjm's
        // builders): PARTIAL_INPUT when the input ran out, on a superframe boundary or not, with room
        // left; NOT_ENOUGH_ROOM when the room ran out with input left, after however many frames (and
        // with none decoded for a zero room); both (0x18) when the input ended just as the output
        // filled, which is every Wwise run job. A run without input (a format query) reports 0.
        // Resampled runs were not measured and keep the earlier rule: a trailing partial superframe is
        // partial input, a zero-output run is out of room.
        const bool measured = !Resampling(instance);
        if (input.size() - consumed < needed) {
            if (input.size() != consumed || (measured && !input.empty())) result |= AJM_RESULT_PARTIAL_INPUT;
            if (measured && !input.empty() && !roomForFrame) result |= AJM_RESULT_NOT_ENOUGH_ROOM;
            break;
        }
        if (!roomForFrame) {
            if (measured || (frames == 0 && pcmOutputs.produced == 0)) result |= AJM_RESULT_NOT_ENOUGH_ROOM;
            break;
        }
        if (instance.superframeRemaining == 0) {
            instance.superframeRemaining = static_cast<std::uint32_t>(superframeSize);
            instance.frameInSuperframe = 0;
        }
        int used = 0;
        decodeStatus = DecodeAt9Frame(instance, input.data() + consumed, static_cast<int>(instance.superframeRemaining), pcm.data(), encoding, sampleBytes, &used);
        if (decodeStatus != 0 || used <= 0 || static_cast<std::uint32_t>(used) > instance.superframeRemaining) {
            result |= AJM_RESULT_INVALID_DATA;
            // The decoder tracks its position in the superframe; a fresh one starts the next superframe.
            ResetDecoder(instance);
            break;
        }
        consumed += static_cast<std::size_t>(used);
        instance.superframeRemaining -= static_cast<std::uint32_t>(used);
        if (++instance.frameInSuperframe == framesInSuperframe) {
            consumed += instance.superframeRemaining;
            instance.superframeRemaining = 0;
        }
        ++frames;

        EmitFrame(instance, pcmOutputs, pcm.data(), frameSamples, channels, sampleBytes);
        if (job.console && (job.flags & RUN_MULTIPLE_FRAMES) == 0) break;
    }

    if (TraceEnabled()) {
        std::fprintf(stderr, "[ajm] instance %u run flags 0x%llx: %zu input bytes in %u buffers, %zu output bytes in %u buffers, sideband %llu bytes; superframe %d (%d frames), %d channels, %d samples/frame, %d Hz, config %02x %02x %02x %02x -> result 0x%x, decoder status 0x%x, %u frames, consumed %zu, produced %zu, total samples %llu, gapless total %u skip %u skipped %u, input",
                     job.instance, static_cast<unsigned long long>(job.flags), input.size(), job.inputCount, pcmOutputs.capacity, job.outputCount, static_cast<unsigned long long>(job.sidebandSize), instance.info.superframeSize, instance.info.framesInSuperframe, instance.info.channels, instance.info.frameSamples, instance.info.samplingRate,
                     instance.info.configData[0], instance.info.configData[1], instance.info.configData[2], instance.info.configData[3], static_cast<unsigned>(result), static_cast<unsigned>(decodeStatus), frames, consumed, pcmOutputs.produced, static_cast<unsigned long long>(instance.totalDecodedSamples), instance.gapless.totalSamples, instance.gapless.skipSamples, instance.gapless.skippedSamples);
        for (std::size_t i = 0; i < consumed && i < 16; ++i) std::fprintf(stderr, " %02x", input[i]);
        std::fprintf(stderr, "\n");
    }
    WriteRunSideband(job, instance, result, consumed, pcmOutputs.produced, frames, CurrentFormat(instance));
}

struct Mp3Frame {
    std::size_t bytes;
    std::uint32_t channels;
    std::uint32_t samples;
    std::uint32_t sampleRate;
    std::uint32_t bitrate;
};

bool ParseMp3Frame(const std::uint8_t* data, std::size_t size, Mp3Frame& frame) {
    if (size < 4) return false;
    const std::uint32_t header = (std::uint32_t{data[0]} << 24u) | (std::uint32_t{data[1]} << 16u) | (std::uint32_t{data[2]} << 8u) | data[3];
    const auto version = (header >> 19u) & 3u;
    const auto layer = (header >> 17u) & 3u;
    const auto bitrateIndex = (header >> 12u) & 15u;
    const auto rateIndex = (header >> 10u) & 3u;
    if ((header & 0xFFE00000u) != 0xFFE00000u || version == 1 || layer != 1 || bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3) return false;
    static constexpr std::uint32_t rates[4][3] = {{11025, 12000, 8000}, {0, 0, 0}, {22050, 24000, 16000}, {44100, 48000, 32000}};
    static constexpr std::uint32_t mpeg1Kbps[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static constexpr std::uint32_t mpeg2Kbps[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    const bool mpeg1 = version == 3;
    frame.sampleRate = rates[version][rateIndex];
    frame.bitrate = (mpeg1 ? mpeg1Kbps : mpeg2Kbps)[bitrateIndex] * 1000u;
    frame.samples = mpeg1 ? 1152u : 576u;
    frame.channels = ((header >> 6u) & 3u) == 3u ? 1u : 2u;
    frame.bytes = (mpeg1 ? 144u : 72u) * frame.bitrate / frame.sampleRate + ((header >> 9u) & 1u);
    return true;
}

constexpr std::uint32_t MP3_OFL_NONE = 0;
constexpr std::uint32_t MP3_OFL_LAME = 1;
constexpr std::uint32_t MP3_OFL_VBRI = 2;
constexpr std::uint32_t MP3_OFL_FGH = 3;
constexpr std::uint32_t MP3_OFL_VBRI_AND_FGH = 4;

std::uint32_t ReadBigEndian(const std::uint8_t* data, std::size_t bytes) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < bytes; ++i) value = (value << 8u) | data[i];
    return value;
}

int ParseMp3Header(const std::uint8_t* stream, std::uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame);

void ParseMp3Ofl(const std::uint8_t* stream, std::uint32_t streamSize, std::uint32_t header, AjmDecMp3ParseFrame* frame, int parseOfl) {
    const std::size_t end = std::min<std::size_t>(streamSize, frame->frame_size);
    const std::size_t sideInfoStart = 4 + (((header >> 16u) & 1u) == 0 ? 2u : 0u);
    if (end <= sideInfoStart) return;
    const std::size_t available = end - sideInfoStart;
    std::size_t bit = 0;
    const auto readBits = [&](std::uint32_t count) {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < count; ++i, ++bit) value = (value << 1u) | (bit / 8 < available ? (stream[sideInfoStart + bit / 8] >> (7 - bit % 8)) & 1u : 0u);
        return value;
    };
    const bool mpeg1 = ((header >> 19u) & 3u) == 3u;
    const bool mono = ((header >> 6u) & 3u) == 3u;
    bit += mpeg1 ? (mono ? 18 : 20) : (mono ? 9 : 10);
    std::uint32_t mainDataBits = 0;
    for (std::uint32_t granule = 0; granule < (mpeg1 ? 2u : 1u); ++granule) {
        for (std::uint32_t channel = 0; channel < frame->num_channels; ++channel) {
            mainDataBits += readBits(12);
            bit += mpeg1 ? 47 : 51;
        }
    }
    bit += mainDataBits;
    std::size_t at = sideInfoStart + std::min<std::size_t>((bit + 7) / 8, available);

    std::size_t tagAt = at;
    if (tagAt + 8 > end || (std::memcmp(stream + tagAt, "Xing", 4) != 0 && std::memcmp(stream + tagAt, "Info", 4) != 0)) {
        const std::size_t fixedOffset = mpeg1 ? (mono ? 21u : 36u) : (mono ? 13u : 21u);
        tagAt = 0;
        if (fixedOffset + 8 <= end && (std::memcmp(stream + fixedOffset, "Xing", 4) == 0 || std::memcmp(stream + fixedOffset, "Info", 4) == 0)) {
            tagAt = fixedOffset;
        }
    }

    if (tagAt != 0) {
        const std::uint8_t flags = stream[tagAt + 7];
        std::size_t field = tagAt + 8;
        std::uint32_t frames = 0;
        if (flags & 0x01u) {
            if (field + 4 > end) return;
            frames = ReadBigEndian(stream + field, 4);
            frame->num_frames = frames;
            field += 4;
        }
        if (flags & 0x02u) {
            field += 4;
            if (field > end) return;
        }
        if (flags & 0x04u) {
            field += 100;
            if (field > end) return;
        }
        if (flags & 0x08u) {
            field += 4;
            if (field > end) return;
        }
        if (field + 0x18 <= end && std::memcmp(stream + field, "LAME", 4) == 0) {
            const std::uint32_t delay = (std::uint32_t{stream[field + 0x15]} << 4u) | (stream[field + 0x16] >> 4u);
            const std::uint32_t padding = ((std::uint32_t{stream[field + 0x16]} & 0x0Fu) << 8u) | stream[field + 0x17];
            if ((flags & 0x01u) && frames > 0) {
                const std::uint64_t samples = std::uint64_t{frames} * frame->samples_per_channel;
                const std::uint64_t trimmed = std::uint64_t{delay} + padding;
                const std::uint64_t total = samples > trimmed ? samples - trimmed : 0;
                frame->total_samples = static_cast<std::uint32_t>(std::min<std::uint64_t>(total, UINT32_MAX));
            }
            frame->encoder_delay = frame->samples_per_channel + delay + 529;
            frame->ofl_type = MP3_OFL_LAME;
        }
        return;
    }

    const std::size_t vbriAt = end >= 40 && std::memcmp(stream + 36, "VBRI", 4) == 0 ? 36 : at;
    if (vbriAt + 26 <= end && std::memcmp(stream + vbriAt, "VBRI", 4) == 0) {
        frame->encoder_delay = ReadBigEndian(stream + vbriAt + 6, 2);
        frame->ofl_type = MP3_OFL_VBRI;
        if (parseOfl == 1 && frame->frame_size <= streamSize) {
            AjmDecMp3ParseFrame next{};
            if (ParseMp3Header(stream + frame->frame_size, streamSize - static_cast<std::uint32_t>(frame->frame_size), 2, &next) == 0 && next.ofl_type == MP3_OFL_FGH) {
                frame->encoder_delay += next.encoder_delay;
                frame->total_samples = next.total_samples;
                frame->ofl_type = MP3_OFL_VBRI_AND_FGH;
            }
        }
        return;
    }

    while (at + 9 < end && stream[at] != 0xB4u) ++at;
    if (at + 9 >= end) return;
    std::uint8_t crc = 0xFF;
    for (std::size_t i = 0; i < 9; ++i) {
        for (int shift = 7; shift >= 0; --shift) {
            const bool same = ((crc >> 7u) & 1u) == ((stream[at + i] >> shift) & 1u);
            crc = static_cast<std::uint8_t>(same ? (crc << 1u) ^ 0x45u : crc << 1u);
        }
    }
    if (stream[at + 9] == crc) {
        frame->encoder_delay = ReadBigEndian(stream + at + 1, 2);
        frame->total_samples = ReadBigEndian(stream + at + 3, 4);
        frame->ofl_type = MP3_OFL_FGH;
    }
}

int ParseMp3Header(const std::uint8_t* stream, std::uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame) {
    if (!stream || streamSize < 4 || !frame) return SCE_AJM_ERROR_INVALID_PARAMETER;
    static constexpr std::uint32_t sampleRates[4][4] = {{11025, 12000, 8000, 0}, {0, 0, 0, 0}, {22050, 24000, 16000, 0}, {44100, 48000, 32000, 0}};
    static constexpr std::uint32_t kbps[4][16] = {
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
    };
    const std::uint32_t header = (std::uint32_t{stream[0]} << 24u) | (std::uint32_t{stream[1]} << 16u) | (std::uint32_t{stream[2]} << 8u) | stream[3];
    const std::uint32_t version = (header >> 19u) & 3u;
    const std::uint32_t layer = (header >> 17u) & 3u;
    const std::uint32_t sampleRate = sampleRates[version][(header >> 10u) & 3u];
    const std::uint32_t bitrate = kbps[version][(header >> 12u) & 15u] * 1000u;
    const bool valid = (header >> 21u) == 0x7FFu && layer != 0 && sampleRate != 0 && bitrate != 0;
    AJM_TRACE("[ajm] parse mp3 frame %02x %02x %02x %02x (%u bytes, parse ofl %d) -> %s, %u Hz, %u bps\n", stream[0], stream[1], stream[2], stream[3], streamSize, parseOfl, valid ? "ok" : "invalid", sampleRate, bitrate);
    if (!valid) return SCE_AJM_ERROR_INVALID_PARAMETER;
    if (parseOfl && layer != 1u) return SCE_AJM_ERROR_INVALID_PARAMETER;
    const bool mpeg1 = version == 3;
    frame->frame_size = (mpeg1 ? 144u : 72u) * bitrate / sampleRate + ((header >> 9u) & 1u);
    frame->num_channels = ((header >> 6u) & 3u) == 3u ? 1u : 2u;
    frame->samples_per_channel = mpeg1 ? 1152u : 576u;
    frame->bitrate = bitrate;
    frame->sample_rate = sampleRate;
    frame->encoder_delay = 0;
    frame->num_frames = 0;
    frame->total_samples = 0;
    frame->ofl_type = MP3_OFL_NONE;
    if (parseOfl) ParseMp3Ofl(stream, streamSize, header, frame, parseOfl);
    return 0;
}

void OpenMp3(Instance& instance) {
    static const bool quiet = (av_log_set_level(TraceEnabled() ? AV_LOG_WARNING : AV_LOG_QUIET), true);
    (void)quiet;
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_MP3);
    if (instance.mp3) avcodec_free_context(&instance.mp3);
    instance.mp3 = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (!instance.mp3 || avcodec_open2(instance.mp3, codec, nullptr) < 0) throw std::runtime_error("AJM: cannot open the FFmpeg MP3 decoder");
}

void RunMp3(Instance& instance, const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    const JobInput input(job, inputs);
    PcmOutputs pcmOutputs(outputs, job.outputCount);
    const auto encoding = PcmEncoding(instance);
    if (encoding > 2) throw std::runtime_error("AJM MP3: PCM encoding " + std::to_string(encoding) + " is not implemented");
    const std::size_t sampleBytes = encoding == 0 ? sizeof(std::int16_t) : sizeof(std::int32_t);
    std::unique_ptr<AVPacket, void (*)(AVPacket*)> packet(av_packet_alloc(), [](AVPacket* p) { av_packet_free(&p); });
    std::unique_ptr<AVFrame, void (*)(AVFrame*)> decoded(av_frame_alloc(), [](AVFrame* f) { av_frame_free(&f); });
    if (!packet || !decoded) throw std::bad_alloc();
    std::vector<std::uint8_t> pcm;

    std::int32_t result = 0;
    std::size_t consumed = 0;
    std::uint32_t frames = 0;
    if (Resampling(instance)) ResamplerProduce(instance, pcmOutputs, instance.mp3Channels, encoding, sampleBytes);
    while (consumed < input.size()) {
        Mp3Frame frame{};
        if (!ParseMp3Frame(input.data() + consumed, input.size() - consumed, frame)) {
            result |= AJM_RESULT_INVALID_DATA;
            break;
        }
        if (input.size() - consumed < frame.bytes) {
            result |= AJM_RESULT_PARTIAL_INPUT;
            break;
        }
        if (pcmOutputs.Room() < (Resampling(instance) ? 1 : frame.samples) * frame.channels * sampleBytes) {
            if (frames == 0 && pcmOutputs.produced == 0) result |= AJM_RESULT_NOT_ENOUGH_ROOM;
            break;
        }
        packet->data = const_cast<std::uint8_t*>(input.data() + consumed);
        packet->size = static_cast<int>(frame.bytes);
        if (avcodec_send_packet(instance.mp3, packet.get()) < 0) {
            result |= AJM_RESULT_INVALID_DATA;
            break;
        }
        consumed += frame.bytes;
        ++frames;
        instance.mp3Channels = frame.channels;
        instance.mp3SampleRate = frame.sampleRate;
        instance.mp3Bitrate = frame.bitrate;
        int status = 0;
        while ((status = avcodec_receive_frame(instance.mp3, decoded.get())) == 0) {
            const auto format = static_cast<AVSampleFormat>(decoded->format);
            if (format != AV_SAMPLE_FMT_FLTP && format != AV_SAMPLE_FMT_S16P) throw std::runtime_error("AJM MP3: FFmpeg sample format " + std::to_string(decoded->format) + " is not converted");
            const auto channels = static_cast<std::size_t>(decoded->ch_layout.nb_channels);
            const auto samples = static_cast<std::size_t>(decoded->nb_samples);
            pcm.resize(samples * channels * sampleBytes);
            for (std::size_t sample = 0; sample < samples; ++sample) {
                for (std::size_t channel = 0; channel < channels; ++channel) {
                    const float value = format == AV_SAMPLE_FMT_FLTP ? reinterpret_cast<const float*>(decoded->extended_data[channel])[sample]
                                                                     : reinterpret_cast<const std::int16_t*>(decoded->extended_data[channel])[sample] / 32768.0f;
                    auto* out = pcm.data() + (sample * channels + channel) * sampleBytes;
                    if (encoding == 0) {
                        const auto converted = static_cast<std::int16_t>(std::clamp(std::lrint(value * 32768.0), -32768L, 32767L));
                        std::memcpy(out, &converted, sizeof(converted));
                    } else if (encoding == 1) {
                        const auto converted = static_cast<std::int32_t>(std::clamp(std::llrint(value * 2147483648.0), -2147483648LL, 2147483647LL));
                        std::memcpy(out, &converted, sizeof(converted));
                    } else {
                        std::memcpy(out, &value, sizeof(value));
                    }
                }
            }
            EmitFrame(instance, pcmOutputs, pcm.data(), samples, channels, sampleBytes);
            av_frame_unref(decoded.get());
        }
        if (status != AVERROR(EAGAIN)) {
            result |= AJM_RESULT_INVALID_DATA;
            break;
        }
        if ((job.flags & RUN_MULTIPLE_FRAMES) == 0) break;
    }
    if (consumed == input.size() && frames == 0 && pcmOutputs.produced == 0 && result == 0) result |= AJM_RESULT_PARTIAL_INPUT;

    AJM_TRACE("[ajm] instance %u mp3 run flags 0x%llx: %zu input bytes, %zu output bytes -> result 0x%x, %u frames, consumed %zu, produced %zu, %u channels, %u Hz, %u bps, total samples %llu\n", job.instance, static_cast<unsigned long long>(job.flags), input.size(), pcmOutputs.capacity, static_cast<unsigned>(result), frames, consumed, pcmOutputs.produced,
              instance.mp3Channels, instance.mp3SampleRate, instance.mp3Bitrate, static_cast<unsigned long long>(instance.totalDecodedSamples));
    WriteRunSideband(job, instance, result, consumed, pcmOutputs.produced, frames, CurrentFormat(instance));
}
