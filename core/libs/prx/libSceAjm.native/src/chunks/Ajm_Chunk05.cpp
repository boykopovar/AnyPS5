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
int APS5_VABI sceAjmInitialize(int64_t reserved, uint32_t* context) {
    (void)reserved;
    if (!context) return SCE_AJM_ERROR_INVALID_PARAMETER;
    std::lock_guard<std::mutex> lock(g_lock);
    const uint32_t id = g_nextContext.fetch_add(1, std::memory_order_relaxed);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(context, &id, sizeof(id));
    errno = savedErrno;
    if (!copied) {
        g_nextContext.fetch_sub(1, std::memory_order_relaxed);
        return SCE_AJM_ERROR_INVALID_PARAMETER;
    }
    return 0;
}

int APS5_VABI sceAjmFinalize(uint32_t context) {
    (void)context;
    return 0;
}

int APS5_VABI sceAjmModuleRegister(uint32_t context, uint32_t codec, int64_t reserved) {
    (void)context;
    (void)codec;
    if (reserved != 0) return SCE_AJM_ERROR_INVALID_PARAMETER;
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
    if (!instance) return SCE_AJM_ERROR_INVALID_PARAMETER;
    if ((flags & 0x380u) >= 0x180u || (flags & 0x2400u) == 0x2000u || (flags & INSTANCE_RESERVED_FLAGS) != 0) return SCE_AJM_ERROR_INVALID_FLAGS;
    if (context == 0 || context >= g_nextContext.load(std::memory_order_relaxed)) return SCE_AJM_ERROR_INVALID_CONTEXT;
    auto created = std::make_unique<Instance>();
    created->context = context;
    created->codec = codec;
    created->flags = flags;
    if (codec == CODEC_MP3) {
        OpenMp3(*created);
        created->initialized = true;
    }
    if (codec == CODEC_M4AAC) CheckAacFlags(flags);
    std::lock_guard lock(g_lock);
    for (std::uint32_t attempt = 0; attempt < 0x4000; ++attempt) {
        const std::uint32_t id = (codec << 14) | (g_nextInstance.fetch_add(1, std::memory_order_relaxed) & 0x3FFF);
        if (g_instances.contains(id)) continue;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(instance, &id, sizeof(id));
        errno = savedErrno;
        if (!copied) return SCE_AJM_ERROR_INVALID_PARAMETER;
        g_instances.emplace(id, std::move(created));
        AJM_TRACE("[ajm] instance create: context %u, codec %u, flags 0x%llx -> instance %u\n", context, codec, static_cast<unsigned long long>(flags), id);
        return 0;
    }
    return SCE_AJM_ERROR_OUT_OF_RESOURCES;
}

int APS5_VABI sceAjmInstanceDestroy(uint32_t context, uint32_t instance) {
    if (context == 0 || context >= g_nextContext.load(std::memory_order_relaxed)) return SCE_AJM_ERROR_INVALID_CONTEXT;
    AJM_TRACE("[ajm] instance %u destroy\n", instance);
    std::lock_guard lock(g_lock);
    const auto found = g_instances.find(instance);
    if (found == g_instances.end() || found->second->context != context) return SCE_AJM_ERROR_INVALID_INSTANCE;
    g_instances.erase(found);
    return 0;
}

int APS5_VABI sceAjmDecAt9ParseConfigData(const void* config_data, AjmDecAt9ConfigDataInfo* config_info) {
    if (!config_data || !config_info) return SCE_AJM_ERROR_INVALID_PARAMETER;
    std::uint8_t config[ATRAC9_CONFIG_DATA_SIZE];
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(config, config_data, sizeof(config))) {
        errno = savedErrno;
        return SCE_AJM_ERROR_INVALID_PARAMETER;
    }
    Atrac9CodecInfo info{};
    const bool valid = DescribeAt9Config(config, info);
    AJM_TRACE("[ajm] parse config %02x %02x %02x %02x -> %s, %d channels, %d Hz, superframe %d bytes (%d frames of %d samples)\n", config[0], config[1], config[2], config[3], valid ? "ok" : "invalid", info.channels, info.samplingRate, info.superframeSize, info.framesInSuperframe, info.frameSamples);
    if (!valid) {
        errno = savedErrno;
        return SCE_AJM_ERROR_INVALID_PARAMETER;
    }
    AjmDecAt9ConfigDataInfo localConfigInfo{};
    localConfigInfo.channels = static_cast<std::uint32_t>(info.channels);
    localConfigInfo.sample_rate = static_cast<std::uint32_t>(info.samplingRate);
    localConfigInfo.frame_samples_per_channel = static_cast<std::uint32_t>(info.frameSamples);
    localConfigInfo.superframe_samples_per_channel = static_cast<std::uint32_t>(info.frameSamples * info.framesInSuperframe);
    localConfigInfo.superframe_size = static_cast<std::uint32_t>(info.superframeSize);
    const bool copied = GuestCopyOut_nid_no_patch(config_info, &localConfigInfo, sizeof(localConfigInfo));
    errno = savedErrno;
    if (!copied) {
        return SCE_AJM_ERROR_INVALID_PARAMETER;
    }
    return 0;
}

