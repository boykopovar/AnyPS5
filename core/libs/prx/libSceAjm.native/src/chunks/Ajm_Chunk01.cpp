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

constexpr int SCE_AJM_ERROR_INVALID_CONTEXT = static_cast<int>(0x80930002);
constexpr int SCE_AJM_ERROR_INVALID_INSTANCE = static_cast<int>(0x80930003);
constexpr int SCE_AJM_ERROR_INVALID_BATCH = static_cast<int>(0x80930004);
constexpr int SCE_AJM_ERROR_INVALID_PARAMETER = static_cast<int>(0x80930005);
constexpr int SCE_AJM_ERROR_OUT_OF_RESOURCES = static_cast<int>(0x80930007);
constexpr int SCE_AJM_ERROR_INVALID_FLAGS = static_cast<int>(0x8093000C);
constexpr int SCE_AJM_ERROR_INVALID_PRIORITY = static_cast<int>(0x8093000E);
constexpr int SCE_AJM_ERROR_MALFORMED_BATCH = static_cast<int>(0x80930011);
constexpr int SCE_AJM_ERROR_JOB_CREATION = static_cast<int>(0x80930012);
constexpr int SCE_AJM_ERROR_INVALID_ADDRESS = static_cast<int>(0x80930016);

constexpr std::int32_t AJM_RESULT_NOT_INITIALIZED = 0x00000001;
constexpr std::int32_t AJM_RESULT_INVALID_DATA = 0x00000002;
constexpr std::int32_t AJM_RESULT_INVALID_PARAMETER = 0x00000004;
constexpr std::int32_t AJM_RESULT_PARTIAL_INPUT = 0x00000008;
constexpr std::int32_t AJM_RESULT_NOT_ENOUGH_ROOM = 0x00000010;
constexpr std::int32_t AJM_RESULT_CODEC_ERROR = 0x40000000;
constexpr std::int32_t AJM_RESULT_FATAL = static_cast<std::int32_t>(0x80000000u);

constexpr std::uint32_t CODEC_MP3 = 0;
constexpr std::uint32_t CODEC_AT9 = 1;
constexpr std::uint32_t CODEC_M4AAC = 2;
constexpr std::uint32_t CODEC_PCM = 23;
constexpr std::uint32_t CODEC_OPUS = 24;

constexpr std::uint32_t M4AAC_CONFIG_ADTS = 1;
constexpr std::uint32_t M4AAC_CONFIG_RAW = 2;
constexpr std::uint64_t M4AAC_ENABLE_SBR_DECODE = 1ull << 32;
constexpr std::uint64_t M4AAC_ENABLE_NONDELAY_OUTPUT = 1ull << 33;
constexpr std::uint64_t M4AAC_SURROUND_INTERLEAVE_ORDERS = 3ull << 34;
constexpr std::uint32_t M4AAC_DELAY_FRAMES = 2;

constexpr std::uint64_t INSTANCE_RESERVED_FLAGS = 0xF0000000FFFFC000ull;

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

struct SidebandM4aacCodecInfo {
    std::uint32_t heaac;
    std::uint32_t reserved;
};

struct SidebandResampleInfo {
    float ratio;
    std::int32_t samples;
    std::uint32_t reserved[8];
};

struct Resampler {
    float ratio = 1.0f;
    float change = 0.0f;
    std::vector<double> frames;
    double position = 1.0;
};

struct Instance {
    std::uint32_t context = 0;
    std::uint32_t codec = 0;
    std::uint64_t flags = 0;
    void* decoder = nullptr;
    atrac9_handle_t* vibrationDecoder = nullptr;
    std::vector<void*> channelDecoders;
    std::vector<std::uint32_t> channelBytes;
    std::vector<std::uint8_t> blockPcm;
    Atrac9CodecInfo info{};
    bool initialized = false;
    std::uint32_t superframeRemaining = 0;
    std::uint32_t frameInSuperframe = 0;
    std::uint64_t totalDecodedSamples = 0;
    std::uint64_t segmentSamples = 0;
    SidebandGaplessDecode gapless{};
    bool flagsReported = false;
    AVCodecContext* mp3 = nullptr;
    std::uint32_t mp3Channels = 0;
    std::uint32_t mp3SampleRate = 0;
    std::uint32_t mp3Bitrate = 0;
    AVCodecContext* opus = nullptr;
    std::uint32_t opusChannels = 0;
    std::uint32_t opusMappingFamily = 0;
    std::uint32_t opusStreams = 0;
    std::uint32_t opusCoupled = 0;
    bool opusLayoutPending = false;
    std::uint32_t opusSampleRate = 0;
    std::vector<std::uint8_t> opusPending;
    AVCodecContext* aac = nullptr;
    std::uint32_t aacConfig = 0;
    std::uint32_t aacFrequencyIndex = 0;
    std::uint32_t aacChannels = 0;
    std::uint32_t aacSampleRate = 0;
    std::uint32_t aacBitrate = 0;
    std::uint32_t aacFrameSamples = 0;
    std::uint32_t aacDelayFrames = 0;
    bool aacHeaac = false;
    std::uint32_t pcmChannels = 0;
    std::uint32_t pcmSampleRate = 0;
    bool pcmResampled = false;
    Resampler resampler;

