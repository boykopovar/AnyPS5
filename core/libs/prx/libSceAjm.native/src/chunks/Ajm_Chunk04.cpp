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
bool AacLayoutImplemented(const AVChannelLayout& layout) {
    const AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
    const AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    const AVChannelLayout three = AV_CHANNEL_LAYOUT_SURROUND;
    const AVChannelLayout four = AV_CHANNEL_LAYOUT_4POINT0;
    const AVChannelLayout five = AV_CHANNEL_LAYOUT_5POINT0_BACK;
    const AVChannelLayout surround = AV_CHANNEL_LAYOUT_5POINT1_BACK;
    return av_channel_layout_compare(&layout, &mono) == 0 || av_channel_layout_compare(&layout, &stereo) == 0 || av_channel_layout_compare(&layout, &three) == 0 || av_channel_layout_compare(&layout, &four) == 0 || av_channel_layout_compare(&layout, &five) == 0 || av_channel_layout_compare(&layout, &surround) == 0;
}

std::size_t AdtsFrameBytes(const Instance& instance, const std::uint8_t* data) {
    AdtsHeader header{};
    if (!ParseAdtsHeader(data, header)) throw std::runtime_error("AJM M4AAC: the input at the consumed offset is not an ADTS frame header (resynchronization is not implemented)");
    if (header.profile != 1) throw std::runtime_error("AJM M4AAC: ADTS profile " + std::to_string(header.profile) + " is not implemented (only AAC LC)");
    if (header.rawBlocks != 0) throw std::runtime_error("AJM M4AAC: ADTS frames with more than one raw data block are not implemented");
    if (header.frequencyIndex >= 12) throw std::runtime_error("AJM M4AAC: ADTS sampling frequency index " + std::to_string(header.frequencyIndex) + " is not implemented");
    if (header.channelConfig < 1 || header.channelConfig > 6) throw std::runtime_error("AJM M4AAC: ADTS channel configuration " + std::to_string(header.channelConfig) + " is not implemented (only 1 through 6)");
    const std::uint32_t limit = InstanceChannels(instance);
    if (limit != 0 && header.channelConfig > limit) throw std::runtime_error("AJM M4AAC: an ADTS stream of " + std::to_string(header.channelConfig) + " channels on an instance created for " + std::to_string(limit) + " is not implemented");
    return header.frameBytes;
}

