#include "prx/libSceAcm/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "SceTypes.hpp"

static void Require(bool value) {
    if (!value) std::abort();
}

constexpr int BadContext = static_cast<int>(0x81940002);
constexpr int BadPointer = static_cast<int>(0x81940006);

int main() {
    Require(sceAcmContextCreate(nullptr) == BadPointer);
    Require(sceAcmContextDestroy(0) == BadContext);
    Require(sceAcmContextDestroy(999999) == BadContext);

    std::uint32_t context1 = 0;
    Require(sceAcmContextCreate(&context1) == 0 && context1 != 0);

    std::uint32_t context2 = 0;
    Require(sceAcmContextCreate(&context2) == 0 && context2 != 0 && context2 != context1);

    std::uint32_t batch1 = 0;
    Require(sceAcmBatchStartBuffer(0, nullptr, 0, nullptr, &batch1) == BadContext);
    Require(sceAcmBatchStartBuffer(999999, nullptr, 0, nullptr, &batch1) == BadContext);
    Require(sceAcmBatchStartBuffer(context1, nullptr, 0, nullptr, nullptr) == BadPointer);
    Require(sceAcmBatchStartBuffer(context1, nullptr, 0, nullptr, &batch1) == 0 && batch1 != 0);

    Require(sceAcmBatchWait(0, batch1, 0) == BadContext);
    Require(sceAcmBatchWait(999999, batch1, 0) == BadContext);
    Require(sceAcmBatchWait(context1, batch1, 0) == 0);
    Require(sceAcmBatchWait(context1, batch1, 0) == 0);

    std::uint32_t batch2 = 0;
    Require(sceAcmBatchStartBuffers(0, 0, nullptr, nullptr, &batch2) == BadContext);
    Require(sceAcmBatchStartBuffers(999999, 0, nullptr, nullptr, &batch2) == BadContext);
    Require(sceAcmBatchStartBuffers(context2, 0, nullptr, nullptr, nullptr) == BadPointer);
    Require(sceAcmBatchStartBuffers(context2, 0, nullptr, nullptr, &batch2) == 0 && batch2 != 0);
    Require(sceAcmBatchWait(context2, batch2, 0) == 0);

    Require(sceAcm_ConvReverb_SharedInput(nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr) == BadPointer);
    Require(sceAcm_FFT(nullptr, 0, 0, 0, nullptr, 0, nullptr, 0) == BadPointer);
    alignas(128) std::byte commands[256]{};
    alignas(128) std::uint32_t sourceData[32]{};
    alignas(128) const void* sources[16]{sourceData};
    alignas(128) void* outputs[16]{sourceData};
    AcmBatchInfo commandBatch{commands, 0, sizeof(commands)};
    Require(sceAcm_Panner(&commandBatch, 0, sources, 0, 0, 1, sources, sources, nullptr, outputs) == BadPointer);
    sourceData[0] = 2;
    Require(sceAcm_Panner(&commandBatch, 1, sources, 0, 0, 1, sources, sources, nullptr, outputs) == BadPointer);
    sourceData[0] = 0;
    outputs[0] = reinterpret_cast<std::byte*>(sourceData) + 4;
    Require(sceAcm_Panner(&commandBatch, 1, sources, 0, 0, 1, sources, sources, nullptr, outputs) == static_cast<int>(0x8194000A));
    outputs[0] = sourceData;
    Require(sceAcm_Panner(&commandBatch, 1, sources, 2, 1, 1, sources, sources, nullptr, outputs) == 0);
    Require(commandBatch.offset == 32 && commandBatch.buffer_size == 128);
    std::uint64_t commandWords[4];
    std::memcpy(commandWords, commands, sizeof(commandWords));
    Require(commandWords[0] == ((std::uint64_t{1} << 34) | (std::uint64_t{1} << 36) | 0x01010300));
    Require(commandWords[1] == 0x8000000000000D && commandWords[2] == reinterpret_cast<std::uintptr_t>(commands + 128) && commandWords[3] == 10);
    std::uint64_t descriptor[16];
    std::memcpy(descriptor, commands + 128, sizeof(descriptor));
    Require(descriptor[0] == 1 && descriptor[1] == 0 && descriptor[2] == reinterpret_cast<std::uintptr_t>(outputs));
    Require(descriptor[3] == reinterpret_cast<std::uintptr_t>(sources) && descriptor[4] == 0x100000002 && descriptor[5] == 1);
    Require(descriptor[6] == reinterpret_cast<std::uintptr_t>(sources) && descriptor[7] == reinterpret_cast<std::uintptr_t>(sources));
    for (std::size_t index = 8; index < 16; ++index) Require(descriptor[index] == 0);
    commandBatch = {commands, 0, sizeof(commands)};
    Require(sceAcm_FFT(&commandBatch, 512, 0, 0, sources, 0, outputs, 0) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 768, 1, 0, sources, 0, outputs, 0) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 4096, 1, 0, sources, 0, outputs, 0) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 512, 1, 2, sources, 0, outputs, 0) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 512, 1, 0, sources, 2, outputs, 0) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 512, 1, 0, nullptr, 0, outputs, 0) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 512, 1, 0, sources, 0, nullptr, 0) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 2048, 1, 0, sources, 0, outputs, 0) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 512, 1, 0, sources, 0, outputs, 3) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 512, 1, 0, sources, 0, outputs, 0) == 0);
    Require(commandBatch.offset == 72 && commandBatch.buffer_size == sizeof(commands));
    std::uint64_t fftWords[9];
    std::memcpy(fftWords, commands, sizeof(fftWords));
    Require(fftWords[0] == 0x38050800 && fftWords[1] == 0xD0A);
    Require(fftWords[2] == 0x800000006 && fftWords[3] == reinterpret_cast<std::uintptr_t>(sourceData));
    Require(fftWords[4] == 0x20A && fftWords[5] == 0xB0A && fftWords[6] == 0x80A);
    Require(fftWords[7] == 0x800000008 && fftWords[8] == reinterpret_cast<std::uintptr_t>(sourceData));
    commandBatch = {commands, 0, 71};
    Require(sceAcm_FFT(&commandBatch, 512, 1, 0, sources, 0, outputs, 0) == static_cast<int>(0x81940009));
    Require(commandBatch.offset == 0);
    commandBatch = {commands, 0, sizeof(commands)};
    Require(sceAcm_FFT(&commandBatch, 2048, 1, 1, sources, 1, outputs, 9) == 0);
    std::memcpy(fftWords, commands, sizeof(fftWords));
    Require(fftWords[0] == 0xA00 && fftWords[1] == 0x80A && fftWords[2] == 0x800000006 && fftWords[4] == 0x20A && fftWords[5] == 0x60A && fftWords[6] == 0x50A);
    commandBatch = {commands, 0, sizeof(commands)};
    Require(sceAcm_FFT(&commandBatch, 256, 1, 0, sources, 0, outputs, 1) == 0 && commandBatch.offset == 72);
    std::memcpy(fftWords, commands, sizeof(fftWords));
    Require(fftWords[0] == 0x39030700 && fftWords[1] == 0xD0A && fftWords[2] == 0x200000006);
    Require(fftWords[4] == 0xA && fftWords[5] == 0xA0A && fftWords[6] == 0x80A && fftWords[7] == 0x400000008);
    std::uint64_t ifftWords[10];
    commandBatch = {commands, 0, sizeof(commands)};
    Require(sceAcm_IFFT(nullptr, 1024, 1, 1, sources, 0, outputs, 1) == BadPointer);
    Require(sceAcm_IFFT(&commandBatch, 1024, 1, 1, sources, 0, outputs, 5) == BadPointer);
    Require(sceAcm_IFFT(&commandBatch, 1024, 1, 1, sources, 0, outputs, 3) == 0 && commandBatch.offset == 88);
    commandBatch = {commands, 0, sizeof(commands)};
    Require(sceAcm_IFFT(&commandBatch, 1024, 1, 1, sources, 0, outputs, 1) == 0 && commandBatch.offset == 80);
    std::memcpy(ifftWords, commands, sizeof(ifftWords));
    Require(ifftWords[0] == 0x32880900 && ifftWords[1] == 0xD0A && ifftWords[2] == 0x800000006 && ifftWords[3] == reinterpret_cast<std::uintptr_t>(sourceData));
    Require(ifftWords[4] == 0x50A && ifftWords[5] == 0xC0A && ifftWords[6] == 0x60A);
    Require(ifftWords[7] == 0x0800001000000009 && ifftWords[8] == 0 && ifftWords[9] == reinterpret_cast<std::uintptr_t>(sourceData));
    commandBatch = {commands, 0, sizeof(commands)};
    Require(sceAcm_IFFT(&commandBatch, 2048, 1, 0, sources, 1, outputs, 0) == 0 && commandBatch.offset == 72);
    std::memcpy(ifftWords, commands, sizeof(ifftWords));
    Require(ifftWords[0] == 0x2C100A00 && ifftWords[1] == 0x80A && ifftWords[4] == 0x10A && ifftWords[5] == 0x70A && ifftWords[6] == 0x50A);
    sources[1] = sourceData;
    outputs[1] = sourceData;
    for (const auto size : {256, 512, 1024, 2048}) {
        for (std::int32_t inputFormat = 0; inputFormat < 2; ++inputFormat) {
            for (std::int32_t outputFormat = 0; outputFormat < 2; ++outputFormat) {
                for (std::int32_t flags = 0; flags < 16; ++flags) {
                    commandBatch = {commands, 0, sizeof(commands)};
                    const auto result = sceAcm_FFT(&commandBatch, size, 1, inputFormat, sources, outputFormat, outputs, flags);
                    if ((flags & 3) == 3 || (size == 2048 && !(flags & 1))) {
                        Require(result == BadPointer && commandBatch.offset == 0);
                    } else {
                        Require(result == 0 && commandBatch.offset == 72 + ((flags & 2) ? 8 : 0) + ((flags & 4) ? 8 : 0));
                    }
                    commandBatch = {commands, 0, sizeof(commands)};
                    const auto inverseResult = sceAcm_IFFT(&commandBatch, size, 1, inputFormat, sources, outputFormat, outputs, flags);
                    if ((flags & 5) == 5) {
                        Require(inverseResult == BadPointer && commandBatch.offset == 0);
                    } else {
                        Require(inverseResult == 0 && commandBatch.offset == 72 + ((flags & 2) ? 8 : 0) + ((flags & 5) ? 8 : 0));
                    }
                }
            }
        }
    }
    commandBatch = {commands, 0, 152};
    Require(sceAcm_Panner(&commandBatch, 1, sources, 0, 0, 1, sources, sources, nullptr, outputs) == static_cast<int>(0x81940009));
    Require(commandBatch.offset == 0 && commandBatch.buffer_size == 152);

    const auto qword = [](const std::byte* base, std::size_t position) { std::uint64_t value; std::memcpy(&value, base + position, sizeof(value)); return value; };
    const auto dword = [](const std::byte* base, std::size_t position) { std::uint32_t value; std::memcpy(&value, base + position, sizeof(value)); return value; };
    const auto single = [](const std::byte* base, std::size_t position) { float value; std::memcpy(&value, base + position, sizeof(value)); return value; };
    const auto address = [](const void* pointer) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)); };
    alignas(128) static std::byte reverbMemory[0x10000];
    alignas(128) static std::byte reverbCommands[4096];
    std::byte* const memory = reverbMemory;
    const std::byte* const descriptor0 = reverbCommands + sizeof(reverbCommands) - 0x280;
    const std::byte* const descriptor1 = reverbCommands + sizeof(reverbCommands) - 0x500;
    AcmReverbBuffer historyBlocks[2]{{memory, nullptr}, {memory + 0x4000, nullptr}};
    const AcmReverbBuffer* historyChannels[2]{&historyBlocks[0], &historyBlocks[1]};
    const void* inputChannels[2]{memory + 0x8000, memory + 0x8080};
    AcmReverbInput reverbInput{512, 4, 2, 0, 0, 0, historyChannels, inputChannels, 0};
    AcmReverbBuffer filterBlocks[4]{{memory + 0x9000, nullptr}, {memory + 0x9080, nullptr}, {memory + 0x9100, nullptr}, {memory + 0x9180, nullptr}};
    const AcmReverbBuffer* filterChannelsA[2]{&filterBlocks[0], &filterBlocks[1]};
    const AcmReverbBuffer* filterChannelsB[2]{&filterBlocks[2], &filterBlocks[3]};
    AcmReverbFilter filterA{512, 3, 2, 0, nullptr, filterChannelsA};
    AcmReverbFilter filterB{512, 5, 2, 0, nullptr, filterChannelsB};
    void* workspaceA[2]{memory + 0xA000, memory + 0xA080};
    void* samplesA[2]{memory + 0xB000, memory + 0xB080};
    void* workspaceB[2]{memory + 0xA100, memory + 0xA180};
    void* samplesB[2]{memory + 0xB100, memory + 0xB180};
    AcmReverbOutput outputA{512, 2, samplesA, workspaceA, 1.0f, 1.0f, nullptr};
    AcmReverbOutput outputB{512, 2, samplesB, workspaceB, 1.0f, 1.0f, nullptr};
    const AcmReverbFilter* reverbFilters[2]{&filterA, &filterB};
    const AcmReverbOutput* reverbOutputs[2]{&outputA, &outputB};
    const float reverbGains[2]{0.5f, 0.25f};
    AcmBatchInfo reverbBatch{reverbCommands, 0, sizeof(reverbCommands)};
    const auto ringIs = [&reverbInput](std::uint32_t current, std::uint32_t available) { return reverbInput.currentBlock == current && reverbInput.availableBlocks == available; };
    const auto reverb = [&](std::uint32_t outputCount, const AcmReverbFilter* const* filters, const AcmReverbOutput* const* outputs) { return sceAcm_ConvReverb_SharedInput(&reverbBatch, 1, &reverbInput, outputCount, filters, reverbGains, outputs); };

    Require(sceAcm_ConvReverb_SharedInput(nullptr, 1, &reverbInput, 1, reverbFilters, reverbGains, reverbOutputs) == BadPointer);
    Require(sceAcm_ConvReverb_SharedInput(&reverbBatch, 0, &reverbInput, 1, reverbFilters, reverbGains, reverbOutputs) == BadPointer);
    Require(sceAcm_ConvReverb_SharedInput(&reverbBatch, 1, nullptr, 1, reverbFilters, reverbGains, reverbOutputs) == BadPointer);
    Require(reverb(1, nullptr, reverbOutputs) == BadPointer);
    reverbInput.blockSize = 768;
    Require(reverb(1, reverbFilters, reverbOutputs) == BadPointer);
    reverbInput.blockSize = 512;
    filterA.blockCount = 0;
    Require(reverb(1, reverbFilters, reverbOutputs) == BadPointer);
    filterA.blockCount = 3;
    Require(ringIs(0, 0) && reverbBatch.offset == 0);
    Require(reverb(0, reverbFilters, reverbOutputs) == BadPointer && ringIs(3, 1));
    Require(reverb(1, reverbFilters, nullptr) == BadPointer && ringIs(2, 2));
    reverbInput.currentBlock = 0;
    historyBlocks[1].blockMetadata = memory;
    Require(reverb(1, reverbFilters, reverbOutputs) == BadPointer && ringIs(3, 3));
    historyBlocks[1].blockMetadata = nullptr;
    Require(reverbBatch.offset == 0 && reverbBatch.buffer_size == sizeof(reverbCommands));

    reverbInput.currentBlock = 0;
    reverbInput.availableBlocks = 0;
    std::memset(reverbCommands, 0xCC, sizeof(reverbCommands));
    Require(reverb(1, reverbFilters, reverbOutputs) == 0);
    Require(ringIs(3, 1) && reverbBatch.offset == 216 && reverbBatch.buffer_size == sizeof(reverbCommands) - 2 * 0x280);
    Require(qword(reverbCommands, 0) == 0x900 && qword(reverbCommands, 24) == address(inputChannels[0]) && qword(reverbCommands, 64) == address(memory + 0x3000) && qword(reverbCommands, 80) == address(inputChannels[1]) && qword(reverbCommands, 120) == address(memory + 0x7000));
    Require(qword(reverbCommands, 128) == ((std::uint64_t{3} << 45) | (std::uint64_t{1} << 40) | (std::uint64_t{1} << 34) | (std::uint64_t{1} << 23) | (std::uint64_t{1} << 21) | (std::uint64_t{1} << 19) | 0xD00));
    Require(qword(reverbCommands, 136) == 0x028000000000000D && qword(reverbCommands, 144) == address(descriptor0) && qword(reverbCommands, 152) == 0x1030A && qword(reverbCommands, 160) == 0x10A && qword(reverbCommands, 168) == 0x20A);
    Require(qword(reverbCommands, 176) == 0x028000000000000D && qword(reverbCommands, 184) == address(descriptor1) && qword(reverbCommands, 208) == 0x20A);
    Require(qword(descriptor0, 0) == address(memory) && qword(descriptor0, 8) == address(memory + 0x9000) && qword(descriptor0, 16) == 0);
    Require(dword(descriptor0, 0x48) == 1 && dword(descriptor0, 0x4C) == 1 && dword(descriptor0, 0x6C) == 0 && dword(descriptor0, 0xC8) == 3 * 4096 && dword(descriptor0, 0xCC) == 0);
    Require(dword(descriptor0, 0x148) == 4 * 4096 && dword(descriptor0, 0x14C) == 0xFFFFFFFF && qword(descriptor0, 0x80) == 0 && qword(descriptor0, 0x100) == 0 && descriptor0[0x16C] == std::byte{0});
    Require(dword(descriptor0, 0x174) == 1024 && dword(descriptor0, 0x178) == 3 && dword(descriptor0, 0x17C) == 2);
    Require(qword(descriptor0, 0x180) == address(workspaceA[0]) && qword(descriptor0, 0x188) == 0 && single(descriptor0, 0x1C0) == 0.5f && qword(descriptor0, 0x200) == address(samplesA[0]) && qword(descriptor0, 0x240) == 0);
    Require(qword(descriptor1, 0) == address(memory + 0x4000) && qword(descriptor1, 8) == address(memory + 0x9080) && qword(descriptor1, 0x180) == address(workspaceA[1]) && qword(descriptor1, 0x200) == address(samplesA[1]));
    Require(reverb(1, reverbFilters, reverbOutputs) == 0);
    Require(ringIs(2, 2) && reverbBatch.offset == 432 && reverbBatch.buffer_size == sizeof(reverbCommands) - 4 * 0x280);
    const std::byte* const descriptor2 = reverbCommands + sizeof(reverbCommands) - 0x780;
    Require(qword(reverbCommands, 280) == address(memory + 0x2000) && qword(reverbCommands, 360) == address(descriptor2) && dword(descriptor2, 0x48) == 2 && dword(descriptor2, 0xC8) == 2 * 4096);

    reverbInput.currentBlock = 0;
    reverbInput.availableBlocks = 4;
    reverbBatch = {reverbCommands, 0, sizeof(reverbCommands)};
    Require(reverb(2, reverbFilters, reverbOutputs) == 0);
    Require(ringIs(3, 4) && reverbBatch.offset == 216);
    Require(qword(reverbCommands, 128) == ((std::uint64_t{3} << 45) | (std::uint64_t{1} << 40) | (std::uint64_t{1} << 34) | (std::uint64_t{1} << 29) | (std::uint64_t{1} << 23) | (std::uint64_t{1} << 21) | (std::uint64_t{1} << 19) | 0xD00));
    Require(qword(descriptor0, 8) == address(memory + 0x9100) && qword(descriptor0, 16) == address(memory + 0x9000));
    Require(dword(descriptor0, 0x48) == 4 && dword(descriptor0, 0x4C) == 4 && dword(descriptor0, 0x50) == 3 && descriptor0[0x16C] == std::byte{1} && descriptor0[0x16D] == std::byte{0});
    Require(dword(descriptor0, 0x17C) == 3 && dword(descriptor0, 0x178) == 7 && single(descriptor0, 0x1C0) == 0.25f && single(descriptor0, 0x1C4) == 0.5f);
    Require(qword(descriptor0, 0x180) == address(workspaceB[0]) && qword(descriptor0, 0x188) == address(workspaceA[0]) && qword(descriptor0, 0x200) == address(samplesB[0]) && qword(descriptor0, 0x208) == address(samplesA[0]));

    const AcmReverbBuffer* broadcastChannels[9];
    void* broadcastWorkspace[9];
    void* broadcastSamples[9];
    for (std::size_t index = 0; index < 9; ++index) {
        broadcastChannels[index] = &filterBlocks[index % 4];
        broadcastWorkspace[index] = memory + 0xC000 + 0x80 * index;
        broadcastSamples[index] = memory + 0xD000 + 0x80 * index;
    }
    AcmReverbInput monoInput{512, 2, 1, 0, 0, 0, historyChannels, inputChannels, 0};
    AcmReverbFilter broadcastFilter{512, 2, 9, 1, nullptr, broadcastChannels};
    AcmReverbOutput broadcastOutput{512, 9, broadcastSamples, broadcastWorkspace, 1.0f, 1.0f, nullptr};
    const AcmReverbFilter* broadcastFilters[1]{&broadcastFilter};
    const AcmReverbOutput* broadcastOutputs[1]{&broadcastOutput};
    reverbBatch = {reverbCommands, 0, sizeof(reverbCommands)};
    Require(sceAcm_ConvReverb_SharedInput(&reverbBatch, 1, &monoInput, 1, broadcastFilters, nullptr, broadcastOutputs) == 0);
    Require(reverbBatch.offset == 160 && reverbBatch.buffer_size == sizeof(reverbCommands) - 2 * 0x280 && monoInput.currentBlock == 1 && monoInput.availableBlocks == 1);
    Require(qword(reverbCommands, 72) == ((std::uint64_t{2} << 45) | (std::uint64_t{1} << 43) | (std::uint64_t{1} << 33) | (std::uint64_t{1} << 22) | (std::uint64_t{1} << 21) | (std::uint64_t{1} << 17) | 0xD00));
    Require(dword(descriptor0, 0x17C) == 9 && dword(descriptor0, 0x178) == 1 && qword(descriptor0, 0x238) == address(broadcastSamples[7]) && single(descriptor0, 0x1DC) == 1.0f && descriptor0[0x173] == std::byte{7});
    Require(dword(descriptor1, 0x17C) == 2 && qword(descriptor1, 8) == address(memory + 0x9000) && qword(descriptor1, 0x180) == address(broadcastWorkspace[8]) && qword(descriptor1, 0x200) == address(broadcastSamples[8]));

    const AcmReverbBuffer* sharedFilterChannel[1]{&filterBlocks[0]};
    AcmReverbFilter sharedFilter{512, 4, 1, 0, nullptr, sharedFilterChannel};
    const AcmReverbFilter* sharedFilters[1]{&sharedFilter};
    reverbInput.sampleFormat = 1;
    reverbInput.currentBlock = 0;
    reverbInput.availableBlocks = 0;
    reverbBatch = {reverbCommands, 0, sizeof(reverbCommands)};
    Require(reverb(1, sharedFilters, reverbOutputs) == 0 && reverbBatch.offset == 216);
    Require(qword(reverbCommands, 24) == address(inputChannels[1]) && qword(reverbCommands, 64) == address(memory + 0x5800) && qword(reverbCommands, 80) == address(inputChannels[0]) && qword(reverbCommands, 120) == address(memory + 0x1800));
    Require(qword(reverbCommands, 128) == ((std::uint64_t{4} << 45) | (std::uint64_t{1} << 39) | (std::uint64_t{1} << 34) | (std::uint64_t{1} << 23) | (std::uint64_t{1} << 20) | (std::uint64_t{1} << 18) | 0xD00));
    Require(dword(descriptor0, 0xC8) == 3 * 2048 && dword(descriptor0, 0x148) == 4 * 2048 && dword(descriptor0, 0x178) == 2 && qword(descriptor1, 8) == address(memory + 0x9000) && qword(descriptor1, 0x200) == address(samplesA[1]));
    reverbInput.sampleFormat = 0;

    reverbInput.blockSize = 128;
    filterA.blockSize = 128;
    outputA.blockSize = 128;
    reverbInput.currentBlock = 0;
    reverbInput.availableBlocks = 0;
    reverbBatch = {reverbCommands, 0, sizeof(reverbCommands)};
    Require(reverb(1, reverbFilters, reverbOutputs) == 0 && ringIs(3, 1) && reverbBatch.offset == 216 && reverbBatch.buffer_size == sizeof(reverbCommands) - 2 * 0x280);
    Require(qword(reverbCommands, 0) == 0x700 && qword(reverbCommands, 8) == 0xD0A && qword(reverbCommands, 16) == 0x200000006 && qword(reverbCommands, 24) == address(inputChannels[0]));
    Require(qword(reverbCommands, 32) == 0xA && qword(reverbCommands, 40) == 0xA0A && qword(reverbCommands, 48) == 0x80A && qword(reverbCommands, 56) == 0x400000008 && qword(reverbCommands, 64) == address(memory + 0xC00));
    Require(qword(reverbCommands, 120) == address(memory + 0x4C00));
    Require(qword(reverbCommands, 128) == ((std::uint64_t{3} << 45) | (std::uint64_t{1} << 40) | (std::uint64_t{1} << 34) | (std::uint64_t{1} << 23) | (std::uint64_t{1} << 21) | (std::uint64_t{1} << 19) | 0xB00));
    Require(qword(reverbCommands, 136) == 0x028000000000000D && qword(reverbCommands, 144) == address(descriptor0) && qword(reverbCommands, 152) == 0x1030A);
    Require(qword(descriptor0, 0) == address(memory) && dword(descriptor0, 0xC8) == 3 * 1024 && dword(descriptor0, 0x148) == 4 * 1024 && dword(descriptor0, 0x174) == 256);
    reverbInput.blockSize = 512;
    filterA.blockSize = 512;
    outputA.blockSize = 512;

    reverbInput.currentBlock = 0;
    reverbInput.availableBlocks = 0;
    reverbBatch = {reverbCommands, 0, 0x280 + 175};
    Require(reverb(1, reverbFilters, reverbOutputs) == static_cast<int>(0x81940009) && ringIs(3, 1));
    Require(reverbBatch.offset == 0 && reverbBatch.buffer_size == 0x280 + 175);
    reverbBatch = {reverbCommands, 0, sizeof(reverbCommands)};
    samplesA[1] = memory + 0xB084;
    Require(reverb(1, reverbFilters, reverbOutputs) == static_cast<int>(0x8194000A));
    samplesA[1] = memory + 0xB080;
    outputA.channelCount = 1;
    Require(reverb(1, reverbFilters, reverbOutputs) == BadPointer);
    filterA.channelCount = 3;
    outputA.channelCount = 3;
    Require(reverb(1, reverbFilters, reverbOutputs) == BadPointer);
    Require(reverbBatch.offset == 0 && reverbBatch.buffer_size == sizeof(reverbCommands));

    Require(sceAcmContextDestroy(context1) == 0);
    Require(sceAcmContextDestroy(context1) == BadContext);
    Require(sceAcmBatchStartBuffer(context1, nullptr, 0, nullptr, &batch1) == BadContext);
    Require(sceAcmBatchStartBuffers(context1, 0, nullptr, nullptr, &batch1) == BadContext);
    Require(sceAcmBatchWait(context1, batch1, 0) == BadContext);

    Require(sceAcmContextDestroy(context2) == 0);
    Require(sceAcmContextDestroy(context2) == BadContext);

    return 0;
}