    ~Instance() {
        if (decoder) Atrac9ReleaseHandle(decoder);
        atrac9_free(vibrationDecoder);
        for (void* channelDecoder : channelDecoders) Atrac9ReleaseHandle(channelDecoder);
        if (mp3) avcodec_free_context(&mp3);
        if (opus) avcodec_free_context(&opus);
        if (aac) avcodec_free_context(&aac);
    }
};

std::mutex& g_lock = *new std::mutex();
std::map<std::uint32_t, std::unique_ptr<Instance>>& g_instances = *new std::map<std::uint32_t, std::unique_ptr<Instance>>();
std::atomic<std::uint32_t> g_nextContext{1};
std::atomic<std::uint32_t> g_nextInstance{1};
std::atomic<std::uint32_t> g_nextBatch{1};
std::mutex& g_batchLock = *new std::mutex();
std::map<std::uint32_t, std::uint32_t>& g_batches = *new std::map<std::uint32_t, std::uint32_t>();

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
    bool console;
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

int JobRejected(const AjmBatchInfo* info, const JobHeader& header) {
    if (!info || !info->p_buffer) return SCE_AJM_ERROR_INVALID_PARAMETER;
    if (header.inputCount > 0xffu || header.outputCount > 0xffu || header.parameterSize > sizeof(header.parameters) || header.sidebandSize > 0xffffffffu) return SCE_AJM_ERROR_INVALID_PARAMETER;
    return 0;
}

