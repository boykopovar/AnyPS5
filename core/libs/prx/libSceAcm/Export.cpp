#include "prx/libSceAcm/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <mutex>
#include <numeric>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_ACM_ERROR_BAD_CONTEXT = static_cast<int>(0x81940002);
constexpr int SCE_ACM_ERROR_BAD_POINTER = static_cast<int>(0x81940006);
constexpr int SCE_ACM_ERROR_BATCH_FULL = static_cast<int>(0x81940009);
constexpr int SCE_ACM_ERROR_ALIGNMENT = static_cast<int>(0x8194000A);
constexpr std::size_t ReverbDescriptorSize = 0x280;

std::mutex g_mutex;
std::unordered_set<AcmContextId> g_contexts;
std::atomic<uint32_t> g_nextId{1};

struct ReverbEntry {
    const void* samples;
    std::uint32_t length;
    std::uint32_t startOffset;
    std::uint32_t total;
    const void* metadata;
    const void* filterMetadata;
    void* workspace;
    void* output;
    float gain;
    bool wideSamples;
};

std::uint64_t Address(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

int AppendReverbJob(AcmBatchInfo& batch, std::uint32_t blockSize, const ReverbEntry& input, const ReverbEntry* entries, std::uint32_t count) {
    const auto offset = batch.offset;
    const auto capacity = batch.buffer_size;
    if (capacity < ReverbDescriptorSize || offset > capacity - ReverbDescriptorSize || capacity - ReverbDescriptorSize - offset < 40) return SCE_ACM_ERROR_BATCH_FULL;
    std::array<std::uint32_t, 8> order{};
    std::iota(order.begin(), order.begin() + count, 0u);
    std::stable_sort(order.begin(), order.begin() + count, [entries](std::uint32_t left, std::uint32_t right) { return entries[left].length > entries[right].length; });
    std::array<std::byte, ReverbDescriptorSize> descriptor{};
    const auto put = [&descriptor](std::size_t position, auto value) { std::memcpy(descriptor.data() + position, &value, sizeof(value)); };
    const auto putEntry = [&put](std::uint32_t slot, const ReverbEntry& entry, std::uint32_t length) {
        put(8 * slot, Address(entry.samples));
        put(0x48 + 4 * slot, length);
        put(0x80 + 8 * slot, Address(entry.metadata));
        put(0xC8 + 4 * slot, entry.startOffset);
        put(0x100 + 8 * slot, Address(entry.filterMetadata));
        put(0x148 + 4 * slot, entry.total);
    };
    std::uint32_t longest = 0;
    std::uint32_t wideMask = input.wideSamples ? 1u : 0u;
    bool hasMetadata = input.metadata || input.filterMetadata;
    for (std::uint32_t position = 0; position < count; ++position) {
        const auto& entry = entries[order[position]];
        longest = std::max(longest, entry.length);
        putEntry(position + 1, entry, entry.length);
        descriptor[0x16C + order[position]] = static_cast<std::byte>(position);
        wideMask |= (entry.wideSamples ? 1u : 0u) << (position + 1);
        hasMetadata = hasMetadata || entry.metadata || entry.filterMetadata;
        put(0x180 + 8 * position, Address(entry.workspace));
        put(0x1C0 + 4 * position, entry.gain);
        put(0x200 + 8 * position, Address(entry.output));
    }
    putEntry(0, input, longest);
    put(0x174, 2 * blockSize);
    put(0x178, wideMask);
    put(0x17C, count + 1);
    auto* buffer = static_cast<std::byte*>(batch.buffer);
    const auto descriptorOffset = capacity - ReverbDescriptorSize;
    std::memcpy(buffer + descriptorOffset, descriptor.data(), descriptor.size());
    const std::array<std::uint64_t, 5> words{0x028000000000000D, Address(buffer + descriptorOffset), blockSize == 1024 || hasMetadata ? 0x1000Au : 0x1030Au, 0x10A, 0x20A};
    std::memcpy(buffer + offset, words.data(), sizeof(words));
    batch.offset = offset + sizeof(words);
    batch.buffer_size = descriptorOffset;
    return 0;
}

int EncodeTransforms(AcmBatchInfo* batch, bool inverse, std::int32_t transformSize, std::uint32_t transformCount, std::int32_t inputFormat, const void* const* inputs, std::int32_t outputFormat, void* const* outputs, std::int32_t flags) {
    const std::int32_t conflictingFlags = inverse ? 5 : 3;
    if (!batch || transformCount == 0 || transformCount > 0x7FFFFFFF || !inputs || !outputs || (flags & conflictingFlags) == conflictingFlags || inputFormat < 0 || inputFormat > 1 || outputFormat < 0 || outputFormat > 1) return SCE_ACM_ERROR_BAD_POINTER;
    if (transformSize != 256 && transformSize != 512 && transformSize != 1024 && transformSize != 2048) return SCE_ACM_ERROR_BAD_POINTER;
    const bool real = (flags & 1) != 0;
    const bool wide = transformSize == 2048;
    if (wide && !inverse && !real) return SCE_ACM_ERROR_BAD_POINTER;
    const std::uint32_t sizeIndex = transformSize == 256 ? 0 : transformSize == 512 ? 1 : transformSize == 1024 ? 2 : 3;
    const std::uint32_t inputBytes = transformSize * (inputFormat == 1 ? 2 : 4);
    const std::uint32_t outputBytes = transformSize * (outputFormat == 1 ? 2 : 4);
    const std::uint64_t inputBlocks = (inputBytes >> 8) >> (!inverse && real ? 1 : 0);
    const std::uint64_t outputBlocks = outputBytes >> 8;
    std::uint32_t inputCommand;
    std::uint32_t transformCommand;
    std::uint32_t outputCommand;
    if (wide) {
        inputCommand = 2 * inputFormat + (inverse ? 1 : 0);
        transformCommand = inverse ? 7 : 6;
        outputCommand = 4 | outputFormat;
    } else if (inverse) {
        inputCommand = 4 | inputFormat;
        transformCommand = 0xC;
        outputCommand = 6 | outputFormat;
    } else {
        inputCommand = (real ? 0 : 2) | inputFormat;
        transformCommand = real ? 0xA : 0xB;
        outputCommand = 8 | outputFormat;
    }
    std::uint64_t header = 0;
    if (!(flags & 8)) {
        const auto encodedCount = transformCount < 64000 ? transformCount : 64000;
        header = (inverse ? 0 : std::uint64_t{1} << 16) | (static_cast<std::uint64_t>(flags & 6) << 20) | (real ? std::uint64_t{1} << (inverse ? 23 : 24) : 0) | (std::uint64_t{1} << (inputFormat == 1 ? 25 : 27)) | (std::uint64_t{1} << (outputFormat == 1 ? 26 : 28)) | (std::uint64_t{1} << (17 + sizeIndex)) | (static_cast<std::uint64_t>(encodedCount) << 29);
    }
    header |= static_cast<std::uint64_t>(7 + sizeIndex) << 8;
    auto* buffer = static_cast<std::byte*>(batch->buffer);
    auto offset = batch->offset;
    const auto capacity = batch->buffer_size;
    const auto hasRoom = [&](std::uint64_t bytes) { return offset <= capacity && capacity - offset >= bytes; };
    const auto appendWord = [&](std::uint64_t value) {
        std::memcpy(buffer + offset, &value, sizeof(value));
        offset += sizeof(value);
    };
    constexpr int noSpace = static_cast<int>(0x81940009);
    constexpr int badAlignment = static_cast<int>(0x8194000A);
    if (!hasRoom(16)) return noSpace;
    appendWord(header);
    appendWord(10 | ((wide ? 8u : 0xDu) << 8));
    const bool pairedInputs = (flags & 2) != 0;
    const bool pairedOutputs = (flags & 4) != 0;
    const bool realOutput = inverse && real;
    for (std::uint32_t index = 0; index < transformCount; ++index) {
        const auto inputIndex = static_cast<std::size_t>(index) * (pairedInputs ? 2 : 1);
        const auto input = reinterpret_cast<std::uintptr_t>(inputs[inputIndex]);
        const auto secondInput = pairedInputs ? reinterpret_cast<std::uintptr_t>(inputs[inputIndex + 1]) : 0;
        if (!pairedInputs && !input) return SCE_ACM_ERROR_BAD_POINTER;
        if ((input | secondInput) & 127) return badAlignment;
        if (!hasRoom((pairedInputs ? 24 : 16) + 24)) return noSpace;
        appendWord((pairedInputs ? 7 : 6) | (inputBlocks << 32) | (pairedInputs ? static_cast<std::uint64_t>(inputBytes / 2) << 48 : 0));
        appendWord(input);
        if (pairedInputs) appendWord(secondInput);
        appendWord(10 | (inputCommand << 8));
        appendWord(10 | (transformCommand << 8));
        appendWord(10 | (outputCommand << 8));
        const auto outputIndex = static_cast<std::size_t>(index) * (pairedOutputs ? 2 : 1);
        const auto output = reinterpret_cast<std::uintptr_t>(outputs[outputIndex]);
        const auto secondOutput = pairedOutputs ? reinterpret_cast<std::uintptr_t>(outputs[outputIndex + 1]) : 0;
        if ((output | secondOutput) & 127) return badAlignment;
        const bool longRecord = pairedOutputs || realOutput;
        if (!hasRoom(longRecord ? 24 : 16)) return noSpace;
        appendWord((longRecord ? 9 : 8) | (outputBlocks << 32) | (longRecord ? static_cast<std::uint64_t>(outputBytes / 2) << 48 : 0));
        if (realOutput) appendWord(0);
        appendWord(output);
        if (pairedOutputs) appendWord(secondOutput);
    }
    batch->offset = offset;
    return 0;
}

}