int APS5_VABI sceAjmDecMp3ParseFrame(const uint8_t* stream, uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame) {
    return ParseMp3Header(stream, streamSize, parseOfl, frame);
}

int APS5_VABI sceAjmBatchInitialize(void* buffer, size_t size, AjmBatchInfo* info) {
    if (!buffer || !info) return SCE_AJM_ERROR_INVALID_PARAMETER;
    AjmBatchInfo local{};
    local.p_buffer = buffer;
    local.offset = 0;
    local.size = size;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &local, sizeof(local));
    errno = savedErrno;
    if (!copied) return SCE_AJM_ERROR_INVALID_PARAMETER;
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
    return CheckedAppend(info, header, &input, nullptr);
}

int AppendRun(AjmBatchInfo* info, std::uint32_t instance, std::uint64_t flags, const AjmBuffer* input_buffers, std::size_t input_buffers_num, const AjmBuffer* output_buffers, std::size_t output_buffers_num, void* sideband_output, std::size_t sideband_output_size) {
    if (input_buffers_num > 0xffu || output_buffers_num > 0xffu) return SCE_AJM_ERROR_INVALID_PARAMETER;
    auto header = MakeHeader(JobKind::Run, instance, sideband_output, sideband_output_size);
    header.flags = flags;
    header.inputCount = static_cast<std::uint32_t>(input_buffers_num);
    header.outputCount = static_cast<std::uint32_t>(output_buffers_num);
    if (const int rejected = JobRejected(info, header)) return rejected;
    if ((input_buffers_num && !input_buffers) || (output_buffers_num && !output_buffers)) throw std::runtime_error("AJM batch job with a null buffer list and a nonzero buffer count is not implemented");
    return Append(info, header, input_buffers, output_buffers);
}

int APS5_VABI sceAjmBatchJobRunSplit(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size) {
    if (!info || sideband_output_size < sizeof(SidebandResult)) return SCE_AJM_ERROR_INVALID_PARAMETER;
    if (input_buffers_num != 0 && !input_buffers) return SCE_AJM_ERROR_INVALID_PARAMETER;
    if (output_buffers_num != 0 && !output_buffers) return SCE_AJM_ERROR_INVALID_PARAMETER;
    const std::uint64_t buffers = static_cast<std::uint64_t>(input_buffers_num) + output_buffers_num;
    const std::uint64_t limit = UINT64_MAX - sizeof(PackedJob);
    if (buffers < input_buffers_num || info->offset > limit || buffers > (limit - info->offset) / sizeof(AjmBuffer)) NotImplemented_nid_no_patch("sceAjmBatchJobRunSplit (a job size past 64 bits)");
    const std::uint64_t end = info->offset + sizeof(PackedJob) + buffers * sizeof(AjmBuffer);
    if (end > info->size) {
        info->offset = end;
        return SCE_AJM_ERROR_JOB_CREATION;
    }
    if (input_buffers_num > 0xffu || output_buffers_num > 0xffu) NotImplemented_nid_no_patch("sceAjmBatchJobRunSplit (more than 255 input or output buffers in one job)");
    return AppendRun(info, instance, flags, input_buffers, input_buffers_num, output_buffers, output_buffers_num, sideband_output, sideband_output_size);
}

int APS5_VABI sceAjmBatchJobRun(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size) {
    const AjmBuffer input{const_cast<void*>(data_input), data_input_size};
    const AjmBuffer output{data_output, data_output_size};
    return AppendRun(info, instance, flags, &input, 1, &output, 1, sideband_output, sideband_output_size);
}

