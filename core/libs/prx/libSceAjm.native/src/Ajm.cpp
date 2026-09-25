#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "libatrac9.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
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

    ~Instance() {
        if (decoder) Atrac9ReleaseHandle(decoder);
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

// Decodes AT9 frames from the concatenated inputs into the concatenated outputs in the instance's PCM
// encoding.
void RunAt9(Instance& instance, const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    std::vector<std::uint8_t> input;
    for (std::uint32_t index = 0; index < job.inputCount; ++index) {
        const auto* data = static_cast<const std::uint8_t*>(inputs[index].ptr);
        input.insert(input.end(), data, data + inputs[index].size);
    }
    std::size_t outputCapacity = 0;
    for (std::uint32_t index = 0; index < job.outputCount; ++index) outputCapacity += outputs[index].size;

    const auto channels = static_cast<std::size_t>(instance.info.channels);
    const auto frameSamples = static_cast<std::size_t>(instance.info.frameSamples);
    // Instance flags bits 7..9 select the PCM encoding: 0 16-bit, 1 32-bit integer, 2 float.
    const auto encoding = static_cast<std::uint32_t>((instance.flags >> 7u) & 7u);
    const std::size_t sampleBytes = encoding == 0 ? sizeof(std::int16_t) : sizeof(std::int32_t);
    const std::size_t frameBytes = frameSamples * channels * sampleBytes;
    std::vector<std::uint8_t> pcm(frameBytes);
    static const bool traceFlags = std::getenv("APS5_TRACE_AJM") != nullptr;
    if (traceFlags && instance.totalDecodedSamples == 0 && consumedFirstRun(instance)) std::fprintf(stderr, "[ajm] instance %u flags 0x%llx (encoding %u)\n", job.instance, static_cast<unsigned long long>(instance.flags), encoding);

    std::int32_t result = 0;
    int decodeStatus = 0;
    std::size_t consumed = 0;
    std::size_t produced = 0;
    std::uint32_t frames = 0;
    std::uint32_t outputIndex = 0;
    std::size_t outputOffset = 0;
    const auto emit = [&](const std::uint8_t* source, std::size_t bytes) {
        while (bytes > 0 && outputIndex < job.outputCount) {
            const std::size_t room = outputs[outputIndex].size - outputOffset;
            const std::size_t chunk = std::min(room, bytes);
            std::memcpy(static_cast<std::uint8_t*>(outputs[outputIndex].ptr) + outputOffset, source, chunk);
            source += chunk;
            bytes -= chunk;
            produced += chunk;
            outputOffset += chunk;
            if (outputOffset == outputs[outputIndex].size) {
                ++outputIndex;
                outputOffset = 0;
            }
        }
    };

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
        if (outputCapacity - produced < frameBytes) {
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

        std::size_t first = 0;
        std::size_t count = frameSamples;
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
        emit(pcm.data() + first * channels * sampleBytes, count * channels * sampleBytes);
        instance.totalDecodedSamples += count;
        if ((job.flags & RUN_MULTIPLE_FRAMES) == 0) break;
    }

    if (TraceEnabled()) {
        std::fprintf(stderr, "[ajm] instance %u run flags 0x%llx: %zu input bytes in %u buffers, %zu output bytes in %u buffers, sideband %llu bytes; superframe %d (%d frames), %d channels, %d samples/frame, %d Hz, config %02x %02x %02x %02x -> result 0x%x, decoder status 0x%x, %u frames, consumed %zu, produced %zu, total samples %llu, gapless total %u skip %u skipped %u, input",
                     job.instance, static_cast<unsigned long long>(job.flags), input.size(), job.inputCount, outputCapacity, job.outputCount, static_cast<unsigned long long>(job.sidebandSize), instance.info.superframeSize, instance.info.framesInSuperframe, instance.info.channels, instance.info.frameSamples, instance.info.samplingRate,
                     instance.info.configData[0], instance.info.configData[1], instance.info.configData[2], instance.info.configData[3], static_cast<unsigned>(result), static_cast<unsigned>(decodeStatus), frames, consumed, produced, static_cast<unsigned long long>(instance.totalDecodedSamples), instance.gapless.totalSamples, instance.gapless.skipSamples, instance.gapless.skippedSamples);
        for (std::size_t i = 0; i < input.size() && i < 16; ++i) std::fprintf(stderr, " %02x", input[i]);
        std::fprintf(stderr, "\n");
    }
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
    if (job.flags & SIDEBAND_FORMAT) {
        const SidebandFormat format{static_cast<std::uint32_t>(channels), ChannelMask(channels), static_cast<std::uint32_t>(instance.info.samplingRate), encoding, 0, 0};
        write(&format, sizeof(format));
    }
    if (job.flags & SIDEBAND_GAPLESS_DECODE) write(&instance.gapless, sizeof(instance.gapless));
    if (job.flags & RUN_MULTIPLE_FRAMES) {
        const SidebandMultipleFrames multiple{frames, 0};
        write(&multiple, sizeof(multiple));
    }
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
        } else {
            AJM_TRACE("[ajm] instance %u run on codec %u without a decoder\n", job.instance, instance->codec);
            WriteResult(job.sideband, job.sidebandSize, AJM_RESULT_CODEC_ERROR);
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
    static std::atomic<std::uint32_t> reportedCodecs{0};
    if (codec != CODEC_AT9 && codec < 32 && (reportedCodecs.fetch_or(1u << codec) & (1u << codec)) == 0)
        std::fprintf(stderr, "[ajm] codec %u has no decoder; its jobs report codec errors\n", codec);
    auto created = std::make_unique<Instance>();
    created->codec = codec;
    created->flags = flags;
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