extern "C" {

int APS5_VABI sceAcmChain_Audio3dPO1_Clear() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcmChain_Audio3dPO1_RenderObject() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcmChain_Audio3dPO1_Activate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcmChain_Audio3dPO1_Output() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcm_ConvReverb_SharedIr_2() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcmChain_Audio3dPO1_RenderAmbisonicsChannel() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcmChain_Audio3dPO2_Activate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcmChain_Audio3dPO1_RenderPassthroughObject() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcmChain_Audio3dPO2_RenderDelay() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcmGetTelemetry() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAcm_ConvReverb_SharedInput_2() {
    NotImplemented_nid_no_patch(__func__);
}

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
    if (!batch || mode != 1 || !input || !filters) return SCE_ACM_ERROR_BAD_POINTER;
    const auto blockSize = input->blockSize;
    if (blockSize != 128 && blockSize != 256 && blockSize != 512 && blockSize != 1024) return SCE_ACM_ERROR_BAD_POINTER;
    for (std::uint32_t index = 0; index < outputCount; ++index) {
        if (!filters[index] || filters[index]->blockCount == 0) return SCE_ACM_ERROR_BAD_POINTER;
    }
    const auto blockCount = input->blockCount;
    if (!input->history || blockCount == 0) return SCE_ACM_ERROR_BAD_POINTER;
    input->currentBlock = (input->availableBlocks == 0 || input->currentBlock == 0 ? blockCount : input->currentBlock) - 1;
    input->availableBlocks = std::min(input->availableBlocks + 1, blockCount);
    const auto channelCount = input->channelCount;
    if (channelCount == 0 || channelCount > 256 || !input->inputChannels || input->sampleFormat > 1) return SCE_ACM_ERROR_BAD_POINTER;
    const bool wideSamples = input->sampleFormat == 0;
    const std::uint32_t slotBytes = blockSize * (wideSamples ? 8 : 4);
    const std::uint32_t current = input->currentBlock;
    std::vector<const void*> sources(channelCount);
    std::vector<void*> destinations(channelCount);
    for (std::uint32_t channel = 0; channel < channelCount; ++channel) {
        const auto* history = input->history[channel];
        if (!history || history->blockMetadata || !history->samples || !input->inputChannels[channel]) return SCE_ACM_ERROR_BAD_POINTER;
        const auto position = wideSamples ? channel : channelCount - 1 - channel;
        sources[position] = input->inputChannels[channel];
        destinations[position] = static_cast<std::byte*>(history->samples) + current * slotBytes;
    }
    AcmBatchInfo work = *batch;
    if (const auto result = sceAcm_FFT(&work, static_cast<std::int32_t>(2 * blockSize), channelCount, 0, sources.data(), wideSamples ? 0 : 1, destinations.data(), 9)) return result;
    if (outputCount == 0 || !outputs) return SCE_ACM_ERROR_BAD_POINTER;
    const auto headerOffset = work.offset;
    if (headerOffset > work.buffer_size || work.buffer_size - headerOffset < 8) return SCE_ACM_ERROR_BATCH_FULL;
    work.offset += 8;
    const auto available = input->availableBlocks;
    const auto start = current >= blockCount ? current - blockCount : current;
    std::uint64_t flags = 0;
    std::uint32_t smallestBlockCount = blockCount;
    for (std::uint32_t channel = 0; channel < channelCount; ++channel) {
        const ReverbEntry inputEntry{input->history[channel]->samples, available, start * slotBytes, blockCount * slotBytes, nullptr, nullptr, nullptr, nullptr, 0.0f, wideSamples};
        flags |= std::uint64_t{1} << (wideSamples ? 21 : 20);
        std::array<ReverbEntry, 8> entries{};
        std::uint32_t entryCount = 0;
        for (std::uint32_t outputIndex = 0; outputIndex < outputCount; ++outputIndex) {
            const auto* filter = filters[outputIndex];
            const auto* output = outputs[outputIndex];
            const auto filterChannels = filter->channelCount;
            if (!filter->channels || !output || !output->workspace || !output->samples || filter->blockSize != blockSize || output->blockSize != blockSize || filterChannels == 0 || output->channelCount != std::max(channelCount, filterChannels)) return SCE_ACM_ERROR_BAD_POINTER;
            if (filterChannels != channelCount && channelCount != 1 && filterChannels != 1) return SCE_ACM_ERROR_BAD_POINTER;
            if (Address(filter->blockMetadata) & 3) return SCE_ACM_ERROR_ALIGNMENT;
            if (filter->sampleFormat > 1) return SCE_ACM_ERROR_BAD_POINTER;
            const bool wideFilter = filter->sampleFormat == 0;
            flags |= (filterChannels != 1 && channelCount == 1 ? std::uint64_t{1} << 17 : 0) | (filterChannels == 1 && channelCount != 1 ? std::uint64_t{1} << 18 : 0) | (filterChannels == channelCount ? std::uint64_t{1} << 19 : 0) | (std::uint64_t{1} << (wideFilter ? 23 : 22)) | (filter->blockMetadata ? std::uint64_t{1} << 27 : 0) | (std::uint64_t{1} << (filterChannels == 1 ? 39 : (0x47 - std::countl_zero(filterChannels - 1)) & 63));
            smallestBlockCount = std::min(smallestBlockCount, filter->blockCount);
            const auto first = filterChannels == channelCount ? channel : 0;
            const auto last = filterChannels == channelCount ? channel + 1 : channelCount == 1 ? filterChannels : 1;
            const auto length = std::min(available, filter->blockCount);
            for (auto filterChannel = first; filterChannel < last; ++filterChannel) {
                const auto outputChannel = channel - first + filterChannel;
                const auto* block = filter->channels[filterChannel];
                if (!block || !block->samples || !output->workspace[outputChannel] || !output->samples[outputChannel]) return SCE_ACM_ERROR_BAD_POINTER;
                if ((Address(block->blockMetadata) & 3) || ((Address(block->samples) | Address(output->workspace[outputChannel]) | Address(output->samples[outputChannel])) & 127)) return SCE_ACM_ERROR_ALIGNMENT;
                if (block->blockMetadata) flags |= std::uint64_t{1} << 26;
                entries[entryCount++] = {block->samples, length, 0, 0xFFFFFFFF, block->blockMetadata, filter->blockMetadata, output->workspace[outputChannel], output->samples[outputChannel], gains ? gains[outputIndex] : 1.0f, wideFilter};
                if (entryCount == entries.size()) {
                    if (const auto result = AppendReverbJob(work, blockSize, inputEntry, entries.data(), entryCount)) return result;
                    entryCount = 0;
                }
            }
        }
        if (entryCount) {
            if (const auto result = AppendReverbJob(work, blockSize, inputEntry, entries.data(), entryCount)) return result;
        }
    }
    flags |= (std::uint64_t{std::min(outputCount - 1, 15u)} << 29) | (std::uint64_t{1} << ((64 - std::countl_zero(channelCount)) & 63)) | (std::uint64_t{std::min(smallestBlockCount, 7u)} << 45);
    const std::uint64_t header = ((flags & 0xFFFF) ? std::uint64_t{1} << 63 : flags) | ((0x0B + static_cast<std::uint64_t>(std::countr_zero(blockSize >> 7))) << 8);
    std::memcpy(static_cast<std::byte*>(work.buffer) + headerOffset, &header, sizeof(header));
    *batch = work;
    return 0;
}