int APS5_VABI sceAjmBatchJobSetResampleParameters(AjmBatchInfo* info, uint32_t instance, float ratio, uint32_t flags, void* result) {
    (void)flags;
    if (!std::isfinite(ratio) || ratio <= 0.0f) return SCE_AJM_ERROR_INVALID_PARAMETER;
    auto header = MakeHeader(JobKind::SetResampleParameters, instance, result, sizeof(SidebandResult));
    std::memcpy(header.parameters, &ratio, sizeof(ratio));
    header.parameterSize = sizeof(ratio);
    return Append(info, header, nullptr, nullptr);
}

int APS5_VABI sceAjmBatchJobSetResampleParametersEx(AjmBatchInfo* info, uint32_t instance, float ratio_start, float ratio_change_per_sample, uint32_t flags, void* result) {
    (void)flags;
    if (!std::isfinite(ratio_start) || ratio_start <= 0.0f || !std::isfinite(ratio_change_per_sample)) return SCE_AJM_ERROR_INVALID_PARAMETER;
    auto header = MakeHeader(JobKind::SetResampleParameters, instance, result, sizeof(SidebandResult));
    const float parameters[2] = {ratio_start, ratio_change_per_sample};
    std::memcpy(header.parameters, parameters, sizeof(parameters));
    header.parameterSize = sizeof(parameters);
    return Append(info, header, nullptr, nullptr);
}

int APS5_VABI sceAjmBatchJobGetResampleInfo(AjmBatchInfo* info, uint32_t instance, void* result) {
    return Append(info, MakeHeader(JobKind::GetResampleInfo, instance, result, sizeof(SidebandResult) + sizeof(SidebandResampleInfo)), nullptr, nullptr);
}

int APS5_VABI sceAjmBatchJobDecode(AjmBatchInfo* info, uint32_t instance, const void* bitstream_input, size_t bitstream_input_size, void* pcm_output, size_t pcm_output_size, void* result) {
    return sceAjmBatchJobRun(info, instance, SIDEBAND_STREAM, bitstream_input, bitstream_input_size, pcm_output, pcm_output_size, result, sizeof(SidebandResult) + sizeof(SidebandStream));
}

int APS5_VABI sceAjmBatchJobDecodeSplit(AjmBatchInfo* info, uint32_t instance, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* result) {
    return AppendRun(info, instance, SIDEBAND_STREAM, input_buffers, input_buffers_num, output_buffers, output_buffers_num, result, sizeof(SidebandResult) + sizeof(SidebandStream));
}

int APS5_VABI sceAjmBatchJobDecodeSingle(AjmBatchInfo* info, uint32_t instance, const void* bitstream_input, size_t bitstream_input_size, void* pcm_output, size_t pcm_output_size, void* result) {
    return sceAjmBatchJobRun(info, instance, SIDEBAND_STREAM, bitstream_input, bitstream_input_size, pcm_output, pcm_output_size, result, sizeof(SidebandResult) + sizeof(SidebandStream));
}

int APS5_VABI sceAjmBatchJobGetGaplessDecode(AjmBatchInfo* info, uint32_t instance, void* result) {
    return AppendRun(info, instance, SIDEBAND_GAPLESS_DECODE, nullptr, 0, nullptr, 0, result, sizeof(SidebandResult) + sizeof(SidebandGaplessDecode));
}

int APS5_VABI sceAjmBatchJobGetInfo(AjmBatchInfo* info, uint32_t instance, void* result) {
    return AppendRun(info, instance, SIDEBAND_FORMAT, nullptr, 0, nullptr, 0, result, sizeof(SidebandResult) + sizeof(SidebandFormat));
}

int APS5_VABI sceAjmBatchJobGetCodecInfo(AjmBatchInfo* info, uint32_t instance, void* result, size_t result_size) {
    return AppendRun(info, instance, RUN_GET_CODEC_INFO, nullptr, 0, nullptr, 0, result, result_size);
}

int APS5_VABI sceAjmBatchJobGetStatistics(AjmBatchInfo* info, float interval, void* result) {
    (void)interval;
    return CheckedAppend(info, MakeHeader(JobKind::GetStatistics, 0, result, 24), nullptr, nullptr);
}

