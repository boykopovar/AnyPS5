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
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// The batch buffer and its job records are private to this library: the guest only reserves the
// memory and reads the "used bytes" field of AjmBatchInfo. Sideband results written back to guest
// memory follow the SDK layouts.
namespace {

constexpr int SCE_AJM_ERROR_INVALID_CONTEXT = static_cast<int>(0x80930002);
constexpr int SCE_AJM_ERROR_INVALID_INSTANCE = static_cast<int>(0x80930003);
constexpr int SCE_AJM_ERROR_INVALID_BATCH = static_cast<int>(0x80930004);
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
constexpr std::uint32_t CODEC_OPUS = 24;

// APS5_TRACE_AJM=1 logs every job the title submits and what the library writes back.
bool TraceEnabled() {
    static const bool enabled = std::getenv("APS5_TRACE_AJM") != nullptr;
    return enabled;
}

#define AJM_TRACE(...) \
    do { \
        if (TraceEnabled()) std::fprintf(stderr, __VA_ARGS__); \
    } while (0)

constexpr std::uint64_t RUN_GET_CODEC_INFO = 1ull << 11;
constexpr std::uint64_t RUN_MULTIPLE_FRAMES = 1ull << 12;
constexpr std::uint64_t CONTROL_RESET = 1ull << 13;
constexpr std::uint64_t CONTROL_INITIALIZE = 1ull << 14;
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

struct SidebandAt9CodecInfo {
    std::uint32_t superframeSize;
    std::uint32_t framesInSuperframe;
    std::uint32_t nextFrameSize;
    std::uint32_t frameSamples;
};

struct SidebandResampleInfo {
    float ratio;
    std::int32_t samples;
    std::uint32_t reserved[8];
};

struct Resampler {
    float ratio = 1.0f;
    std::vector<double> frames;
    double position = 1.0;
};

struct Instance {
    std::uint32_t codec = 0;
    std::uint64_t flags = 0;
    void* decoder = nullptr;
    std::vector<void*> channelDecoders;
    std::vector<std::uint32_t> channelBytes;
    std::vector<std::uint8_t> blockPcm;
    Atrac9CodecInfo info{};
    bool initialized = false;
    std::uint32_t superframeRemaining = 0;
    std::uint32_t frameInSuperframe = 0;
    std::uint64_t totalDecodedSamples = 0;
    SidebandGaplessDecode gapless{};
    bool flagsReported = false;
    AVCodecContext* mp3 = nullptr;
    std::uint32_t mp3Channels = 0;
    std::uint32_t mp3SampleRate = 0;
    std::uint32_t mp3Bitrate = 0;
    AVCodecContext* opus = nullptr;
    std::uint32_t opusChannels = 0;
    std::uint32_t opusSampleRate = 0;
    std::vector<std::uint8_t> opusPending;
    Resampler resampler;