int APS5_VABI sceAcm_FFT(AcmBatchInfo* batch, std::int32_t transformSize, std::uint32_t transformCount, std::int32_t inputFormat, const void* const* inputs, std::int32_t outputFormat, void* const* outputs, std::int32_t flags) {
    return EncodeTransforms(batch, false, transformSize, transformCount, inputFormat, inputs, outputFormat, outputs, flags);
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
    if (!batch || priority >= 4) return SCE_ACM_ERROR_BAD_POINTER;
    const auto offset = batch->offset;
    if (offset > batch->buffer_size || batch->buffer_size - offset < 8) return static_cast<int>(0x81940009);
    const std::uint64_t job = 0x10 | (static_cast<std::uint64_t>(priority) << 8);
    std::memcpy(static_cast<std::byte*>(batch->buffer) + offset, &job, sizeof(job));
    batch->offset = offset + sizeof(job);
    return 0;
}

int APS5_VABI sceAcm_ConvReverb_SharedIr() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAcm_IFFT(AcmBatchInfo* batch, std::int32_t transformSize, std::uint32_t transformCount, std::int32_t inputFormat, const void* const* inputs, std::int32_t outputFormat, void* const* outputs, std::int32_t flags) {
    return EncodeTransforms(batch, true, transformSize, transformCount, inputFormat, inputs, outputFormat, outputs, flags);
}

int APS5_VABI sceAcmBatchJobNotification() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