int APS5_VABI sceAjmBatchStart(uint32_t context, const AjmBatchInfo* info, int priority, AjmBatchError* error, uint32_t* batch) {
    if (context == 0 || context >= g_nextContext.load(std::memory_order_relaxed)) return SCE_AJM_ERROR_INVALID_CONTEXT;
    if (!info || !batch || !info->p_buffer || info->offset > info->size || info->offset == 0) return SCE_AJM_ERROR_INVALID_PARAMETER;
    if (priority < SCE_AJM_PRIORITY_MIN || priority > SCE_AJM_PRIORITY_MAX) return SCE_AJM_ERROR_INVALID_PRIORITY;
    AJM_TRACE("[ajm] batch start: context %u, priority %d, %llu of %llu buffer bytes used\n", context, priority, static_cast<unsigned long long>(info->offset), static_cast<unsigned long long>(info->size));
    const auto* cursor = static_cast<const std::uint8_t*>(info->p_buffer);
    auto remaining = info->offset;
    std::vector<AjmBuffer> buffers;
    while (remaining != 0) {
        if (remaining < sizeof(PackedJob)) NotImplemented_nid_no_patch("AJM batch contains a truncated private job header");
        PackedJob record;
        std::memcpy(&record, cursor, sizeof(record));
        const std::size_t bufferCount = static_cast<std::size_t>(record.inputCount) + record.outputCount;
        const std::size_t parameterBytes = (static_cast<std::size_t>(record.parameterSize) + 7u) & ~std::size_t{7};
        const std::size_t requiredBytes = sizeof(PackedJob) + bufferCount * sizeof(AjmBuffer) + parameterBytes;
        if (record.parameterSize > sizeof(JobHeader::parameters) || record.bytes < requiredBytes || record.bytes > remaining) NotImplemented_nid_no_patch("AJM batch contains an invalid private job size");
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
        remaining -= record.bytes;
        if (remaining != 0) cursor += record.bytes;
    }
    return RegisterBatch(context, error, batch);
}

int APS5_VABI sceAjmBatchWait(uint32_t context, uint32_t batch, uint32_t timeout, AjmBatchError* error) {
    if (context == 0 || context >= g_nextContext.load(std::memory_order_relaxed)) return SCE_AJM_ERROR_INVALID_CONTEXT;
    (void)timeout;
    {
        std::lock_guard lock(g_batchLock);
        const auto found = g_batches.find(batch);
        if (found == g_batches.end() || found->second != context) return SCE_AJM_ERROR_INVALID_BATCH;
        g_batches.erase(found);
    }
    if (error) std::memset(error, 0, sizeof(*error));
    return 0;
}

int APS5_VABI sceAjmBatchCancel(uint32_t context, uint32_t batch) {
    if (context == 0 || context >= g_nextContext.load(std::memory_order_relaxed)) return SCE_AJM_ERROR_INVALID_CONTEXT;
    std::lock_guard lock(g_batchLock);
    const auto found = g_batches.find(batch);
    return found != g_batches.end() && found->second == context ? 0 : SCE_AJM_ERROR_INVALID_BATCH;
}

int APS5_VABI sceAjmBatchErrorDump(const AjmBatchInfo* info, AjmBatchError* error) {
    (void)info;
    if (error) {
        constexpr AjmBatchError localError{};
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(error, &localError, sizeof(localError));
        errno = savedErrno;
        if (!copied) return SCE_AJM_ERROR_INVALID_PARAMETER;
    }
    return 0;
}

void* APS5_VABI sceAjmBatchJobControlBufferRa(void* position, uint32_t instance, uint64_t flags, const void* sideband_input, size_t sideband_input_size, void* sideband_output, size_t sideband_output_size, void* return_address) {
    std::vector<std::uint8_t> job = LoadConsoleJob(position, 2 * CONSOLE_WORDS_BYTES + (return_address ? CONSOLE_BUFFER_BYTES : 0) + 2 * CONSOLE_BUFFER_BYTES);
    auto* at = BeginConsoleJob(job.data(), return_address);
    at = KeepConsoleBuffer(at, ConsoleChunk::InputControlBuffer, sideband_input, sideband_input_size);
    at = KeepConsoleFlags(at, ConsoleChunk::ControlFlags, flags & CONSOLE_CONTROL_FLAGS);
    at = KeepConsoleBuffer(at, ConsoleChunk::OutputControlBuffer, sideband_output, sideband_output_size);
    EndConsoleJob(job.data(), at, instance);
    return StoreConsoleJob(position, job);
}