void RunAac(Instance& instance, const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    const bool raw = instance.aacConfig == M4AAC_CONFIG_RAW;
    if (raw && (job.flags & RUN_MULTIPLE_FRAMES)) throw std::runtime_error("AJM M4AAC: RUN_MULTIPLE_FRAMES on raw input is not implemented (a raw access unit carries no length)");
    const JobInput input(job, inputs);
    PcmOutputs pcmOutputs(outputs, job.outputCount);
    const auto encoding = PcmEncoding(instance);
    if (encoding > 2) throw std::runtime_error("AJM M4AAC: PCM encoding " + std::to_string(encoding) + " is not implemented");
    if (job.flags & SIDEBAND_FORMAT) {
        static_cast<void>(CurrentFormat(instance));
        if (raw && !input.empty() && (AacRawChannels(instance) == 3 || AacRawChannels(instance) == 5)) NotImplemented_nid_no_patch("AJM M4AAC format with an unknown three- or five-channel speaker mask");
        for (std::size_t offset = 0; !raw && input.size() - offset >= ADTS_HEADER_BYTES;) {
            AdtsHeader header{};
            if (!ParseAdtsHeader(input.data() + offset, header) || header.frameBytes > input.size() - offset) break;
            if (header.channelConfig == 3 || header.channelConfig == 5) NotImplemented_nid_no_patch("AJM M4AAC format with an unknown three- or five-channel speaker mask");
            if ((job.flags & RUN_MULTIPLE_FRAMES) == 0) break;
            offset += header.frameBytes;
        }
    }
    const std::size_t sampleBytes = encoding == 0 ? sizeof(std::int16_t) : sizeof(std::int32_t);
    std::unique_ptr<AVPacket, void (*)(AVPacket*)> packet(av_packet_alloc(), [](AVPacket* p) { av_packet_free(&p); });
    std::unique_ptr<AVFrame, void (*)(AVFrame*)> decoded(av_frame_alloc(), [](AVFrame* f) { av_frame_free(&f); });
    if (!packet || !decoded) throw std::bad_alloc();
    std::vector<std::uint8_t> bitstream;
    std::vector<std::uint8_t> pcm;

    std::int32_t result = 0;
    std::size_t consumed = 0;
    std::uint32_t frames = 0;
    if (Resampling(instance)) ResamplerProduce(instance, pcmOutputs, instance.aacChannels, encoding, sampleBytes);
    while (consumed < input.size()) {
        const std::uint8_t* data = input.data() + consumed;
        const std::size_t available = input.size() - consumed;
        if (!raw && available < ADTS_HEADER_BYTES) {
            result |= AJM_RESULT_PARTIAL_INPUT;
            break;
        }
        const std::size_t frameBytes = raw ? available : AdtsFrameBytes(instance, data);
        if (available < frameBytes) {
            result |= AJM_RESULT_PARTIAL_INPUT;
            break;
        }
        const std::size_t frameChannelBytes = instance.aacChannels * sampleBytes;
        if (pcmOutputs.Room() < (Resampling(instance) ? frameChannelBytes : EmittedSamples(instance, instance.aacFrameSamples) * frameChannelBytes)) {
            if (frames == 0 && pcmOutputs.produced == 0) result |= AJM_RESULT_NOT_ENOUGH_ROOM;
            break;
        }
        bitstream.assign(data, data + frameBytes);
        bitstream.resize(frameBytes + AV_INPUT_BUFFER_PADDING_SIZE, 0);
        packet->data = bitstream.data();
        packet->size = static_cast<int>(frameBytes);
        int status = avcodec_send_packet(instance.aac, packet.get());
        std::size_t received = 0;
        while (status >= 0 && (status = avcodec_receive_frame(instance.aac, decoded.get())) == 0) {
            if (++received > 1) throw std::runtime_error("AJM M4AAC: one input frame decoded to more than one output frame (raw input holding several access units is not implemented)");
            const auto format = static_cast<AVSampleFormat>(decoded->format);
            if (format != AV_SAMPLE_FMT_FLTP) throw std::runtime_error("AJM M4AAC: FFmpeg sample format " + std::to_string(decoded->format) + " is not converted");
            if (!AacLayoutImplemented(decoded->ch_layout)) throw std::runtime_error("AJM M4AAC: decoded channel layout is not implemented");
            const bool heaac = instance.aac->profile == AV_PROFILE_AAC_HE || instance.aac->profile == AV_PROFILE_AAC_HE_V2;
            if (heaac && (instance.flags & M4AAC_ENABLE_SBR_DECODE) == 0) throw std::runtime_error("AJM M4AAC: an HE-AAC (SBR or PS) stream on an instance without ENABLE_SBR_DECODE is not implemented");
            const auto channels = static_cast<std::size_t>(decoded->ch_layout.nb_channels);
            const auto samples = static_cast<std::size_t>(decoded->nb_samples);
            instance.aacChannels = static_cast<std::uint32_t>(channels);
            instance.aacSampleRate = static_cast<std::uint32_t>(decoded->sample_rate);
            instance.aacFrameSamples = static_cast<std::uint32_t>(samples);
            instance.aacHeaac = heaac;
            instance.aacBitrate = samples == 0 ? 0 : static_cast<std::uint32_t>(std::uint64_t{frameBytes} * 8u * instance.aacSampleRate / samples);
            if (instance.aacDelayFrames == 0) {
                pcm.resize(samples * channels * sampleBytes);
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    for (std::size_t channel = 0; channel < channels; ++channel) StorePcm(reinterpret_cast<const float*>(decoded->extended_data[channel])[sample], pcm.data() + (sample * channels + channel) * sampleBytes, encoding);
                }
                if (!Resampling(instance) && EmittedSamples(instance, samples) * channels * sampleBytes > pcmOutputs.Room()) throw std::runtime_error("AJM M4AAC: a decoded frame of " + std::to_string(samples) + " samples does not fit the " + std::to_string(pcmOutputs.Room()) + " output bytes left (a change of the frame length is not implemented)");
                EmitFrame(instance, pcmOutputs, pcm.data(), samples, channels, sampleBytes);
            }
            av_frame_unref(decoded.get());
        }
        if (status != AVERROR(EAGAIN)) {
            avcodec_flush_buffers(instance.aac);
            result |= AJM_RESULT_CODEC_ERROR | AJM_RESULT_FATAL;
            break;
        }
        if (instance.aacDelayFrames > 0) --instance.aacDelayFrames;
        consumed += frameBytes;
        ++frames;
        if ((job.flags & RUN_MULTIPLE_FRAMES) == 0) break;
    }
    if (consumed == input.size() && frames == 0 && pcmOutputs.produced == 0 && result == 0) result |= AJM_RESULT_PARTIAL_INPUT;

    AJM_TRACE("[ajm] instance %u m4aac run flags 0x%llx: %zu input bytes, %zu output bytes -> result 0x%x, %u frames, consumed %zu, produced %zu, %u channels, %u Hz, %u bps, heaac %d, total samples %llu\n", job.instance, static_cast<unsigned long long>(job.flags), input.size(), pcmOutputs.capacity, static_cast<unsigned>(result), frames, consumed, pcmOutputs.produced,
              instance.aacChannels, instance.aacSampleRate, instance.aacBitrate, instance.aacHeaac ? 1 : 0, static_cast<unsigned long long>(instance.totalDecodedSamples));
    WriteRunSideband(job, instance, result, consumed, pcmOutputs.produced, frames, (job.flags & SIDEBAND_FORMAT) ? CurrentFormat(instance) : SidebandFormat{});
}

