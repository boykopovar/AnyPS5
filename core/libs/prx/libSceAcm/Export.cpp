#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_ACM_ERROR_BAD_CONTEXT = static_cast<int>(0x81940002);
constexpr int SCE_ACM_ERROR_BAD_POINTER = static_cast<int>(0x81940006);

std::mutex g_mutex;
std::unordered_set<AcmContextId> g_contexts;
std::atomic<uint32_t> g_nextId{1};

}

extern "C" {

int APS5_VABI sceAcmBatchStartBuffer(AcmContextId context, const void* batch_commands, size_t batch_size, AcmBatchError* batch_error, AcmBatchId* batch) {
    (void)batch_commands;
    (void)batch_size;
    if (!batch) return SCE_ACM_ERROR_BAD_POINTER;
    if (context == 0) return SCE_ACM_ERROR_BAD_CONTEXT;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_contexts.contains(context)) return SCE_ACM_ERROR_BAD_CONTEXT;
    AcmBatchId id = g_nextId.fetch_add(1, std::memory_order_relaxed);
    while (id == 0) id = g_nextId.fetch_add(1, std::memory_order_relaxed);
    if (batch_error) std::memset(batch_error, 0, sizeof(*batch_error));
    *batch = id;
    return 0;
}

int APS5_VABI sceAcmBatchStartBuffers(AcmContextId context, uint32_t batch_info_count, const AcmBatchInfo* const batch_info[], AcmBatchError* batch_error, AcmBatchId* batch) {
    (void)batch_info_count;
    (void)batch_info;
    if (!batch) return SCE_ACM_ERROR_BAD_POINTER;
    if (context == 0) return SCE_ACM_ERROR_BAD_CONTEXT;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_contexts.contains(context)) return SCE_ACM_ERROR_BAD_CONTEXT;
    AcmBatchId id = g_nextId.fetch_add(1, std::memory_order_relaxed);
    while (id == 0) id = g_nextId.fetch_add(1, std::memory_order_relaxed);
    if (batch_error) std::memset(batch_error, 0, sizeof(*batch_error));
    *batch = id;
    return 0;
}

int APS5_VABI sceAcmBatchWait(AcmContextId context, AcmBatchId batch, uint32_t timeout) {
    (void)batch;
    (void)timeout;
    if (context == 0) return SCE_ACM_ERROR_BAD_CONTEXT;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_contexts.contains(context)) return SCE_ACM_ERROR_BAD_CONTEXT;
    return 0;
}

int APS5_VABI sceAcmContextCreate(AcmContextId* context) {
    if (!context) return SCE_ACM_ERROR_BAD_POINTER;
    AcmContextId id = g_nextId.fetch_add(1, std::memory_order_relaxed);
    while (id == 0) id = g_nextId.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_contexts.insert(id);
    }
    *context = id;
    return 0;
}

int APS5_VABI sceAcmContextDestroy(AcmContextId context) {
    if (context == 0) return SCE_ACM_ERROR_BAD_CONTEXT;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_contexts.find(context);
    if (it == g_contexts.end()) return SCE_ACM_ERROR_BAD_CONTEXT;
    g_contexts.erase(it);
    return 0;
}

int APS5_VABI sceAcm_ConvReverb_SharedInput(AcmBatchInfo* batch, std::uint32_t mode, AcmReverbInput* input, std::uint32_t outputCount, const AcmReverbFilter* const* filters, const float* gains, const AcmReverbOutput* const* outputs) {
    return 0;
}