int Append(AjmBatchInfo* info, const JobHeader& header, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    if (const int rejected = JobRejected(info, header)) return rejected;
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

int CheckedAppend(AjmBatchInfo* info, const JobHeader& header, const AjmBuffer* inputs, const AjmBuffer* outputs) {
    if (const int rejected = JobRejected(info, header)) return rejected;
    if ((header.inputCount && !inputs) || (header.outputCount && !outputs)) NotImplemented_nid_no_patch("AJM batch job with a null buffer list and a nonzero buffer count");
    return Append(info, header, inputs, outputs);
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

bool IsAt9Vibration(const std::uint8_t* config) {
    return config[0] == 0xFE && ((config[1] >> 1u) & 7u) >= 6u;
}

bool DescribeAt9Config(const std::uint8_t* configData, Atrac9CodecInfo& info) {
    if (IsAt9Vibration(configData)) {
        atrac9_info_t vibration{};
        if (atrac9_get_config_info(configData, &vibration) != 0 || vibration.sample_rate <= 0) return false;
        info.channels = vibration.channels;
        info.channelConfigIndex = (configData[1] >> 1u) & 7u;
        info.samplingRate = vibration.sample_rate;
        info.superframeSize = vibration.superframe_size;
        info.framesInSuperframe = vibration.frames_per_superframe;
        info.frameSamples = vibration.frame_samples;
        std::memcpy(info.configData, configData, ATRAC9_CONFIG_DATA_SIZE);
        return true;
    }
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
    atrac9_free(instance.vibrationDecoder);
    instance.vibrationDecoder = nullptr;
    for (void* channelDecoder : instance.channelDecoders) Atrac9ReleaseHandle(channelDecoder);
    instance.channelDecoders.clear();
    instance.superframeRemaining = 0;
    instance.frameInSuperframe = 0;
    unsigned char config[ATRAC9_CONFIG_DATA_SIZE];
    std::memcpy(config, instance.info.configData, sizeof(config));
    if (IsAt9Vibration(config)) {
        instance.vibrationDecoder = atrac9_init(config);
        return instance.vibrationDecoder != nullptr;
    }
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
    return instance.decoder || instance.vibrationDecoder || !instance.channelDecoders.empty();
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
    if (instance.vibrationDecoder) {
        if (encoding > 2) NotImplemented_nid_no_patch("AJM ATRAC9 vibration PCM encoding other than PCM16, PCM32 and float");
        int samples = 0;
        if (encoding == 0) samples = atrac9_decode_pcm16(instance.vibrationDecoder, data, size, reinterpret_cast<std::int16_t*>(pcm), used);
        else if (encoding == 2) samples = atrac9_decode_float(instance.vibrationDecoder, data, size, reinterpret_cast<float*>(pcm), used);
        else {
            std::array<float, 512> decoded{};
            samples = atrac9_decode(instance.vibrationDecoder, data, size, decoded.data(), used);
            if (samples > 0) {
                const auto count = static_cast<std::size_t>(samples) * instance.info.channels;
                if (count > decoded.size()) return AT9_MULTICHANNEL_BLOCK_OVERRUN;
                for (std::size_t index = 0; index < count; ++index) {
                    const auto value = static_cast<std::int32_t>(std::clamp(static_cast<double>(decoded[index]) * 65536.0, -2147483648.0, 2147483647.0));
                    std::memcpy(pcm + index * sizeof(value), &value, sizeof(value));
                }
            }
        }
        return samples < 0 ? samples : samples == instance.info.frameSamples ? 0 : AT9_MULTICHANNEL_BLOCK_OVERRUN;
    }
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
std::int32_t InitializeAac(Instance& instance, const std::uint8_t* parameters, std::uint64_t size);

void ClearResampler(Instance& instance) {
    instance.resampler.frames.clear();
    instance.resampler.position = 1.0;
}

std::int32_t InitializePcm(Instance& instance, const std::uint8_t* parameters, std::uint64_t size) {
    if (size != 12) NotImplemented_nid_no_patch("sceAjmBatchJobInitialize (codec 23 parameters other than 12 bytes)");
    std::uint32_t words[3] = {};
    std::memcpy(words, parameters, sizeof(words));
    if (words[0] != 0) NotImplemented_nid_no_patch("sceAjmBatchJobInitialize (codec 23 input format other than 0, interleaved s16)");
    if (words[1] == 0 || words[1] > 8 || words[2] == 0) return AJM_RESULT_INVALID_PARAMETER;
    instance.pcmChannels = words[1];
    instance.pcmSampleRate = words[2];
    instance.pcmResampled = false;
    instance.initialized = true;
    instance.totalDecodedSamples = 0;
    instance.segmentSamples = 0;
    instance.gapless = {};
    return 0;
}

std::int32_t InitializeInstance(Instance& instance, const std::uint8_t* parameters, std::uint64_t size) {
    ClearResampler(instance);
    if (instance.codec == CODEC_PCM) return InitializePcm(instance, parameters, size);
    if (instance.codec == CODEC_OPUS) {
        if (size < 8) return AJM_RESULT_INVALID_PARAMETER;
        std::uint32_t channels = 0;
        std::uint32_t sampleRate = 0;
        std::memcpy(&channels, parameters, sizeof(channels));
        std::memcpy(&sampleRate, parameters + 4, sizeof(sampleRate));
        if (channels == 0 || channels > 8 || sampleRate == 0) return AJM_RESULT_INVALID_PARAMETER;
        std::uint32_t mappingFamily = 0;
        if (size >= 12) std::memcpy(&mappingFamily, parameters + 8, sizeof(mappingFamily));
        if (sampleRate != 48000) NotImplemented_nid_no_patch("sceAjmBatchJobInitialize (Opus sample rate other than 48000)");
        if (mappingFamily > 1 && mappingFamily != 255) NotImplemented_nid_no_patch("sceAjmBatchJobInitialize (Opus mapping family other than 0, 1 or 255)");
        if (mappingFamily == 0 && channels > 2) return AJM_RESULT_INVALID_PARAMETER;
        instance.opusChannels = channels;
        instance.opusMappingFamily = mappingFamily;
        instance.opusSampleRate = sampleRate;
        instance.opusLayoutPending = mappingFamily == 255 && channels > 1;
        if (!instance.opusLayoutPending) OpenOpus(instance);
        instance.opusPending.clear();
        instance.initialized = true;
        instance.totalDecodedSamples = 0;
        instance.segmentSamples = 0;
        instance.gapless = {};
        return 0;
    }
    if (instance.codec == CODEC_M4AAC) return InitializeAac(instance, parameters, size);
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
    instance.segmentSamples = 0;
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

class JobInput {
public:
    JobInput(const JobHeader& job, const AjmBuffer* inputs) {
        if (job.inputCount == 1) {
            bytes = {static_cast<const std::uint8_t*>(inputs[0].ptr), static_cast<std::size_t>(inputs[0].size)};
            return;
        }
        for (std::uint32_t index = 0; index < job.inputCount; ++index) {
            const auto* data = static_cast<const std::uint8_t*>(inputs[index].ptr);
            joined.insert(joined.end(), data, data + inputs[index].size);
        }
        bytes = joined;
    }
    JobInput(const JobInput&) = delete;
    JobInput& operator=(const JobInput&) = delete;
    std::size_t size() const { return bytes.size(); }
    bool empty() const { return bytes.empty(); }
    const std::uint8_t* data() const { return bytes.data(); }
    std::uint8_t operator[](std::size_t index) const { return bytes[index]; }

private:
    std::vector<std::uint8_t> joined;
    std::span<const std::uint8_t> bytes;
};

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
    return instance.resampler.ratio != 1.0f || instance.resampler.change != 0.0f || !instance.resampler.frames.empty();
}

std::size_t ResamplerHeld(const Instance& instance, std::size_t channels) {
    const auto& resampler = instance.resampler;
    if (resampler.frames.empty() || channels == 0) return 0;
    const std::size_t frames = resampler.frames.size() / channels;
    if (resampler.position >= static_cast<double>(frames)) return 0;
    const auto next = static_cast<std::size_t>(resampler.position);
    return frames > next ? frames - next : 0;
}

void ResamplerPush(Instance& instance, const double* samples, std::size_t frames, std::size_t channels) {
    auto& stored = instance.resampler.frames;
    if (frames == 0) return;
    if (stored.empty()) stored.insert(stored.end(), samples, samples + channels);
    stored.insert(stored.end(), samples, samples + frames * channels);
}

double CatmullRom(double p0, double p1, double p2, double p3, double t) {
    return p1 + 0.5 * t * (p2 - p0 + t * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3 + t * (3.0 * (p1 - p2) + p3 - p0)));
}

void ResamplerProduce(Instance& instance, PcmOutputs& outputs, std::size_t channels, std::uint32_t encoding, std::size_t sampleBytes) {
    auto& resampler = instance.resampler;
    if (resampler.frames.empty() || channels == 0) return;
    const std::size_t frameBytes = channels * sampleBytes;
    const std::size_t available = resampler.frames.size() / channels;
    std::vector<std::uint8_t> frame(frameBytes);
    while (outputs.Room() >= frameBytes) {
        if (available <= 2 || resampler.position >= static_cast<double>(available - 2)) break;
        const auto index = static_cast<std::size_t>(resampler.position);
        const double t = resampler.position - static_cast<double>(index);
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const double p0 = resampler.frames[(index - 1) * channels + channel];
            const double p1 = resampler.frames[index * channels + channel];
            const double p2 = resampler.frames[(index + 1) * channels + channel];
            const double p3 = resampler.frames[(index + 2) * channels + channel];
            StorePcm(CatmullRom(p0, p1, p2, p3, t), frame.data() + channel * sampleBytes, encoding);
        }
        outputs.Emit(frame.data(), frameBytes);
        resampler.position += resampler.ratio;
        if (resampler.change != 0.0f) {
            resampler.ratio = std::max(0.0f, resampler.ratio + resampler.change);
        }
    }
    const auto index = resampler.position >= static_cast<double>(available) ? available : static_cast<std::size_t>(resampler.position);
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
        const std::uint64_t remaining = instance.gapless.totalSamples > instance.segmentSamples ? instance.gapless.totalSamples - instance.segmentSamples : 0;
        count = static_cast<std::size_t>(std::min<std::uint64_t>(count, remaining));
    }
    instance.segmentSamples += count;
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

// A PS4 reports an ATRAC9 stream's bitrate from its superframe: 448-byte superframes of 4 frames of 256
// samples at 48 kHz gave 168000 (Knowledge is Power: Decades, every run job's format sideband).