void ClearContext(Instance& instance) {
    instance.totalDecodedSamples = 0;
    instance.segmentSamples = 0;
    instance.gapless.skippedSamples = 0;
    if (HasAt9Decoder(instance) && instance.initialized) ResetDecoder(instance);
    if (instance.mp3) avcodec_flush_buffers(instance.mp3);
    if (instance.opus) avcodec_flush_buffers(instance.opus);
    if (instance.aac) {
        avcodec_flush_buffers(instance.aac);
        instance.aacDelayFrames = AacDelayFrames(instance);
    }
    instance.opusPending.clear();
    instance.pcmResampled = false;
    ClearResampler(instance);
}

void CheckAacFlags(std::uint64_t flags) {
    if ((flags >> 32u) & ~((M4AAC_ENABLE_SBR_DECODE | M4AAC_ENABLE_NONDELAY_OUTPUT | M4AAC_SURROUND_INTERLEAVE_ORDERS) >> 32u)) NotImplemented_nid_no_patch("sceAjmInstanceCreate (M4AAC codec flags other than ENABLE_SBR_DECODE, ENABLE_NONDELAY_OUTPUT and the surround interleave orders)");
}

std::size_t ControlInitializeSize(std::uint32_t codec) {
    switch (codec) {
    case CODEC_MP3: return 0;
    case CODEC_AT9: return 8;
    case CODEC_M4AAC: return 8;
    case CODEC_OPUS: return 12;
    default: NotImplemented_nid_no_patch("sceAjmBatchJobControl (INITIALIZE for a codec other than MP3, ATRAC9, M4AAC and Opus)"); return 0;
    }
}

void Control(Instance& instance, const JobHeader& job, const AjmBuffer* inputs) {
    const auto* input = job.inputCount ? static_cast<const std::uint8_t*>(inputs[0].ptr) : nullptr;
    const std::size_t inputSize = job.inputCount ? inputs[0].size : 0;
    const std::size_t gaplessSize = (job.flags & SIDEBAND_GAPLESS_DECODE) ? sizeof(SidebandGaplessDecode) : 0;
    const std::size_t initializeSize = (job.flags & CONTROL_INITIALIZE) ? ControlInitializeSize(instance.codec) : 0;
    if (inputSize != gaplessSize + initializeSize) NotImplemented_nid_no_patch("sceAjmBatchJobControl (sideband input size other than the gapless decode and the codec's initialize parameters)");
    std::int32_t result = 0;
    if (job.flags & CONTROL_RESET) ClearContext(instance);
    if (job.flags & CONTROL_INITIALIZE) result = InitializeInstance(instance, input + gaplessSize, initializeSize);
    if (gaplessSize) {
        SidebandGaplessDecode gapless{};
        std::memcpy(&gapless, input, sizeof(gapless));
        instance.gapless.totalSamples = gapless.totalSamples;
        instance.gapless.skipSamples = gapless.skipSamples;
    }
    AJM_TRACE("[ajm] instance %u control flags 0x%llx: %zu sideband input bytes -> result 0x%x, gapless total %u skip %u skipped %u\n", job.instance, static_cast<unsigned long long>(job.flags), inputSize, static_cast<unsigned>(result), instance.gapless.totalSamples, instance.gapless.skipSamples, instance.gapless.skippedSamples);
    WriteResult(job.sideband, job.sidebandSize, result);
}

