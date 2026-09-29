#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "libatrac9.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/log.h>
}
#include <algorithm>
#include <atomic>
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

// The batch buffer and its job records are private to this library: the guest only reserves the
// memory and reads the "used bytes" field of AjmBatchInfo. Sideband results written back to guest
// memory follow the SDK layouts.
namespace {

constexpr int SCE_AJM_ERROR_INVALID_INSTANCE = static_cast<int>(0x80930003);
constexpr int SCE_AJM_ERROR_INVALID_PARAMETER = static_cast<int>(0x80930005);
constexpr int SCE_AJM_ERROR_OUT_OF_RESOURCES = static_cast<int>(0x80930007);

constexpr std::int32_t AJM_RESULT_NOT_INITIALIZED = 0x00000001;
constexpr std::int32_t AJM_RESULT_INVALID_DATA = 0x00000002;
constexpr std::int32_t AJM_RESULT_INVALID_PARAMETER = 0x00000004;
constexpr std::int32_t AJM_RESULT_PARTIAL_INPUT = 0x00000008;
constexpr std::int32_t AJM_RESULT_NOT_ENOUGH_ROOM = 0x00000010;
constexpr std::int32_t AJM_RESULT_CODEC_ERROR = 0x40000000;

constexpr std::uint32_t CODEC_MP3 = 0;
constexpr std::uint32_t CODEC_AT9 = 1;

// APS5_TRACE_AJM=1 logs every job the title submits and what the library writes back.
bool TraceEnabled() {
    static const bool enabled = std::getenv("APS5_TRACE_AJM") != nullptr;
    return enabled;
}

#define AJM_TRACE(...) \
    do { \
        if (TraceEnabled()) std::fprintf(stderr, __VA_ARGS__); \
    } while (0)

constexpr std::uint64_t RUN_MULTIPLE_FRAMES = 1ull << 12;
constexpr std::uint64_t SIDEBAND_GAPLESS_DECODE = 1ull << 45;
constexpr std::uint64_t SIDEBAND_FORMAT = 1ull << 46;
constexpr std::uint64_t SIDEBAND_STREAM = 1ull << 47;

struct SidebandResult {
    std::int32_t result;
    std::int32_t internalResult;
};

struct SidebandStream {
    std::int32_t inputConsumed;
    std::int32_t outputWritten;
    std::uint64_t totalDecodedSamples;
};

struct SidebandFormat {
    std::uint32_t numChannels;
    std::uint32_t channelMask;
    std::uint32_t sampleRate;
    std::uint32_t sampleFormat;
    std::uint32_t bitrate;
    std::uint32_t reserved;
};

struct SidebandGaplessDecode {
    std::uint32_t totalSamples;
    std::uint16_t skipSamples;
    std::uint16_t skippedSamples;
};

struct SidebandMultipleFrames {
    std::uint32_t numFrames;
    std::uint32_t reserved;
};

struct Instance {
    std::uint32_t codec = 0;
    std::uint64_t flags = 0;
    void* decoder = nullptr;
    Atrac9CodecInfo info{};
    bool initialized = false;
    std::uint32_t superframeRemaining = 0;
    std::uint32_t frameInSuperframe = 0;
    std::uint64_t totalDecodedSamples = 0;
    SidebandGaplessDecode gapless{};
    bool flagsReported = false;
    // MP3: the FFmpeg decoder (it keeps the bit reservoir between frames) and the last frame's shape.
    AVCodecContext* mp3 = nullptr;
    std::uint32_t mp3Channels = 0;
    std::uint32_t mp3SampleRate = 0;
    std::uint32_t mp3Bitrate = 0;