    ~Instance() {
        if (decoder) Atrac9ReleaseHandle(decoder);
        for (void* channelDecoder : channelDecoders) Atrac9ReleaseHandle(channelDecoder);
        if (mp3) avcodec_free_context(&mp3);
        if (opus) avcodec_free_context(&opus);
    }
};

std::mutex& g_lock = *new std::mutex();
std::map<std::uint32_t, std::unique_ptr<Instance>>& g_instances = *new std::map<std::uint32_t, std::unique_ptr<Instance>>();
std::atomic<std::uint32_t> g_nextContext{1};
std::atomic<std::uint32_t> g_nextInstance{1};
std::atomic<std::uint32_t> g_nextBatch{1};
std::mutex& g_batchLock = *new std::mutex();
std::set<std::uint32_t>& g_batches = *new std::set<std::uint32_t>();

enum class JobKind : std::uint32_t {
    Initialize = 1,
    ClearContext = 2,
    SetGaplessDecode = 3,
    Run = 4,
    GetStatistics = 5,
    Control = 6,
    SetResampleParameters = 7,
    GetResampleInfo = 8,
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

struct PackedJob {
    std::uint8_t kind;
    std::uint8_t inputCount;
    std::uint8_t outputCount;
    std::uint8_t parameterSize;
    std::uint32_t instance;
    std::uint64_t flags;
    void* sideband;
    std::uint32_t sidebandSize;
    std::uint32_t bytes;
};
static_assert(sizeof(PackedJob) == 32);

int Append(AjmBatchInfo* info, const JobHeader& header, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    if (!info || !info->p_buffer) return SCE_AJM_ERROR_INVALID_PARAMETER;
    if (header.inputCount > 0xffu || header.outputCount > 0xffu || header.parameterSize > sizeof(header.parameters) || header.sidebandSize > 0xffffffffu) return SCE_AJM_ERROR_INVALID_PARAMETER;
    if ((header.inputCount && !inputs) || (header.outputCount && !outputs)) NotImplemented_nid_no_patch("AJM batch job with a null buffer list and a nonzero buffer count");
    const std::size_t parameterBytes = (header.parameterSize + 7u) & ~std::size_t{7};
    const std::size_t bytes = sizeof(PackedJob) + (header.inputCount + header.outputCount) * sizeof(AjmBuffer) + parameterBytes;
    if (info->offset > info->size || bytes > info->size - info->offset) return SCE_AJM_ERROR_OUT_OF_RESOURCES;
    auto* cursor = static_cast<std::uint8_t*>(info->p_buffer) + info->offset;
    const PackedJob record{static_cast<std::uint8_t>(header.kind), static_cast<std::uint8_t>(header.inputCount), static_cast<std::uint8_t>(header.outputCount), static_cast<std::uint8_t>(header.parameterSize), header.instance, header.flags, header.sideband, static_cast<std::uint32_t>(header.sidebandSize), static_cast<std::uint32_t>(bytes)};
    std::memcpy(cursor, &record, sizeof(record));
    cursor += sizeof(record);
    if (inputs) std::memcpy(cursor, inputs, header.inputCount * sizeof(AjmBuffer));
    cursor += header.inputCount * sizeof(AjmBuffer);
    if (outputs) std::memcpy(cursor, outputs, header.outputCount * sizeof(AjmBuffer));
    cursor += header.outputCount * sizeof(AjmBuffer);
    if (header.parameterSize) std::memcpy(cursor, header.parameters, header.parameterSize);
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

constexpr std::uint8_t AT9_MULTICHANNEL_HEADER = 0x30;

struct At9MultichannelConfig {
    std::uint32_t channels;
    std::uint32_t frameBytes;
    std::uint32_t framesInSuperframe;
    unsigned char monoConfig[ATRAC9_CONFIG_DATA_SIZE];
};

bool ParseMultichannelConfig(const std::uint8_t* config, At9MultichannelConfig& parsed) {
    if (config[0] != AT9_MULTICHANNEL_HEADER) return false;
    const std::uint32_t bits = (std::uint32_t{config[1]} << 16) | (std::uint32_t{config[2]} << 8) | config[3];
    if ((bits >> 13) & 1u) return false;
    const std::uint32_t sampleRateIndex = (bits >> 20) & 0xFu;
    const std::uint32_t superframeIndex = bits & 3u;
    parsed.channels = ((bits >> 14) & 0x3Fu) + 1;
    parsed.frameBytes = ((bits >> 2) & 0x7FFu) + 1;
    parsed.framesInSuperframe = 1u << superframeIndex;
    const std::uint32_t mono = (0xFEu << 24) | (sampleRateIndex << 20) | ((parsed.frameBytes - 1) << 5) | (superframeIndex << 3);
    for (std::size_t i = 0; i < ATRAC9_CONFIG_DATA_SIZE; ++i) parsed.monoConfig[i] = static_cast<unsigned char>(mono >> (24 - 8 * i));
    return true;
}

bool DescribeAt9Config(const std::uint8_t* configData, Atrac9CodecInfo& info) {
    At9MultichannelConfig multichannel{};
    const bool isMultichannel = ParseMultichannelConfig(configData, multichannel);
    unsigned char config[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(config, isMultichannel ? multichannel.monoConfig : configData, sizeof(config));
    void* decoder = Atrac9GetHandle();
    const bool valid = Atrac9InitDecoder(decoder, config) == 0;
    if (valid) Atrac9GetCodecInfo(decoder, &info);
    Atrac9ReleaseHandle(decoder);
    if (!valid) return false;
    if (isMultichannel) {
        info.channels = static_cast<int>(multichannel.channels);
        info.channelConfigIndex = -1;
        info.superframeSize = static_cast<int>(multichannel.channels * multichannel.frameBytes * multichannel.framesInSuperframe);
    }
    std::memcpy(info.configData, configData, ATRAC9_CONFIG_DATA_SIZE);
    return true;
}

// decoder handle keeps its position within the superframe, which a re-initialization does not clear.
bool ResetDecoder(Instance& instance) {
    if (instance.decoder) Atrac9ReleaseHandle(instance.decoder);
    instance.decoder = nullptr;
    for (void* channelDecoder : instance.channelDecoders) Atrac9ReleaseHandle(channelDecoder);
    instance.channelDecoders.clear();
    instance.superframeRemaining = 0;
    instance.frameInSuperframe = 0;
    unsigned char config[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(config, instance.info.configData, sizeof(config));
    At9MultichannelConfig multichannel{};
    if (!ParseMultichannelConfig(config, multichannel)) {
        instance.decoder = Atrac9GetHandle();
        return Atrac9InitDecoder(instance.decoder, config) == 0;
    }
    instance.channelBytes.assign(multichannel.channels, 0);
    for (std::uint32_t channel = 0; channel < multichannel.channels; ++channel) {
        instance.channelDecoders.push_back(Atrac9GetHandle());
        if (Atrac9InitDecoder(instance.channelDecoders.back(), multichannel.monoConfig) != 0) return false;
    }
    return true;
}

bool HasAt9Decoder(const Instance& instance) {
    return instance.decoder || !instance.channelDecoders.empty();
}

int DecodeAt9Block(void* decoder, std::uint32_t encoding, const std::uint8_t* data, int size, std::uint8_t* pcm, int* used) {
    switch (encoding) {
    case 0: return Atrac9Decode(decoder, data, size, reinterpret_cast<short*>(pcm), used, 0);
    case 1: return Atrac9DecodeS32(decoder, data, size, reinterpret_cast<int*>(pcm), used, 0);
    default: return Atrac9DecodeF32(decoder, data, size, reinterpret_cast<float*>(pcm), used, 0);
    }
}

constexpr int AT9_MULTICHANNEL_BLOCK_OVERRUN = 0x7F000001;

int DecodeAt9Frame(Instance& instance, const std::uint8_t* data, int size, std::uint8_t* pcm, std::uint32_t encoding, std::size_t sampleBytes, int* used) {
    if (instance.channelDecoders.empty()) return DecodeAt9Block(instance.decoder, encoding, data, size, pcm, used);
    const std::size_t channels = instance.channelDecoders.size();
    const auto frameSamples = static_cast<std::size_t>(instance.info.frameSamples);
    const auto framesInSuperframe = static_cast<std::uint32_t>(std::max(1, instance.info.framesInSuperframe));
    const auto budget = static_cast<std::uint32_t>(instance.info.superframeSize / static_cast<int>(channels));
    const bool lastFrame = instance.frameInSuperframe + 1 >= framesInSuperframe;
    if (instance.frameInSuperframe == 0) std::fill(instance.channelBytes.begin(), instance.channelBytes.end(), 0u);
    instance.blockPcm.resize(frameSamples * sampleBytes);
    int offset = 0;
    for (std::size_t channel = 0; channel < channels; ++channel) {
        const std::uint32_t left = budget - std::min(budget, instance.channelBytes[channel]);
        const int available = std::min(static_cast<int>(left), size - offset);
        if (available <= 0) return AT9_MULTICHANNEL_BLOCK_OVERRUN;
        int blockUsed = 0;
        const int status = DecodeAt9Block(instance.channelDecoders[channel], encoding, data + offset, available, instance.blockPcm.data(), &blockUsed);
        if (status != 0) return status;
        if (blockUsed <= 0 || blockUsed > available) return AT9_MULTICHANNEL_BLOCK_OVERRUN;
        const std::uint32_t blockBytes = lastFrame ? left : static_cast<std::uint32_t>(blockUsed);
        if (static_cast<int>(blockBytes) > size - offset) return AT9_MULTICHANNEL_BLOCK_OVERRUN;
        for (std::size_t sample = 0; sample < frameSamples; ++sample) {
            std::memcpy(pcm + (sample * channels + channel) * sampleBytes, instance.blockPcm.data() + sample * sampleBytes, sampleBytes);
        }
        instance.channelBytes[channel] += blockBytes;
        offset += static_cast<int>(blockBytes);
    }
    *used = offset;
    return 0;
}

void OpenOpus(Instance& instance);

void ClearResampler(Instance& instance) {
    instance.resampler.frames.clear();
    instance.resampler.position = 1.0;
}

std::int32_t InitializeInstance(Instance& instance, const std::uint8_t* parameters, std::uint64_t size) {
    ClearResampler(instance);
    if (instance.codec == CODEC_OPUS) {
        if (size < 8) return AJM_RESULT_INVALID_PARAMETER;
        std::uint32_t channels = 0;
        std::uint32_t sampleRate = 0;
        std::memcpy(&channels, parameters, sizeof(channels));
        std::memcpy(&sampleRate, parameters + 4, sizeof(sampleRate));
        if (channels == 0 || channels > 8 || sampleRate == 0) return AJM_RESULT_INVALID_PARAMETER;
        std::uint32_t third = 0;
        if (size >= 12) std::memcpy(&third, parameters + 8, sizeof(third));
        if (sampleRate != 48000 || third != 0) NotImplemented_nid_no_patch("sceAjmBatchJobInitialize (Opus sample rate other than 48000 or nonzero third parameter word)");
        instance.opusChannels = channels;
        instance.opusSampleRate = sampleRate;
        OpenOpus(instance);
        instance.opusPending.clear();
        instance.initialized = true;
        instance.totalDecodedSamples = 0;
        instance.gapless = {};
        return 0;
    }
    if (instance.codec != CODEC_AT9) {
        instance.initialized = true;
        return 0;
    }
    if (size < ATRAC9_CONFIG_DATA_SIZE) return AJM_RESULT_INVALID_PARAMETER;
    std::memcpy(instance.info.configData, parameters, ATRAC9_CONFIG_DATA_SIZE);
    std::uint8_t config[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(config, parameters, sizeof(config));
    if (!ResetDecoder(instance) || !DescribeAt9Config(config, instance.info)) {
        instance.initialized = false;
        return AJM_RESULT_INVALID_PARAMETER;
    }
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

std::uint32_t PcmEncoding(const Instance& instance) {
    return static_cast<std::uint32_t>((instance.flags >> 7u) & 7u);
}

double PcmValue(const std::uint8_t* at, std::uint32_t encoding) {
    if (encoding == 0) {
        std::int16_t value = 0;
        std::memcpy(&value, at, sizeof(value));
        return value / 32768.0;
    }
    if (encoding == 1) {
        std::int32_t value = 0;
        std::memcpy(&value, at, sizeof(value));
        return value / 2147483648.0;
    }
    float value = 0;
    std::memcpy(&value, at, sizeof(value));
    return value;
}

void StorePcm(double value, std::uint8_t* out, std::uint32_t encoding) {
    if (encoding == 0) {
        const auto converted = static_cast<std::int16_t>(std::clamp(std::lrint(value * 32768.0), -32768L, 32767L));
        std::memcpy(out, &converted, sizeof(converted));
    } else if (encoding == 1) {
        const auto converted = static_cast<std::int32_t>(std::clamp(std::llrint(value * 2147483648.0), -2147483648LL, 2147483647LL));
        std::memcpy(out, &converted, sizeof(converted));
    } else {
        const auto converted = static_cast<float>(value);
        std::memcpy(out, &converted, sizeof(converted));
    }
}

bool Resampling(const Instance& instance) {
    return instance.resampler.ratio != 1.0f || !instance.resampler.frames.empty();
}

std::size_t ResamplerHeld(const Instance& instance, std::size_t channels) {
    const auto& resampler = instance.resampler;
    if (resampler.frames.empty() || channels == 0) return 0;
    const std::size_t frames = resampler.frames.size() / channels;
    const auto next = static_cast<std::size_t>(resampler.position);
    return frames > next ? frames - next : 0;
}

void ResamplerPush(Instance& instance, const double* samples, std::size_t frames, std::size_t channels) {
    auto& stored = instance.resampler.frames;
    if (frames == 0) return;
    if (stored.empty()) stored.insert(stored.end(), samples, samples + channels);
    stored.insert(stored.end(), samples, samples + frames * channels);
}

void ResamplerProduce(Instance& instance, PcmOutputs& outputs, std::size_t channels, std::uint32_t encoding, std::size_t sampleBytes) {
    auto& resampler = instance.resampler;
    if (resampler.frames.empty() || channels == 0) return;
    const std::size_t frameBytes = channels * sampleBytes;
    const std::size_t available = resampler.frames.size() / channels;
    std::vector<std::uint8_t> frame(frameBytes);
    while (outputs.Room() >= frameBytes) {
        const auto index = static_cast<std::size_t>(resampler.position);
        if (index + 2 >= available) break;
        const double t = resampler.position - static_cast<double>(index);
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const double p0 = resampler.frames[(index - 1) * channels + channel];
            const double p1 = resampler.frames[index * channels + channel];
            const double p2 = resampler.frames[(index + 1) * channels + channel];
            const double p3 = resampler.frames[(index + 2) * channels + channel];
            const double value = p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
            StorePcm(value, frame.data() + channel * sampleBytes, encoding);
        }
        outputs.Emit(frame.data(), frameBytes);
        resampler.position += resampler.ratio;
    }
    const auto index = std::min(static_cast<std::size_t>(resampler.position), available);
    if (index > 1) {
        resampler.frames.erase(resampler.frames.begin(), resampler.frames.begin() + static_cast<std::ptrdiff_t>((index - 1) * channels));
        resampler.position -= static_cast<double>(index - 1);
    }
}

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
    instance.totalDecodedSamples += count;
    if (!Resampling(instance)) {
        outputs.Emit(pcm + first * channels * sampleBytes, count * channels * sampleBytes);
        return;
    }
    const auto encoding = PcmEncoding(instance);
    std::vector<double> values(count * channels);
    for (std::size_t index = 0; index < values.size(); ++index) values[index] = PcmValue(pcm + (first * channels + index) * sampleBytes, encoding);
    ResamplerPush(instance, values.data(), count, channels);
    ResamplerProduce(instance, outputs, channels, encoding, sampleBytes);
}

SidebandFormat CurrentFormat(const Instance& instance) {
    switch (instance.codec) {
    case CODEC_AT9: return {static_cast<std::uint32_t>(instance.info.channels), ChannelMask(static_cast<std::size_t>(instance.info.channels)), static_cast<std::uint32_t>(instance.info.samplingRate), PcmEncoding(instance), 0, 0};
    case CODEC_MP3: return {instance.mp3Channels, ChannelMask(instance.mp3Channels), instance.mp3SampleRate, PcmEncoding(instance), instance.mp3Bitrate, 0};
    case CODEC_OPUS: return {instance.opusChannels, ChannelMask(instance.opusChannels), instance.opusSampleRate, PcmEncoding(instance), 0, 0};
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
    if (job.flags & RUN_GET_CODEC_INFO) {
        const SidebandAt9CodecInfo codecInfo = At9CodecInfo(instance);
        write(&codecInfo, sizeof(codecInfo));
    }
    if (job.flags & RUN_MULTIPLE_FRAMES) {
        const SidebandMultipleFrames multiple{frames, 0};
        write(&multiple, sizeof(multiple));
    }
}

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
    if (Resampling(instance)) ResamplerProduce(instance, pcmOutputs, channels, encoding, sampleBytes);
    for (;;) {
        if (instance.superframeRemaining == 0) consumed += RiffDataOffset(input.data() + consumed, input.size() - consumed);
        const std::size_t needed = instance.superframeRemaining == 0 ? superframeSize : instance.superframeRemaining;
        if (input.size() - consumed < needed) {
            if (input.size() != consumed) result |= AJM_RESULT_PARTIAL_INPUT;
            break;
        }
        if (Resampling(instance) ? pcmOutputs.Room() < channels * sampleBytes : pcmOutputs.Room() < frameBytes) {
            if (frames == 0 && pcmOutputs.produced == 0) result |= AJM_RESULT_NOT_ENOUGH_ROOM;
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
        if ((job.flags & RUN_MULTIPLE_FRAMES) == 0) break;
    }

    if (TraceEnabled()) {
        std::fprintf(stderr, "[ajm] instance %u run flags 0x%llx: %zu input bytes in %u buffers, %zu output bytes in %u buffers, sideband %llu bytes; superframe %d (%d frames), %d channels, %d samples/frame, %d Hz, config %02x %02x %02x %02x -> result 0x%x, decoder status 0x%x, %u frames, consumed %zu, produced %zu, total samples %llu, gapless total %u skip %u skipped %u, input",
                     job.instance, static_cast<unsigned long long>(job.flags), input.size(), job.inputCount, pcmOutputs.capacity, job.outputCount, static_cast<unsigned long long>(job.sidebandSize), instance.info.superframeSize, instance.info.framesInSuperframe, instance.info.channels, instance.info.frameSamples, instance.info.samplingRate,
                     instance.info.configData[0], instance.info.configData[1], instance.info.configData[2], instance.info.configData[3], static_cast<unsigned>(result), static_cast<unsigned>(decodeStatus), frames, consumed, pcmOutputs.produced, static_cast<unsigned long long>(instance.totalDecodedSamples), instance.gapless.totalSamples, instance.gapless.skipSamples, instance.gapless.skippedSamples);
        for (std::size_t i = 0; i < input.size() && i < 16; ++i) std::fprintf(stderr, " %02x", input[i]);
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

void ParseMp3Ofl(const std::uint8_t* stream, std::uint32_t streamSize, std::uint32_t header, AjmDecMp3ParseFrame* frame) {
    const std::size_t end = std::min<std::size_t>(streamSize, frame->frame_size);
    if (end <= 4) return;
    const std::size_t available = end - 4;
    std::size_t bit = 0;
    const auto readBits = [&](std::uint32_t count) {
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < count; ++i, ++bit) value = (value << 1u) | (bit / 8 < available ? (stream[4 + bit / 8] >> (7 - bit % 8)) & 1u : 0u);
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
    std::size_t at = 4 + std::min<std::size_t>((bit + 7) / 8, available);

    if (at + 8 <= end && (std::memcmp(stream + at, "Xing", 4) == 0 || std::memcmp(stream + at, "Info", 4) == 0)) {
        const std::uint8_t flags = stream[at + 7];
        std::size_t field = at + 8;
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
            if ((flags & 0x01u) && frames > 0) frame->total_samples = frames * frame->samples_per_channel - (delay + padding);
            frame->encoder_delay = frame->samples_per_channel + delay + 529;
            frame->ofl_type = MP3_OFL_LAME;
        }
    } else if (at != 36 && end >= 40 && std::memcmp(stream + 36, "VBRI", 4) == 0) {
        NotImplemented_nid_no_patch("sceAjmDecMp3ParseFrame (VBRI header outside MPEG-1 stereo)");
    } else if (at + 26 <= end && std::memcmp(stream + at, "VBRI", 4) == 0) {
        frame->encoder_delay = ReadBigEndian(stream + at + 6, 2);
        frame->ofl_type = MP3_OFL_VBRI;
        if (frame->frame_size <= streamSize) {
            AjmDecMp3ParseFrame next{};
            if (ParseMp3Header(stream + frame->frame_size, streamSize - static_cast<std::uint32_t>(frame->frame_size), 1, &next) == 0 && next.ofl_type == MP3_OFL_FGH) {
                frame->encoder_delay += next.encoder_delay;
                frame->total_samples = next.total_samples;
                frame->ofl_type = MP3_OFL_VBRI_AND_FGH;
            }
        }
    } else if (at + 10 <= end) {
        while (at + 9 < end && stream[at] != 0xB4u) ++at;
        if (at + 9 >= end) return;
        std::uint8_t crc = 0xFF;
        for (std::size_t i = 0; i < 9; ++i) {
            for (int shift = 7; shift >= 0; --shift) {
                const bool same = ((crc >> 7u) & 1u) == ((stream[at + i] >> shift) & 1u);
                crc = static_cast<std::uint8_t>(same ? (crc << 1u) ^ 0x45u : crc << 1u);
            }
        }
        if (stream[at + 9] != crc) return;
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
    const std::uint32_t sampleRate = sampleRates[version][(header >> 10u) & 3u];
    const std::uint32_t bitrate = kbps[version][(header >> 12u) & 15u] * 1000u;
    const bool valid = (header >> 21u) == 0x7FFu && sampleRate != 0 && bitrate != 0;
    AJM_TRACE("[ajm] parse mp3 frame %02x %02x %02x %02x (%u bytes, parse ofl %d) -> %s, %u Hz, %u bps\n", stream[0], stream[1], stream[2], stream[3], streamSize, parseOfl, valid ? "ok" : "invalid", sampleRate, bitrate);
    if (!valid) return SCE_AJM_ERROR_INVALID_PARAMETER;
    if (parseOfl && ((header >> 17u) & 3u) != 1u) NotImplemented_nid_no_patch("sceAjmDecMp3ParseFrame (original file length lookup outside layer III)");
    if (parseOfl && ((header >> 16u) & 1u) == 0) NotImplemented_nid_no_patch("sceAjmDecMp3ParseFrame (original file length lookup in a CRC-protected frame)");
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
    if (parseOfl) ParseMp3Ofl(stream, streamSize, header, frame);
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
void OpenOpus(Instance& instance) {
    static const bool quiet = (av_log_set_level(TraceEnabled() ? AV_LOG_WARNING : AV_LOG_QUIET), true);
    (void)quiet;
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    if (instance.opus) avcodec_free_context(&instance.opus);
    instance.opus = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (!instance.opus) throw std::runtime_error("AJM: cannot allocate the FFmpeg Opus decoder");
    av_channel_layout_default(&instance.opus->ch_layout, static_cast<int>(instance.opusChannels));
    instance.opus->sample_rate = 48000;
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

void RunOpus(Instance& instance, const JobHeader& job, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    const auto input = JoinInputs(job, inputs);
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
                const std::uint64_t remaining = instance.gapless.totalSamples > instance.totalDecodedSamples ? instance.gapless.totalSamples - instance.totalDecodedSamples : 0;
                count = static_cast<std::size_t>(std::min<std::uint64_t>(count, remaining));
            }
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

void ClearContext(Instance& instance) {
    instance.totalDecodedSamples = 0;
    instance.gapless.skippedSamples = 0;
    if (HasAt9Decoder(instance) && instance.initialized) ResetDecoder(instance);
    if (instance.mp3) avcodec_flush_buffers(instance.mp3);
    if (instance.opus) avcodec_flush_buffers(instance.opus);
    instance.opusPending.clear();
    ClearResampler(instance);
}

std::size_t ControlInitializeSize(std::uint32_t codec) {
    switch (codec) {
    case CODEC_MP3: return 0;
    case CODEC_AT9: return 8;
    case CODEC_OPUS: return 12;
    default: NotImplemented_nid_no_patch("sceAjmBatchJobControl (INITIALIZE for a codec other than MP3, ATRAC9 and Opus)"); return 0;
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
    const std::size_t channels = instance.codec == CODEC_OPUS ? instance.opusChannels : instance.codec == CODEC_MP3 ? instance.mp3Channels : static_cast<std::size_t>(std::max(0, instance.info.channels));
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
        if (job.flags) instance->gapless.skippedSamples = 0;
        WriteResult(job.sideband, job.sidebandSize, 0);
        break;
    }
    case JobKind::Control:
        Control(*instance, job, inputs);
        break;
    case JobKind::SetResampleParameters:
        std::memcpy(&instance->resampler.ratio, job.parameters, sizeof(instance->resampler.ratio));
        AJM_TRACE("[ajm] instance %u set resample ratio %f\n", job.instance, static_cast<double>(instance->resampler.ratio));
        WriteResult(job.sideband, job.sidebandSize, 0);
        break;
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
        } else if (instance->codec != CODEC_AT9 && instance->codec != CODEC_MP3 && instance->codec != CODEC_OPUS) {
            throw std::runtime_error("AJM: decoding codec " + std::to_string(instance->codec) + " is not implemented");
        } else if ((job.flags & RUN_GET_CODEC_INFO) && (job.flags & RUN_MULTIPLE_FRAMES)) {
            NotImplemented_nid_no_patch("AJM run job with both RUN_GET_CODEC_INFO and RUN_MULTIPLE_FRAMES (sideband order)");
        } else if ((job.flags & RUN_GET_CODEC_INFO) && instance->codec != CODEC_AT9) {
            NotImplemented_nid_no_patch("AJM RUN_GET_CODEC_INFO for a codec other than ATRAC9");
        } else if (job.inputCount == 0 && job.outputCount == 0) {
            AJM_TRACE("[ajm] instance %u run flags 0x%llx without buffers, sideband %llu bytes\n", job.instance, static_cast<unsigned long long>(job.flags), static_cast<unsigned long long>(job.sidebandSize));
            WriteRunSideband(job, *instance, 0, 0, 0, 0, CurrentFormat(*instance));
        } else if (instance->codec == CODEC_AT9) {
            RunAt9(*instance, job, inputs, outputs);
        } else if (instance->codec == CODEC_MP3) {
            RunMp3(*instance, job, inputs, outputs);
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
    std::uint8_t config[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(config, config_data, sizeof(config));
    Atrac9CodecInfo info{};
    const bool valid = DescribeAt9Config(config, info);
    AJM_TRACE("[ajm] parse config %02x %02x %02x %02x -> %s, %d channels, %d Hz, superframe %d bytes (%d frames of %d samples)\n", config[0], config[1], config[2], config[3], valid ? "ok" : "invalid", info.channels, info.samplingRate, info.superframeSize, info.framesInSuperframe, info.frameSamples);
    if (!valid) return SCE_AJM_ERROR_INVALID_PARAMETER;
    config_info->channels = static_cast<std::uint32_t>(info.channels);
    config_info->sample_rate = static_cast<std::uint32_t>(info.samplingRate);
    config_info->frame_samples_per_channel = static_cast<std::uint32_t>(info.frameSamples);
    config_info->superframe_samples_per_channel = static_cast<std::uint32_t>(info.frameSamples * info.framesInSuperframe);
    config_info->superframe_size = static_cast<std::uint32_t>(info.superframeSize);
    return 0;
}

int APS5_VABI sceAjmDecMp3ParseFrame(const uint8_t* stream, uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame) {
    return ParseMp3Header(stream, streamSize, parseOfl, frame);
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
    header.parameterSize = gapless_decode ? sizeof(SidebandGaplessDecode) : 0;
    header.flags = reset ? 1 : 0;
    return Append(info, header, nullptr, nullptr);
}

int APS5_VABI sceAjmBatchJobControl(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* sideband_input, size_t sideband_input_size, void* sideband_output, size_t sideband_output_size) {
    constexpr std::uint64_t supported = CONTROL_RESET | CONTROL_INITIALIZE | SIDEBAND_GAPLESS_DECODE;
    if (flags == 0 || (flags & ~supported) != 0) NotImplemented_nid_no_patch("sceAjmBatchJobControl (flags other than a combination of RESET, INITIALIZE and SIDEBAND_GAPLESS_DECODE)");
    if ((flags & SIDEBAND_GAPLESS_DECODE) != 0 && (flags & CONTROL_RESET) == 0) NotImplemented_nid_no_patch("sceAjmBatchJobControl (SIDEBAND_GAPLESS_DECODE without RESET)");
    if (sideband_output_size != sizeof(SidebandResult)) NotImplemented_nid_no_patch("sceAjmBatchJobControl (sideband output other than the 8-byte result)");
    if (sideband_input_size != 0 && !sideband_input) NotImplemented_nid_no_patch("sceAjmBatchJobControl (null sideband input)");
    auto header = MakeHeader(JobKind::Control, instance, sideband_output, sideband_output_size);
    header.flags = flags;
    header.inputCount = sideband_input_size != 0 ? 1 : 0;
    const AjmBuffer input{const_cast<void*>(sideband_input), sideband_input_size};
    return Append(info, header, &input, nullptr);
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

int APS5_VABI sceAjmBatchJobSetResampleParameters(AjmBatchInfo* info, uint32_t instance, float ratio, uint32_t flags, void* result) {
    (void)flags;
    if (!std::isfinite(ratio) || ratio <= 0.0f) return SCE_AJM_ERROR_INVALID_PARAMETER;
    auto header = MakeHeader(JobKind::SetResampleParameters, instance, result, sizeof(SidebandResult));
    std::memcpy(header.parameters, &ratio, sizeof(ratio));
    header.parameterSize = sizeof(ratio);
    return Append(info, header, nullptr, nullptr);
}

int APS5_VABI sceAjmBatchJobGetResampleInfo(AjmBatchInfo* info, uint32_t instance, void* result) {
    return Append(info, MakeHeader(JobKind::GetResampleInfo, instance, result, sizeof(SidebandResult) + sizeof(SidebandResampleInfo)), nullptr, nullptr);
}

int APS5_VABI sceAjmBatchJobDecode(AjmBatchInfo* info, uint32_t instance, const void* bitstream_input, size_t bitstream_input_size, void* pcm_output, size_t pcm_output_size, void* result) {
    return sceAjmBatchJobRun(info, instance, SIDEBAND_STREAM, bitstream_input, bitstream_input_size, pcm_output, pcm_output_size, result, sizeof(SidebandResult) + sizeof(SidebandStream));
}

int APS5_VABI sceAjmBatchJobDecodeSingle(AjmBatchInfo* info, uint32_t instance, const void* bitstream_input, size_t bitstream_input_size, void* pcm_output, size_t pcm_output_size, void* result) {
    return sceAjmBatchJobRun(info, instance, SIDEBAND_STREAM, bitstream_input, bitstream_input_size, pcm_output, pcm_output_size, result, sizeof(SidebandResult) + sizeof(SidebandStream));
}

int APS5_VABI sceAjmBatchJobGetGaplessDecode(AjmBatchInfo* info, uint32_t instance, void* result) {
    return sceAjmBatchJobRunSplit(info, instance, SIDEBAND_GAPLESS_DECODE, nullptr, 0, nullptr, 0, result, sizeof(SidebandResult) + sizeof(SidebandGaplessDecode));
}

int APS5_VABI sceAjmBatchJobGetInfo(AjmBatchInfo* info, uint32_t instance, void* result) {
    return sceAjmBatchJobRunSplit(info, instance, SIDEBAND_FORMAT, nullptr, 0, nullptr, 0, result, sizeof(SidebandResult) + sizeof(SidebandFormat));
}

int APS5_VABI sceAjmBatchJobGetCodecInfo(AjmBatchInfo* info, uint32_t instance, void* result, size_t result_size) {
    return sceAjmBatchJobRunSplit(info, instance, RUN_GET_CODEC_INFO, nullptr, 0, nullptr, 0, result, result_size);
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
    std::vector<AjmBuffer> buffers;
    while (cursor < end) {
        PackedJob record;
        std::memcpy(&record, cursor, sizeof(record));
        JobHeader job{};
        job.kind = static_cast<JobKind>(record.kind);
        job.bytes = record.bytes;
        job.instance = record.instance;
        job.flags = record.flags;
        job.sideband = record.sideband;
        job.sidebandSize = record.sidebandSize;
        job.inputCount = record.inputCount;
        job.outputCount = record.outputCount;
        job.parameterSize = record.parameterSize;
        buffers.resize(record.inputCount + record.outputCount);
        if (!buffers.empty()) std::memcpy(buffers.data(), cursor + sizeof(record), buffers.size() * sizeof(AjmBuffer));
        if (record.parameterSize) std::memcpy(job.parameters, cursor + sizeof(record) + buffers.size() * sizeof(AjmBuffer), record.parameterSize);
        Execute(job, buffers.data(), buffers.data() + job.inputCount);
        cursor += record.bytes;
    }
    if (error) std::memset(error, 0, sizeof(*error));
    const auto id = g_nextBatch.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard lock(g_batchLock);
        g_batches.insert(id);
    }
    *batch = id;
    return 0;
}

int APS5_VABI sceAjmBatchWait(uint32_t context, uint32_t batch, uint32_t timeout, AjmBatchError* error) {
    (void)context;
    (void)timeout;
    {
        std::lock_guard lock(g_batchLock);
        if (g_batches.erase(batch) == 0) return SCE_AJM_ERROR_INVALID_BATCH;
    }
    if (error) std::memset(error, 0, sizeof(*error));
    return 0;
}

int APS5_VABI sceAjmBatchCancel(uint32_t context, uint32_t batch) {
    if (context == 0 || context >= g_nextContext.load(std::memory_order_relaxed)) return SCE_AJM_ERROR_INVALID_CONTEXT;
    std::lock_guard lock(g_batchLock);
    return g_batches.contains(batch) ? 0 : SCE_AJM_ERROR_INVALID_BATCH;
}

int APS5_VABI sceAjmBatchErrorDump(const AjmBatchInfo* info, AjmBatchError* error) {
    (void)info;
    if (error) std::memset(error, 0, sizeof(*error));
    return 0;
}

}