std::size_t HeldSamples(const Instance& instance) {
    const std::size_t channels = instance.codec == CODEC_PCM ? instance.pcmChannels : instance.codec == CODEC_OPUS ? instance.opusChannels : instance.codec == CODEC_MP3 ? instance.mp3Channels : instance.codec == CODEC_M4AAC ? instance.aacChannels : static_cast<std::size_t>(std::max(0, instance.info.channels));
    if (Resampling(instance)) return ResamplerHeld(instance, channels);
    const std::size_t sampleBytes = PcmEncoding(instance) == 0 ? sizeof(std::int16_t) : sizeof(std::int32_t);
    return channels == 0 ? 0 : instance.opusPending.size() / (channels * sampleBytes);
}

void Execute(const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    if (job.kind == JobKind::GetStatistics) {
        AJM_TRACE("[ajm] statistics job: sideband %llu bytes\n", static_cast<unsigned long long>(job.sidebandSize));
        if (job.sideband && job.sidebandSize) std::memset(job.sideband, 0, std::min<std::uint64_t>(job.sidebandSize, 24));
        return;
    }
    std::lock_guard lock(g_lock);
    auto* instance = Find(job.instance);
    if (!instance) {
        AJM_TRACE("[ajm] job kind %u on unknown instance %u\n", static_cast<unsigned>(job.kind), job.instance);
        WriteResult(job.sideband, job.sidebandSize, AJM_RESULT_INVALID_PARAMETER);
        return;
    }
    switch (job.kind) {
    case JobKind::Initialize: {
        const std::int32_t result = InitializeInstance(*instance, job.parameters, job.parameterSize);
        AJM_TRACE("[ajm] instance %u initialize: %llu parameter bytes (%02x %02x %02x %02x %02x %02x %02x %02x), sideband %llu bytes -> result 0x%x, %d channels, %d Hz, superframe %d bytes (%d frames of %d samples)\n", job.instance, static_cast<unsigned long long>(job.parameterSize), job.parameters[0], job.parameters[1], job.parameters[2], job.parameters[3], job.parameters[4], job.parameters[5], job.parameters[6], job.parameters[7], static_cast<unsigned long long>(job.sidebandSize), static_cast<unsigned>(result), instance->info.channels, instance->info.samplingRate, instance->info.superframeSize, instance->info.framesInSuperframe, instance->info.frameSamples);
        WriteResult(job.sideband, job.sidebandSize, result);
        break;
    }
    case JobKind::ClearContext:
        AJM_TRACE("[ajm] instance %u clear context (sideband %llu bytes)\n", job.instance, static_cast<unsigned long long>(job.sidebandSize));
        ClearContext(*instance);
        WriteResult(job.sideband, job.sidebandSize, 0);
        break;
    case JobKind::SetGaplessDecode: {
        SidebandGaplessDecode gapless{};
        std::memcpy(&gapless, job.parameters, sizeof(gapless));
        AJM_TRACE("[ajm] instance %u set gapless decode: total %u, skip %u, reset %llu (sideband %llu bytes)\n", job.instance, gapless.totalSamples, gapless.skipSamples, static_cast<unsigned long long>(job.flags), static_cast<unsigned long long>(job.sidebandSize));
        instance->gapless.totalSamples = gapless.totalSamples;
        instance->gapless.skipSamples = gapless.skipSamples;
        if (job.flags) {
            instance->gapless.skippedSamples = 0;
            instance->segmentSamples = 0;
            instance->totalDecodedSamples = 0;
        }
        WriteResult(job.sideband, job.sidebandSize, 0);
        break;
    }
    case JobKind::Control:
        Control(*instance, job, inputs);
        break;
    case JobKind::SetResampleParameters: {
        float params[2] = {1.0f, 0.0f};
        std::memcpy(params, job.parameters, std::min<std::size_t>(job.parameterSize, sizeof(params)));
        instance->resampler.ratio = params[0];
        instance->resampler.change = job.parameterSize >= sizeof(float) * 2 ? params[1] : 0.0f;
        AJM_TRACE("[ajm] instance %u set resample ratio %f change %f\n", job.instance, static_cast<double>(instance->resampler.ratio), static_cast<double>(instance->resampler.change));
        WriteResult(job.sideband, job.sidebandSize, 0);
        break;
    }
    case JobKind::GetResampleInfo: {
        const SidebandResampleInfo resample{instance->resampler.ratio, static_cast<std::int32_t>(HeldSamples(*instance)), {}};
        WriteResult(job.sideband, job.sidebandSize, 0);
        if (job.sideband && job.sidebandSize >= sizeof(SidebandResult) + sizeof(resample)) std::memcpy(static_cast<std::uint8_t*>(job.sideband) + sizeof(SidebandResult), &resample, sizeof(resample));
        break;
    }
    case JobKind::Run:
        if (!instance->initialized) {
            AJM_TRACE("[ajm] instance %u run before initialize\n", job.instance);
            WriteResult(job.sideband, job.sidebandSize, AJM_RESULT_NOT_INITIALIZED);
        } else if (instance->codec != CODEC_AT9 && instance->codec != CODEC_MP3 && instance->codec != CODEC_M4AAC && instance->codec != CODEC_PCM && instance->codec != CODEC_OPUS) {
            throw std::runtime_error("AJM: decoding codec " + std::to_string(instance->codec) + " is not implemented");
        } else if ((job.flags & RUN_GET_CODEC_INFO) && (job.flags & RUN_MULTIPLE_FRAMES)) {
            NotImplemented_nid_no_patch("AJM run job with both RUN_GET_CODEC_INFO and RUN_MULTIPLE_FRAMES (sideband order)");
        } else if ((job.flags & RUN_GET_CODEC_INFO) && instance->codec != CODEC_AT9 && instance->codec != CODEC_M4AAC) {
            NotImplemented_nid_no_patch("AJM RUN_GET_CODEC_INFO for a codec other than ATRAC9 and M4AAC");
        } else if (job.inputCount == 0 && job.outputCount == 0) {
            AJM_TRACE("[ajm] instance %u run flags 0x%llx without buffers, sideband %llu bytes\n", job.instance, static_cast<unsigned long long>(job.flags), static_cast<unsigned long long>(job.sidebandSize));
            WriteRunSideband(job, *instance, 0, 0, 0, 0, (job.flags & SIDEBAND_FORMAT) ? CurrentFormat(*instance) : SidebandFormat{});
        } else if (instance->codec == CODEC_AT9) {
            RunAt9(*instance, job, inputs, outputs);
        } else if (instance->codec == CODEC_MP3) {
            RunMp3(*instance, job, inputs, outputs);
        } else if (instance->codec == CODEC_M4AAC) {
            RunAac(*instance, job, inputs, outputs);
        } else if (instance->codec == CODEC_PCM) {
            RunPcm(*instance, job, inputs, outputs);
        } else {
            RunOpus(*instance, job, inputs, outputs);
        }
        break;
    default:
        AJM_TRACE("[ajm] instance %u unknown job kind %u\n", job.instance, static_cast<unsigned>(job.kind));
        WriteResult(job.sideband, job.sidebandSize, AJM_RESULT_INVALID_PARAMETER);
        break;
    }
}