void* APS5_VABI sceAjmBatchJobRunSplitBufferRa(void* position, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size, void* return_address) {
    if ((input_buffers_num && !input_buffers) || (output_buffers_num && !output_buffers)) NotImplemented_nid_no_patch("AJM batch job with a null buffer list and a nonzero buffer count");
    return BuildRunJob(position, instance, flags, input_buffers, input_buffers_num, output_buffers, output_buffers_num, sideband_output, sideband_output_size, return_address);
}

void* APS5_VABI sceAjmBatchJobRunBufferRa(void* position, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size, void* return_address) {
    const AjmBuffer input{const_cast<void*>(data_input), data_input_size};
    const AjmBuffer output{data_output, data_output_size};
    return BuildRunJob(position, instance, flags, &input, 1, &output, 1, sideband_output, sideband_output_size, return_address);
}

void* APS5_VABI sceAjmBatchJobInlineBuffer(void* position, const void* data_input, size_t data_input_size, const void** batch_address) {
    const std::size_t size = (data_input_size + 7) & ~std::size_t{7};
    std::vector<std::uint8_t> header = LoadConsoleJob(position, CONSOLE_WORDS_BYTES);
    KeepConsoleWords(header.data(), ~0x1Fu, static_cast<std::uint32_t>(ConsoleChunk::InlineBuffer), static_cast<std::uint32_t>(size));
    auto* at = static_cast<std::uint8_t*>(StoreConsoleJob(position, header));
    const auto source = reinterpret_cast<std::uintptr_t>(data_input);
    const auto destination = reinterpret_cast<std::uintptr_t>(at);
    std::array<std::uint8_t, 4096> staging;
    for (std::size_t offset = 0; offset < data_input_size;) {
        const std::size_t count = std::min(staging.size(), data_input_size - offset);
        CopyBatchIn(staging.data(), reinterpret_cast<const void*>(source + offset), count, "AJM inline buffer input is unreadable");
        CopyBatchOut(reinterpret_cast<void*>(destination + offset), staging.data(), count, "AJM inline buffer data is unwritable");
        offset += count;
    }
    const void* address = at;
    if (batch_address) CopyBatchOut(batch_address, &address, sizeof(address), "AJM inline buffer address output is unwritable");
    return at + size;
}

int APS5_VABI sceAjmBatchStartBuffer(uint32_t context, const uint8_t* batch, uint32_t batch_size, int priority, AjmBatchError* error, uint32_t* batch_id) {
    if (context == 0 || context >= g_nextContext.load(std::memory_order_relaxed)) return SCE_AJM_ERROR_INVALID_CONTEXT;
    if (!batch_id || !batch) return SCE_AJM_ERROR_INVALID_PARAMETER;
    AJM_TRACE("[ajm] batch start buffer: context %u, priority %d, %u bytes\n", context, priority, batch_size);
    std::vector<ConsoleJob> jobs;
    ConsoleRefusal refusal;
    const std::uint8_t* at = batch;
    const std::uint8_t* end = batch + batch_size;
    bool accepted = batch_size % 8 == 0;
    while (accepted && at < end) {
        refusal = ConsoleRefusal{};
        refusal.job = at;
        if (end - at < 8) {
            accepted = false;
            break;
        }
        std::uint32_t words[2];
        std::memcpy(words, at, sizeof(words));
        const std::uint8_t* body = at + sizeof(words);
        if (words[1] > static_cast<std::size_t>(end - body)) {
            accepted = false;
            break;
        }
        const auto kind = ConsoleChunkKind(words[0]);
        if (kind == ConsoleChunk::Job) {
            accepted = ReadConsoleJob(ConsoleChunkPayload(words[0]), at, body + words[1], jobs.emplace_back(), refusal);
        } else if (kind != ConsoleChunk::InlineBuffer) {
            NotImplemented_nid_no_patch(("sceAjmBatchStartBuffer (batch chunk kind " + std::to_string(static_cast<unsigned>(kind)) + ")").c_str());
        }
        at = body + words[1];
    }
    if (!accepted) {
        if (error) *error = AjmBatchError{refusal.code, refusal.job, refusal.offset, refusal.returnAddress};
        return SCE_AJM_ERROR_MALFORMED_BATCH;
    }
    for (const ConsoleJob& job : jobs) Execute(job.header, job.buffers.data(), job.buffers.data() + job.header.inputCount);
    return RegisterBatch(context, error, batch_id);