    ~Instance() {
        if (decoder) Atrac9ReleaseHandle(decoder);
        if (mp3) avcodec_free_context(&mp3);
    }
};

std::mutex g_lock;
std::map<std::uint32_t, std::unique_ptr<Instance>> g_instances;
std::atomic<std::uint32_t> g_nextContext{1};
std::atomic<std::uint32_t> g_nextInstance{1};
std::atomic<std::uint32_t> g_nextBatch{1};

enum class JobKind : std::uint32_t {
    Initialize = 1,
    ClearContext = 2,
    SetGaplessDecode = 3,
    Run = 4,
    GetStatistics = 5,
};

struct JobHeader {
    JobKind kind;
    std::uint32_t bytes;
    std::uint32_t instance;
    std::uint32_t reserved;
    std::uint64_t flags;
    void* sideband;
    std::uint64_t sidebandSize;
    std::uint64_t parameterSize;
    std::uint32_t inputCount;
    std::uint32_t outputCount;
    std::uint8_t parameters[16];
};

int Append(AjmBatchInfo* info, const JobHeader& header, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    if (!info || !info->p_buffer) return SCE_AJM_ERROR_INVALID_PARAMETER;
    const std::size_t bytes = sizeof(JobHeader) + (header.inputCount + header.outputCount) * sizeof(AjmBuffer);
    if (bytes > info->size - info->offset) return SCE_AJM_ERROR_OUT_OF_RESOURCES;
    auto* cursor = static_cast<std::uint8_t*>(info->p_buffer) + info->offset;
    JobHeader record = header;
    record.bytes = static_cast<std::uint32_t>(bytes);
    std::memcpy(cursor, &record, sizeof(record));
    cursor += sizeof(record);
    if (header.inputCount) std::memcpy(cursor, inputs, header.inputCount * sizeof(AjmBuffer));
    cursor += header.inputCount * sizeof(AjmBuffer);
    if (header.outputCount) std::memcpy(cursor, outputs, header.outputCount * sizeof(AjmBuffer));
    info->offset += bytes;
    return 0;
}

JobHeader MakeHeader(JobKind kind, std::uint32_t instance, void* sideband, std::uint64_t sidebandSize) {
    JobHeader header{};
    header.kind = kind;
    header.instance = instance;
    header.sideband = sideband;
    header.sidebandSize = sidebandSize;
    return header;
}

// The SDK's speaker masks (SCE_AJM_CHANNELMASK_*) for a decoded channel count.
std::uint32_t ChannelMask(std::size_t channels) {
    switch (channels) {
    case 1: return 0x4;
    case 2: return 0x3;
    case 4: return 0x33;
    case 6: return 0x3F;
    case 8: return 0x63F;
    default: return 0;
    }
}

void WriteResult(void* sideband, std::uint64_t size, std::int32_t result) {
    if (!sideband || size < sizeof(SidebandResult)) return;
    const SidebandResult value{result, 0};
    std::memcpy(sideband, &value, sizeof(value));
}

Instance* Find(std::uint32_t id) {
    const auto found = g_instances.find(id);
    return found == g_instances.end() ? nullptr : found->second.get();
}

// Replaces the instance's decoder with a fresh one for its configuration, at a superframe boundary. The
// decoder handle keeps its position within the superframe, which a re-initialization does not clear.
bool ResetDecoder(Instance& instance) {
    if (instance.decoder) Atrac9ReleaseHandle(instance.decoder);
    instance.decoder = Atrac9GetHandle();
    instance.superframeRemaining = 0;
    instance.frameInSuperframe = 0;
    unsigned char config[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(config, instance.info.configData, sizeof(config));
    return Atrac9InitDecoder(instance.decoder, config) == 0;
}

std::int32_t InitializeInstance(Instance& instance, const std::uint8_t* parameters, std::uint64_t size) {
    if (instance.codec != CODEC_AT9) {
        instance.initialized = true;
        return 0;
    }
    if (size < ATRAC9_CONFIG_DATA_SIZE) return AJM_RESULT_INVALID_PARAMETER;
    std::memcpy(instance.info.configData, parameters, ATRAC9_CONFIG_DATA_SIZE);
    if (!ResetDecoder(instance)) {
        instance.initialized = false;
        return AJM_RESULT_INVALID_PARAMETER;
    }
    Atrac9GetCodecInfo(instance.decoder, &instance.info);
    instance.initialized = true;
    instance.totalDecodedSamples = 0;
    instance.gapless = {};
    return 0;
}

// Titles may hand the decoder a whole .at9 file. Like the console's decoder, a RIFF/WAVE header at a
// frame boundary is skipped up to the data chunk's payload and counted as consumed input. Returns the
// payload offset, or 0 when the bytes do not start a complete RIFF header.
std::size_t RiffDataOffset(const std::uint8_t* data, std::size_t size) {
    if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) return 0;
    std::size_t cursor = 12;
    while (cursor + 8 <= size) {
        std::uint32_t chunkBytes = 0;
        std::memcpy(&chunkBytes, data + cursor + 4, sizeof(chunkBytes));
        if (std::memcmp(data + cursor, "data", 4) == 0) return cursor + 8;
        cursor += 8 + static_cast<std::size_t>(chunkBytes) + (chunkBytes & 1u);
    }
    return 0;
}

// Whether this is the instance's first run since it was (re)initialized; used to report its flags once.
bool consumedFirstRun(Instance& instance) {
    if (instance.flagsReported) return false;
    instance.flagsReported = true;
    return true;
}

std::vector<std::uint8_t> JoinInputs(const JobHeader& job, const AjmBuffer* inputs) {
    std::vector<std::uint8_t> input;
    for (std::uint32_t index = 0; index < job.inputCount; ++index) {
        const auto* data = static_cast<const std::uint8_t*>(inputs[index].ptr);
        input.insert(input.end(), data, data + inputs[index].size);
    }
    return input;
}

// The job's output buffers, filled in order.
struct PcmOutputs {
    const AjmBuffer* buffers;
    std::uint32_t count;
    std::size_t capacity = 0;
    std::size_t produced = 0;
    std::uint32_t index = 0;
    std::size_t offset = 0;