int RegisterBatch(std::uint32_t context, AjmBatchError* error, std::uint32_t* batch) {
    if (error) std::memset(error, 0, sizeof(*error));
    const auto id = g_nextBatch.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard lock(g_batchLock);
        g_batches.emplace(id, context);
    }
    *batch = id;
    return 0;
}

// The PS4 batch buffer (sceAjmBatchStartBuffer) is not private: the PS4 SDK builds it with inline code
// in the title, so it has the console's layout, taken from shadPS4's reimplementation of the PS4
// library. It is a run of chunks, each starting with a 32-bit word that holds the chunk kind in bits
// 0-5 and a payload in bits 6-25. A job chunk has the instance as payload and, in a second word, the
// byte count of the chunks after it that describe the job; an inline chunk holds that many bytes of data
// a job's control buffer may point to. A job's flags chunk has flags bits 32-51 as payload and bits 0-31
// in its second word; its buffer chunks have a 32-bit size and a 64-bit address.
enum class ConsoleChunk : std::uint32_t {
    Job = 0,
    InputRunBuffer = 1,
    InputControlBuffer = 2,
    ControlFlags = 3,
    RunFlags = 4,
    ReturnAddress = 6,
    InlineBuffer = 7,
    OutputRunBuffer = 17,
    OutputControlBuffer = 18,
};