int APS5_VABI sceAcm_FFT(AcmBatchInfo* batch, std::int32_t transformSize, std::uint32_t transformCount, std::int32_t inputFormat, const void* const* inputs, std::int32_t outputFormat, void* const* outputs, std::int32_t flags) {
    if (!batch || transformCount == 0 || transformCount > 0x7FFFFFFF || !inputs || !outputs || (flags & 3) == 3 || inputFormat < 0 || inputFormat > 1 || outputFormat < 0 || outputFormat > 1) return SCE_ACM_ERROR_BAD_POINTER;
    if (transformSize != 512 && transformSize != 1024 && transformSize != 2048) return SCE_ACM_ERROR_BAD_POINTER;
    const bool realInput = (flags & 1) != 0;
    if (transformSize == 2048 && !realInput) return SCE_ACM_ERROR_BAD_POINTER;
    const std::uint32_t inputBytes = transformSize * (inputFormat == 1 ? 2 : 4);
    const std::uint32_t outputBytes = transformSize * (outputFormat == 1 ? 2 : 4);
    const std::uint32_t inputBlocks = (inputBytes >> 9) >> (realInput ? 1 : 0);
    const std::uint32_t outputBlocks = outputBytes >> 9;
    std::uint32_t transformCommand;
    std::uint32_t inputCommand;
    std::uint32_t outputCommand;
    std::uint32_t setupCommand;
    std::uint32_t sizeCode;
    if (transformSize == 512) {
        sizeCode = 4;
        transformCommand = realInput ? 7 : 11;
        inputCommand = realInput ? 2 * inputFormat : 9 + inputFormat;
        outputCommand = 4 | outputFormat;
        setupCommand = realInput ? 6 : 12;
    } else if (transformSize == 1024) {
        sizeCode = 5;
        transformCommand = realInput ? 8 : 13;
        inputCommand = realInput ? 2 * inputFormat : 14 | inputFormat;
        outputCommand = 5 | (2 * outputFormat);
        setupCommand = realInput ? 10 : 12;
    } else {
        sizeCode = 6;
        transformCommand = 6;
        inputCommand = 2 * inputFormat;
        outputCommand = 4 | outputFormat;
        setupCommand = 8;
    }
    auto* buffer = static_cast<std::byte*>(batch->buffer);
    auto offset = batch->offset;
    const auto capacity = batch->buffer_size;
    const auto appendWord = [&](std::uint64_t value) {
        if (offset > capacity || capacity - offset < sizeof(value)) return false;
        std::memcpy(buffer + offset, &value, sizeof(value));
        offset += sizeof(value);
        return true;
    };
    std::uint64_t header = static_cast<std::uint64_t>(sizeCode) << 8;
    if (!(flags & 8)) {
        const auto encodedCount = transformCount < 64000 ? transformCount : 64000;
        header |= (std::uint64_t{1} << 16) | (static_cast<std::uint64_t>(transformSize) << 8) | (static_cast<std::uint64_t>(flags & 6) << 19) | (static_cast<std::uint64_t>(flags & 1) << 23) | (std::uint64_t{1} << (inputFormat == 1 ? 24 : 26)) | (std::uint64_t{1} << (outputFormat == 1 ? 25 : 27)) | (static_cast<std::uint64_t>(encodedCount) << 28);
    }
    constexpr int noSpace = static_cast<int>(0x81940009);
    constexpr int badAlignment = static_cast<int>(0x8194000A);
    if (!appendWord(header) || !appendWord(10 | (setupCommand << 8))) return noSpace;
    for (std::uint32_t index = 0; index < transformCount; ++index) {
        const auto inputIndex = static_cast<std::size_t>(index) * ((flags & 2) ? 2 : 1);
        const auto input = reinterpret_cast<std::uintptr_t>(inputs[inputIndex]);
        const auto secondInput = (flags & 2) ? reinterpret_cast<std::uintptr_t>(inputs[inputIndex + 1]) : 0;
        if (!(flags & 2) && !input) return SCE_ACM_ERROR_BAD_POINTER;
        if ((input | secondInput) & 127) return badAlignment;
        const auto inputRecordSize = (flags & 2) ? 24u : 16u;
        if (offset > capacity || capacity - offset < inputRecordSize) return noSpace;
        appendWord(((flags & 2) ? 7 : 6) | (static_cast<std::uint64_t>(inputBlocks) << 32) | ((flags & 2) ? static_cast<std::uint64_t>(inputBytes / 2) << 48 : 0));
        appendWord(input);
        if (flags & 2) appendWord(secondInput);
        if (!appendWord(10 | (inputCommand << 8)) || !appendWord(10 | (transformCommand << 8)) || !appendWord(10 | (outputCommand << 8))) return noSpace;
        const auto outputIndex = static_cast<std::size_t>(index) * ((flags & 4) ? 2 : 1);
        const auto output = reinterpret_cast<std::uintptr_t>(outputs[outputIndex]);
        const auto secondOutput = (flags & 4) ? reinterpret_cast<std::uintptr_t>(outputs[outputIndex + 1]) : 0;
        if ((output | secondOutput) & 127) return badAlignment;
        const auto outputRecordSize = (flags & 4) ? 24u : 16u;
        if (offset > capacity || capacity - offset < outputRecordSize) return noSpace;
        appendWord(((flags & 4) ? 9 : 8) | (static_cast<std::uint64_t>(outputBlocks) << 32) | ((flags & 4) ? static_cast<std::uint64_t>(outputBytes / 2) << 48 : 0));
        appendWord(output);
        if (flags & 4) appendWord(secondOutput);
    }
    batch->offset = offset;
    return 0;
}