    PcmOutputs(const AjmBuffer* buffers, std::uint32_t count) : buffers(buffers), count(count) {
        for (std::uint32_t i = 0; i < count; ++i) capacity += buffers[i].size;
    }
    std::size_t Room() const { return capacity - produced; }
    void Emit(const std::uint8_t* source, std::size_t bytes) {
        while (bytes > 0 && index < count) {
            const std::size_t chunk = std::min(buffers[index].size - offset, bytes);
            std::memcpy(static_cast<std::uint8_t*>(buffers[index].ptr) + offset, source, chunk);
            source += chunk;
            bytes -= chunk;
            produced += chunk;
            offset += chunk;
            if (offset == buffers[index].size) {
                ++index;
                offset = 0;
            }
        }
    }
};

// Instance flags bits 7..9 select the PCM encoding: 0 16-bit, 1 32-bit integer, 2 float.
std::uint32_t PcmEncoding(const Instance& instance) {
    return static_cast<std::uint32_t>((instance.flags >> 7u) & 7u);
}

// Emits one decoded frame of interleaved PCM after the instance's gapless skip and up to its sample limit.
void EmitFrame(Instance& instance, PcmOutputs& outputs, const std::uint8_t* pcm, std::size_t samples, std::size_t channels, std::size_t sampleBytes) {
    std::size_t first = 0;
    std::size_t count = samples;
    if (instance.gapless.skippedSamples < instance.gapless.skipSamples) {
        const std::size_t skip = std::min<std::size_t>(count, instance.gapless.skipSamples - instance.gapless.skippedSamples);
        instance.gapless.skippedSamples = static_cast<std::uint16_t>(instance.gapless.skippedSamples + skip);
        first += skip;
        count -= skip;
    }
    if (instance.gapless.totalSamples != 0) {
        const std::uint64_t remaining = instance.gapless.totalSamples > instance.totalDecodedSamples ? instance.gapless.totalSamples - instance.totalDecodedSamples : 0;
        count = static_cast<std::size_t>(std::min<std::uint64_t>(count, remaining));
    }
    outputs.Emit(pcm + first * channels * sampleBytes, count * channels * sampleBytes);
    instance.totalDecodedSamples += count;
}

// The sidebands a run job asked for, in the SDK's order after its result.
void WriteRunSideband(const JobHeader& job, const Instance& instance, std::int32_t result, std::size_t consumed, std::size_t produced, std::uint32_t frames, const SidebandFormat& format) {
    auto* sideband = static_cast<std::uint8_t*>(job.sideband);
    std::size_t offset = 0;
    const auto write = [&](const void* value, std::size_t size) {
        if (!sideband || offset + size > job.sidebandSize) return;
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
    if (job.flags & RUN_MULTIPLE_FRAMES) {
        const SidebandMultipleFrames multiple{frames, 0};
        write(&multiple, sizeof(multiple));
    }
}

// Decodes AT9 frames from the concatenated inputs into the concatenated outputs in the instance's PCM
// encoding.
void RunAt9(Instance& instance, const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    const auto input = JoinInputs(job, inputs);
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
    for (;;) {
        if (instance.superframeRemaining == 0) consumed += RiffDataOffset(input.data() + consumed, input.size() - consumed);
        const std::size_t needed = instance.superframeRemaining == 0 ? superframeSize : instance.superframeRemaining;
        if (input.size() - consumed < needed) {
            if (input.size() != consumed) result |= AJM_RESULT_PARTIAL_INPUT;
            break;
        }
        if (pcmOutputs.Room() < frameBytes) {
            if (frames == 0) result |= AJM_RESULT_NOT_ENOUGH_ROOM;
            break;
        }
        if (instance.superframeRemaining == 0) {
            instance.superframeRemaining = static_cast<std::uint32_t>(superframeSize);
            instance.frameInSuperframe = 0;
        }
        int used = 0;
        switch (encoding) {
        case 0: decodeStatus = Atrac9Decode(instance.decoder, input.data() + consumed, reinterpret_cast<short*>(pcm.data()), &used, 0); break;
        case 1: decodeStatus = Atrac9DecodeS32(instance.decoder, input.data() + consumed, reinterpret_cast<int*>(pcm.data()), &used, 0); break;
        default: decodeStatus = Atrac9DecodeF32(instance.decoder, input.data() + consumed, reinterpret_cast<float*>(pcm.data()), &used, 0); break;
        }
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
        if ((job.flags & RUN_MULTIPLE_FRAMES) == 0) break;
    }

    if (TraceEnabled()) {
        std::fprintf(stderr, "[ajm] instance %u run flags 0x%llx: %zu input bytes in %u buffers, %zu output bytes in %u buffers, sideband %llu bytes; superframe %d (%d frames), %d channels, %d samples/frame, %d Hz, config %02x %02x %02x %02x -> result 0x%x, decoder status 0x%x, %u frames, consumed %zu, produced %zu, total samples %llu, gapless total %u skip %u skipped %u, input",
                     job.instance, static_cast<unsigned long long>(job.flags), input.size(), job.inputCount, pcmOutputs.capacity, job.outputCount, static_cast<unsigned long long>(job.sidebandSize), instance.info.superframeSize, instance.info.framesInSuperframe, instance.info.channels, instance.info.frameSamples, instance.info.samplingRate,
                     instance.info.configData[0], instance.info.configData[1], instance.info.configData[2], instance.info.configData[3], static_cast<unsigned>(result), static_cast<unsigned>(decodeStatus), frames, consumed, pcmOutputs.produced, static_cast<unsigned long long>(instance.totalDecodedSamples), instance.gapless.totalSamples, instance.gapless.skipSamples, instance.gapless.skippedSamples);
        for (std::size_t i = 0; i < input.size() && i < 16; ++i) std::fprintf(stderr, " %02x", input[i]);
        std::fprintf(stderr, "\n");
    }
    WriteRunSideband(job, instance, result, consumed, pcmOutputs.produced, frames, SidebandFormat{static_cast<std::uint32_t>(channels), ChannelMask(channels), static_cast<std::uint32_t>(instance.info.samplingRate), encoding, 0, 0});
}

// An MPEG audio Layer III frame header: the frame's bytes and PCM shape; false when the bytes do not
// start one (free-format bitrates included).
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

void OpenMp3(Instance& instance) {
    static const bool quiet = (av_log_set_level(TraceEnabled() ? AV_LOG_WARNING : AV_LOG_QUIET), true);
    (void)quiet;
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_MP3);
    if (instance.mp3) avcodec_free_context(&instance.mp3);
    instance.mp3 = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (!instance.mp3 || avcodec_open2(instance.mp3, codec, nullptr) < 0) throw std::runtime_error("AJM: cannot open the FFmpeg MP3 decoder");
}

// Decodes whole MP3 frames from the concatenated inputs. Like AT9 superframes, a frame is decoded only
// once all its bytes are present: a trailing partial frame stays unconsumed as partial input.
void RunMp3(Instance& instance, const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    const auto input = JoinInputs(job, inputs);
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
        if (pcmOutputs.Room() < frame.samples * frame.channels * sampleBytes) {
            if (frames == 0) result |= AJM_RESULT_NOT_ENOUGH_ROOM;
            break;
        }
        // Not reference counted: the decoder copies the frame into a padded buffer of its own.
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
            if (decoded->format != AV_SAMPLE_FMT_FLTP) throw std::runtime_error("AJM MP3: FFmpeg sample format " + std::to_string(decoded->format) + " is not converted");
            const auto channels = static_cast<std::size_t>(decoded->ch_layout.nb_channels);
            const auto samples = static_cast<std::size_t>(decoded->nb_samples);
            pcm.resize(samples * channels * sampleBytes);
            for (std::size_t sample = 0; sample < samples; ++sample) {
                for (std::size_t channel = 0; channel < channels; ++channel) {
                    const float value = reinterpret_cast<const float*>(decoded->extended_data[channel])[sample];
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
    if (consumed == input.size() && frames == 0 && result == 0) result |= AJM_RESULT_PARTIAL_INPUT;

    AJM_TRACE("[ajm] instance %u mp3 run flags 0x%llx: %zu input bytes, %zu output bytes -> result 0x%x, %u frames, consumed %zu, produced %zu, %u channels, %u Hz, %u bps, total samples %llu\n", job.instance, static_cast<unsigned long long>(job.flags), input.size(), pcmOutputs.capacity, static_cast<unsigned>(result), frames, consumed, pcmOutputs.produced,
              instance.mp3Channels, instance.mp3SampleRate, instance.mp3Bitrate, static_cast<unsigned long long>(instance.totalDecodedSamples));
    WriteRunSideband(job, instance, result, consumed, pcmOutputs.produced, frames, SidebandFormat{instance.mp3Channels, ChannelMask(instance.mp3Channels), instance.mp3SampleRate, encoding, instance.mp3Bitrate, 0});
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
        instance->totalDecodedSamples = 0;
        instance->gapless.skippedSamples = 0;
        if (instance->decoder && instance->initialized) ResetDecoder(*instance);
        if (instance->mp3) avcodec_flush_buffers(instance->mp3);
        WriteResult(job.sideband, job.sidebandSize, 0);
        break;
    case JobKind::SetGaplessDecode: {
        SidebandGaplessDecode gapless{};
        std::memcpy(&gapless, job.parameters, sizeof(gapless));
        AJM_TRACE("[ajm] instance %u set gapless decode: total %u, skip %u, reset %llu (sideband %llu bytes)\n", job.instance, gapless.totalSamples, gapless.skipSamples, static_cast<unsigned long long>(job.flags), static_cast<unsigned long long>(job.sidebandSize));
        instance->gapless.totalSamples = gapless.totalSamples;
        instance->gapless.skipSamples = gapless.skipSamples;
        if (job.flags) instance->gapless.skippedSamples = 0;
        WriteResult(job.sideband, job.sidebandSize, 0);
        break;
    }
    case JobKind::Run:
        if (!instance->initialized) {
            AJM_TRACE("[ajm] instance %u run before initialize\n", job.instance);
            WriteResult(job.sideband, job.sidebandSize, AJM_RESULT_NOT_INITIALIZED);
        } else if (instance->codec == CODEC_AT9) {
            RunAt9(*instance, job, inputs, outputs);
        } else if (instance->codec == CODEC_MP3) {
            RunMp3(*instance, job, inputs, outputs);
        } else {
            throw std::runtime_error("AJM: decoding codec " + std::to_string(instance->codec) + " is not implemented");
        }
        break;
    default:
        AJM_TRACE("[ajm] instance %u unknown job kind %u\n", job.instance, static_cast<unsigned>(job.kind));
        WriteResult(job.sideband, job.sidebandSize, AJM_RESULT_INVALID_PARAMETER);
        break;
    }
}

}

extern "C" {

int APS5_VABI sceAjmInitialize(int64_t reserved, uint32_t* context) {
    (void)reserved;
    if (!context) return SCE_AJM_ERROR_INVALID_PARAMETER;
    *context = g_nextContext.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceAjmFinalize(uint32_t context) {
    (void)context;
    return 0;
}

int APS5_VABI sceAjmModuleRegister(uint32_t context, uint32_t codec, int64_t reserved) {
    (void)context;
    (void)codec;
    (void)reserved;
    return 0;
}

int APS5_VABI sceAjmModuleUnregister(uint32_t context, uint32_t codec) {
    (void)context;
    (void)codec;
    return 0;
}

int APS5_VABI sceAjmMemoryRegister(uint32_t context, void* ptr, size_t pages) {
    (void)context;
    (void)ptr;
    (void)pages;
    return 0;
}

int APS5_VABI sceAjmMemoryUnregister(uint32_t context, void* ptr) {
    (void)context;
    (void)ptr;
    return 0;
}

int APS5_VABI sceAjmInstanceCreate(uint32_t context, uint32_t codec, uint64_t flags, uint32_t* instance) {
    (void)context;
    if (!instance) return SCE_AJM_ERROR_INVALID_PARAMETER;
    auto created = std::make_unique<Instance>();
    created->codec = codec;
    created->flags = flags;
    // MP3 has no codec parameters: the stream's frame headers carry them, so it runs without an initialize job.
    if (codec == CODEC_MP3) {
        OpenMp3(*created);
        created->initialized = true;
    }
    const std::uint32_t id = (codec << 14) | (g_nextInstance.fetch_add(1, std::memory_order_relaxed) & 0x3FFF);
    std::lock_guard lock(g_lock);
    g_instances[id] = std::move(created);
    *instance = id;
    AJM_TRACE("[ajm] instance create: context %u, codec %u, flags 0x%llx -> instance %u\n", context, codec, static_cast<unsigned long long>(flags), id);
    return 0;
}

int APS5_VABI sceAjmInstanceDestroy(uint32_t context, uint32_t instance) {
    (void)context;
    AJM_TRACE("[ajm] instance %u destroy\n", instance);
    std::lock_guard lock(g_lock);
    return g_instances.erase(instance) ? 0 : SCE_AJM_ERROR_INVALID_INSTANCE;
}

int APS5_VABI sceAjmDecAt9ParseConfigData(const void* config_data, AjmDecAt9ConfigDataInfo* config_info) {
    if (!config_data || !config_info) return SCE_AJM_ERROR_INVALID_PARAMETER;
    void* decoder = Atrac9GetHandle();
    unsigned char config[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(config, config_data, sizeof(config));
    const bool valid = Atrac9InitDecoder(decoder, config) == 0;
    Atrac9CodecInfo info{};
    if (valid) Atrac9GetCodecInfo(decoder, &info);
    Atrac9ReleaseHandle(decoder);
    AJM_TRACE("[ajm] parse config %02x %02x %02x %02x -> %s, %d channels, %d Hz, superframe %d bytes (%d frames of %d samples)\n", config[0], config[1], config[2], config[3], valid ? "ok" : "invalid", info.channels, info.samplingRate, info.superframeSize, info.framesInSuperframe, info.frameSamples);
    if (!valid) return SCE_AJM_ERROR_INVALID_PARAMETER;
    config_info->channels = static_cast<std::uint32_t>(info.channels);
    config_info->sample_rate = static_cast<std::uint32_t>(info.samplingRate);
    config_info->frame_samples_per_channel = static_cast<std::uint32_t>(info.frameSamples);
    config_info->superframe_samples_per_channel = static_cast<std::uint32_t>(info.frameSamples * info.framesInSuperframe);
    config_info->superframe_size = static_cast<std::uint32_t>(info.superframeSize);
    return 0;
}

int APS5_VABI sceAjmBatchInitialize(void* buffer, size_t size, AjmBatchInfo* info) {
    if (!buffer || !info) return SCE_AJM_ERROR_INVALID_PARAMETER;
    info->p_buffer = buffer;
    info->offset = 0;
    info->size = size;
    return 0;
}

int APS5_VABI sceAjmBatchJobInitialize(AjmBatchInfo* info, uint32_t instance, const void* codec_parameters, size_t codec_parameters_size, void* result) {
    auto header = MakeHeader(JobKind::Initialize, instance, result, sizeof(SidebandResult));
    header.parameterSize = std::min<std::size_t>(codec_parameters_size, sizeof(header.parameters));
    if (codec_parameters && header.parameterSize) std::memcpy(header.parameters, codec_parameters, header.parameterSize);
    return Append(info, header, nullptr, nullptr);
}

int APS5_VABI sceAjmBatchJobClearContext(AjmBatchInfo* info, uint32_t instance, void* result) {
    return Append(info, MakeHeader(JobKind::ClearContext, instance, result, sizeof(SidebandResult)), nullptr, nullptr);
}

int APS5_VABI sceAjmBatchJobSetGaplessDecode(AjmBatchInfo* info, uint32_t instance, const void* gapless_decode, int reset, void* result) {
    auto header = MakeHeader(JobKind::SetGaplessDecode, instance, result, sizeof(SidebandResult));
    if (gapless_decode) std::memcpy(header.parameters, gapless_decode, sizeof(SidebandGaplessDecode));
    header.flags = reset ? 1 : 0;
    return Append(info, header, nullptr, nullptr);
}

int APS5_VABI sceAjmBatchJobRunSplit(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size) {
    auto header = MakeHeader(JobKind::Run, instance, sideband_output, sideband_output_size);
    header.flags = flags;
    header.inputCount = static_cast<std::uint32_t>(input_buffers_num);
    header.outputCount = static_cast<std::uint32_t>(output_buffers_num);
    return Append(info, header, input_buffers, output_buffers);
}

int APS5_VABI sceAjmBatchJobRun(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size) {
    const AjmBuffer input{const_cast<void*>(data_input), data_input_size};
    const AjmBuffer output{data_output, data_output_size};
    return sceAjmBatchJobRunSplit(info, instance, flags, &input, 1, &output, 1, sideband_output, sideband_output_size);
}

int APS5_VABI sceAjmBatchJobDecode(AjmBatchInfo* info, uint32_t instance, const void* bitstream_input, size_t bitstream_input_size, void* pcm_output, size_t pcm_output_size, void* result) {
    return sceAjmBatchJobRun(info, instance, SIDEBAND_STREAM, bitstream_input, bitstream_input_size, pcm_output, pcm_output_size, result, sizeof(SidebandResult) + sizeof(SidebandStream));
}

int APS5_VABI sceAjmBatchJobGetStatistics(AjmBatchInfo* info, float interval, void* result) {
    (void)interval;
    return Append(info, MakeHeader(JobKind::GetStatistics, 0, result, 24), nullptr, nullptr);
}

int APS5_VABI sceAjmBatchStart(uint32_t context, const AjmBatchInfo* info, int priority, AjmBatchError* error, uint32_t* batch) {
    (void)context;
    (void)priority;
    if (!info || !batch) return SCE_AJM_ERROR_INVALID_PARAMETER;
    AJM_TRACE("[ajm] batch start: context %u, priority %d, %llu of %llu buffer bytes used\n", context, priority, static_cast<unsigned long long>(info->offset), static_cast<unsigned long long>(info->size));
    const auto* cursor = static_cast<const std::uint8_t*>(info->p_buffer);
    const auto* end = cursor + info->offset;
    while (cursor < end) {
        JobHeader job;
        std::memcpy(&job, cursor, sizeof(job));
        const auto* buffers = reinterpret_cast<const AjmBuffer*>(cursor + sizeof(JobHeader));
        Execute(job, buffers, buffers + job.inputCount);
        cursor += job.bytes;
    }
    if (error) std::memset(error, 0, sizeof(*error));
    *batch = g_nextBatch.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceAjmBatchWait(uint32_t context, uint32_t batch, uint32_t timeout, AjmBatchError* error) {
    (void)context;
    (void)batch;
    (void)timeout;
    if (error) std::memset(error, 0, sizeof(*error));
    return 0;
}

int APS5_VABI sceAjmBatchErrorDump(const AjmBatchInfo* info, AjmBatchError* error) {
    (void)info;
    if (error) std::memset(error, 0, sizeof(*error));
    return 0;
}

}