constexpr std::uint32_t CONSOLE_STATISTICS_INSTANCE = 0x80000;
constexpr std::uint64_t CONSOLE_FLAGS_VERSION_AND_CODEC = 0x7FF;

// The low header word as the PS4's libSceAjm builders write it (a trace of 32 545 batches a Wwise title
// started on a PS4): bits 0-3 the kind, bit 4 the output direction of a buffer chunk
// (0x11 output run, 0x12 output control), bits 6-25 the job's instance or the flags' bits 32-51. The
// builders leave bits 5, 26, 27 and 31 as they were, and a title that reuses its batch memory (Wwise
// does) carries the previous batch's bytes there: a 6-bit kind read job headers as 16, 32 or 48 and run
// flags as 20, 36 or 52, and a 26-bit payload read flags bit 52 set.
ConsoleChunk ConsoleChunkKind(std::uint32_t word) {
    const std::uint32_t kind = word & 0xFu;
    return static_cast<ConsoleChunk>(kind == 1 || kind == 2 ? kind | (word & 0x10u) : kind);
}

std::uint32_t ConsoleChunkPayload(std::uint32_t word) {
    return (word >> 6) & 0xFFFFFu;
}

constexpr std::uint64_t CONSOLE_RUN_FLAGS = 0x0000E00000001FFF;
constexpr std::uint64_t CONSOLE_CONTROL_FLAGS = 0x000060000000E7FF;

std::uint8_t* KeepConsoleWords(std::uint8_t* at, std::uint32_t keep, std::uint32_t low, std::uint32_t word) {
    std::uint32_t words[2];
    std::memcpy(words, at, sizeof(words));
    words[0] = (words[0] & keep) | low;
    words[1] = word;
    std::memcpy(at, words, sizeof(words));
    return at + sizeof(words);
}

std::uint8_t* KeepConsoleBuffer(std::uint8_t* at, ConsoleChunk kind, const void* address, std::size_t size) {
    at = KeepConsoleWords(at, 0xFFFFFFE0u, static_cast<std::uint32_t>(kind), static_cast<std::uint32_t>(size));
    std::memcpy(at, &address, sizeof(address));
    return at + sizeof(address);
}

std::uint8_t* KeepConsoleFlags(std::uint8_t* at, ConsoleChunk kind, std::uint64_t flags) {
    return KeepConsoleWords(at, 0xFC000000u, static_cast<std::uint32_t>(kind) | ((static_cast<std::uint32_t>(flags >> 32) & 0xFFFFFu) << 6), static_cast<std::uint32_t>(flags));
}

std::uint8_t* BeginConsoleJob(std::uint8_t* job, const void* returnAddress) {
    std::uint8_t* at = job + 2 * sizeof(std::uint32_t);
    return returnAddress ? KeepConsoleBuffer(at, ConsoleChunk::ReturnAddress, returnAddress, 0) : at;
}

void* EndConsoleJob(std::uint8_t* job, std::uint8_t* end, std::uint32_t instance) {
    KeepConsoleWords(job, 0xFC000000u, (instance & 0xFFFFFu) << 6, static_cast<std::uint32_t>(end - job) - 2 * sizeof(std::uint32_t));
    return end;
}

constexpr std::size_t CONSOLE_WORDS_BYTES = 2 * sizeof(std::uint32_t);
constexpr std::size_t CONSOLE_BUFFER_BYTES = CONSOLE_WORDS_BYTES + sizeof(const void*);