int APS5_VABI sceAcm_Panner(AcmBatchInfo* batch, std::uint32_t sourceCount, const void* const* sources, std::uint32_t sourceMode, std::uint32_t outputMode, std::uint32_t channelCount, const void* const* sourceParameters, const void* const* sourceMatrices, const void* const* inputChannels, void* const* outputChannels) {
    if (!batch || sourceCount == 0 || sourceCount > 65535 || sourceMode > 2 || outputMode > 1 || channelCount == 0 || channelCount > 36 || !sources || !sourceParameters || !sourceMatrices || !outputChannels) return SCE_ACM_ERROR_BAD_POINTER;
    std::uintptr_t alignment = reinterpret_cast<std::uintptr_t>(sources) | reinterpret_cast<std::uintptr_t>(sourceParameters) | reinterpret_cast<std::uintptr_t>(sourceMatrices) | reinterpret_cast<std::uintptr_t>(inputChannels) | reinterpret_cast<std::uintptr_t>(outputChannels);
    for (std::uint32_t index = 0; index < sourceCount; ++index) {
        if (!sources[index] || !sourceParameters[index] || !sourceMatrices[index]) return SCE_ACM_ERROR_BAD_POINTER;
        std::uint32_t parameterMode;
        std::memcpy(&parameterMode, sourceParameters[index], sizeof(parameterMode));
        if (parameterMode > 1) return SCE_ACM_ERROR_BAD_POINTER;
        alignment |= reinterpret_cast<std::uintptr_t>(sources[index]) | reinterpret_cast<std::uintptr_t>(sourceParameters[index]) | reinterpret_cast<std::uintptr_t>(sourceMatrices[index]);
    }
    for (std::uint32_t index = 0; index < channelCount; ++index) {
        if (!outputChannels[index]) return SCE_ACM_ERROR_BAD_POINTER;
        alignment |= reinterpret_cast<std::uintptr_t>(outputChannels[index]);
        if (inputChannels) alignment |= reinterpret_cast<std::uintptr_t>(inputChannels[index]);
    }
    if (alignment & 127) return static_cast<int>(0x8194000A);
    const auto offset = batch->offset;
    const auto capacity = batch->buffer_size;
    if (offset > capacity || capacity - offset < 8) return static_cast<int>(0x81940009);
    auto* buffer = static_cast<std::byte*>(batch->buffer);
    const auto writeWord = [buffer](std::size_t position, std::uint64_t value) { std::memcpy(buffer + position, &value, sizeof(value)); };
    const auto encodedCount = sourceCount < 255 ? sourceCount : 255;
    writeWord(offset, (static_cast<std::uint64_t>(channelCount) << 24) | (static_cast<std::uint64_t>(encodedCount) << 16) | (std::uint64_t{1} << (sourceMode + 32)) | (std::uint64_t{1} << (outputMode + 35)) | 0x300);
    if (capacity < 128 || capacity - offset < 152) return static_cast<int>(0x81940009);
    const auto descriptorOffset = capacity - 128;
    std::memset(buffer + descriptorOffset, 0, 128);
    writeWord(descriptorOffset, sourceCount);
    writeWord(descriptorOffset + 8, reinterpret_cast<std::uintptr_t>(inputChannels));
    writeWord(descriptorOffset + 16, reinterpret_cast<std::uintptr_t>(outputChannels));
    writeWord(descriptorOffset + 24, reinterpret_cast<std::uintptr_t>(sources));
    writeWord(descriptorOffset + 32, sourceMode | (static_cast<std::uint64_t>(outputMode) << 32));
    writeWord(descriptorOffset + 40, channelCount);
    writeWord(descriptorOffset + 48, reinterpret_cast<std::uintptr_t>(sourceParameters));
    writeWord(descriptorOffset + 56, reinterpret_cast<std::uintptr_t>(sourceMatrices));
    writeWord(offset + 8, 0x8000000000000D);
    writeWord(offset + 16, reinterpret_cast<std::uintptr_t>(buffer + descriptorOffset));
    if (capacity - offset < 160) return static_cast<int>(0x81940009);
    writeWord(offset + 24, 10);
    batch->offset = offset + 32;
    batch->buffer_size = descriptorOffset;
    return 0;
}

int APS5_VABI sceAcmBatchJobPriority(AcmBatchInfo* batch, std::uint32_t priority) {
    (void)batch;
    (void)priority;
    return 0;
}

int APS5_VABI sceAcm_ConvReverb_SharedIr() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAcm_IFFT() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


int APS5_VABI sceAcmBatchJobNotification() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