void CopyBatchIn(void* destination, const void* source, std::size_t bytes, const char* message) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::runtime_error(message);
}

void CopyBatchOut(void* destination, const void* source, std::size_t bytes, const char* message) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) throw std::runtime_error(message);
}

std::vector<std::uint8_t> LoadConsoleJob(const void* position, std::size_t bytes) {
    std::vector<std::uint8_t> job(bytes);
    CopyBatchIn(job.data(), position, bytes, "AJM batch job buffer is unreadable");
    return job;
}

void* StoreConsoleJob(void* position, const std::vector<std::uint8_t>& job) {
    CopyBatchOut(position, job.data(), job.size(), "AJM batch job buffer is unwritable");
    return static_cast<std::uint8_t*>(position) + job.size();
}

std::vector<AjmBuffer> LoadBufferList(const AjmBuffer* buffers, std::size_t count, const char* message) {
    if (count > SIZE_MAX / sizeof(AjmBuffer)) throw std::runtime_error(message);
    std::vector<AjmBuffer> list(count);
    CopyBatchIn(list.data(), buffers, count * sizeof(AjmBuffer), message);
    return list;
}

void* BuildRunJob(void* position, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size, void* return_address) {
    const std::vector<AjmBuffer> inputs = LoadBufferList(input_buffers, input_buffers_num, "AJM batch input buffer list is unreadable");
    const std::vector<AjmBuffer> outputs = LoadBufferList(output_buffers, output_buffers_num, "AJM batch output buffer list is unreadable");
    const std::size_t fixedBytes = 2 * CONSOLE_WORDS_BYTES + (return_address ? CONSOLE_BUFFER_BYTES : 0) + CONSOLE_BUFFER_BYTES;
    std::vector<std::uint8_t> job = LoadConsoleJob(position, fixedBytes + (inputs.size() + outputs.size()) * CONSOLE_BUFFER_BYTES);
    auto* at = BeginConsoleJob(job.data(), return_address);
    for (const AjmBuffer& input : inputs) at = KeepConsoleBuffer(at, ConsoleChunk::InputRunBuffer, input.ptr, input.size);
    at = KeepConsoleFlags(at, ConsoleChunk::RunFlags, flags & CONSOLE_RUN_FLAGS);
    for (const AjmBuffer& output : outputs) at = KeepConsoleBuffer(at, ConsoleChunk::OutputRunBuffer, output.ptr, output.size);
    at = KeepConsoleBuffer(at, ConsoleChunk::OutputControlBuffer, sideband_output, sideband_output_size);
    EndConsoleJob(job.data(), at, instance);
    return StoreConsoleJob(position, job);
}

struct ConsoleJob {
    JobHeader header;
    std::vector<AjmBuffer> buffers;
};

// What sceAjmBatchStartBuffer reports in the AjmBatchError when it refuses a batch: the error, the job
// chunk, the offending chunk's offset in the job and the job's return address, as a PS4 fills them for a
// buffer chunk with a null address and a nonzero size (INVALID_ADDRESS, nothing of the batch run). The
// other refusals here report MALFORMED_BATCH the same way; they were not measured.
struct ConsoleRefusal {
    std::int32_t code = SCE_AJM_ERROR_MALFORMED_BATCH;
    const std::uint8_t* job = nullptr;
    std::uint32_t offset = 0;
    const void* returnAddress = nullptr;
};

// Reads one console job: the chunks between the job header at `job` and `end`. A buffer chunk of size
// zero is dropped whatever its address, as a PS4 does. Returns false with the refusal filled for a null
// address with a nonzero size, for chunks that overrun the job and for a job without flags.
bool ReadConsoleJob(std::uint32_t instance, const std::uint8_t* job, const std::uint8_t* end, ConsoleJob& out, ConsoleRefusal& refusal) {
    out.header = JobHeader{};
    out.header.instance = instance;
    out.header.console = true;
    std::vector<AjmBuffer> inputs;
    std::vector<AjmBuffer> outputs;
    AjmBuffer controlInput{};
    AjmBuffer controlOutput{};
    ConsoleChunk flagsKind = ConsoleChunk::Job;
    refusal = ConsoleRefusal{};
    refusal.job = job;
    const std::uint8_t* at = job + 2 * sizeof(std::uint32_t);
    while (at < end) {
        refusal.offset = static_cast<std::uint32_t>(at - job);
        if (end - at < 8) return false;
        std::uint32_t words[2];
        std::memcpy(words, at, sizeof(words));
        const auto kind = ConsoleChunkKind(words[0]);
        if (kind == ConsoleChunk::ControlFlags || kind == ConsoleChunk::RunFlags) {
            if (flagsKind != ConsoleChunk::Job) return false;
            flagsKind = kind;
            out.header.flags = (std::uint64_t{ConsoleChunkPayload(words[0])} << 32) | words[1];
            at += sizeof(words);
            continue;
        }
        if (kind != ConsoleChunk::InputRunBuffer && kind != ConsoleChunk::InputControlBuffer && kind != ConsoleChunk::ReturnAddress && kind != ConsoleChunk::OutputRunBuffer && kind != ConsoleChunk::OutputControlBuffer) {
            NotImplemented_nid_no_patch(("sceAjmBatchStartBuffer (job chunk kind " + std::to_string(static_cast<unsigned>(kind)) + ")").c_str());
        }
        if (end - at < 16) return false;
        AjmBuffer buffer{nullptr, words[1]};
        std::memcpy(&buffer.ptr, at + sizeof(words), sizeof(buffer.ptr));
        at += 16;
        if (kind == ConsoleChunk::ReturnAddress) {
            refusal.returnAddress = buffer.ptr;
            continue;
        }
        if (buffer.size == 0) continue;
        if (!buffer.ptr) {
            refusal.code = SCE_AJM_ERROR_INVALID_ADDRESS;
            return false;
        }
        if (kind == ConsoleChunk::InputRunBuffer) inputs.push_back(buffer);
        if (kind == ConsoleChunk::OutputRunBuffer) outputs.push_back(buffer);
        if (kind == ConsoleChunk::InputControlBuffer) controlInput = buffer;
        if (kind == ConsoleChunk::OutputControlBuffer) controlOutput = buffer;
    }
    refusal.offset = static_cast<std::uint32_t>(end - job);
    if (flagsKind == ConsoleChunk::Job) return false;
    JobHeader& header = out.header;
    header.sideband = controlOutput.ptr;
    header.sidebandSize = controlOutput.size;
    if (instance == CONSOLE_STATISTICS_INSTANCE) {
        header.kind = JobKind::GetStatistics;
    } else if (flagsKind == ConsoleChunk::ControlFlags) {
        constexpr std::uint64_t implemented = CONSOLE_FLAGS_VERSION_AND_CODEC | CONTROL_RESET | CONTROL_INITIALIZE | SIDEBAND_GAPLESS_DECODE;
        if ((header.flags & ~implemented) != 0) NotImplemented_nid_no_patch("sceAjmBatchStartBuffer (control job flags other than RESET, INITIALIZE and SIDEBAND_GAPLESS_DECODE)");
        if (!inputs.empty() || !outputs.empty()) NotImplemented_nid_no_patch("sceAjmBatchStartBuffer (control job with run buffers)");
        header.kind = JobKind::Control;
        if (controlInput.size) inputs.push_back(controlInput);
    } else {
        if (controlInput.size) NotImplemented_nid_no_patch("sceAjmBatchStartBuffer (run job with an input control buffer)");
        header.kind = JobKind::Run;
    }
    header.inputCount = static_cast<std::uint32_t>(inputs.size());
    header.outputCount = static_cast<std::uint32_t>(outputs.size());
    AJM_TRACE("[ajm] console job: instance %u, %s flags 0x%llx, %u input and %u output buffers, control input %llu and output %llu bytes\n", instance, flagsKind == ConsoleChunk::ControlFlags ? "control" : "run", static_cast<unsigned long long>(header.flags), header.inputCount, header.outputCount, static_cast<unsigned long long>(controlInput.size), static_cast<unsigned long long>(controlOutput.size));
    out.buffers = std::move(inputs);
    out.buffers.insert(out.buffers.end(), outputs.begin(), outputs.end());
    return true;
}
}

extern "C" {

