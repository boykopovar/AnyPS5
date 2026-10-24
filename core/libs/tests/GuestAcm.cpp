#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "SceTypes.hpp"

extern "C" {
int APS5_VABI sceAcmContextCreate(std::uint32_t*);
int APS5_VABI sceAcmContextDestroy(std::uint32_t);
int APS5_VABI sceAcmBatchStartBuffer(std::uint32_t, const void*, std::size_t, void*, std::uint32_t*);
int APS5_VABI sceAcmBatchStartBuffers(std::uint32_t, std::uint32_t, const void* const[], void*, std::uint32_t*);
int APS5_VABI sceAcmBatchWait(std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceAcm_ConvReverb_SharedInput(AcmBatchInfo*, std::uint32_t, AcmReverbInput*, std::uint32_t, const AcmReverbFilter* const*, const float*, const AcmReverbOutput* const*);
int APS5_VABI sceAcm_FFT(AcmBatchInfo*, std::int32_t, std::uint32_t, std::int32_t, const void* const*, std::int32_t, void* const*, std::int32_t);
int APS5_VABI sceAcm_Panner(AcmBatchInfo*, std::uint32_t, const void* const*, std::uint32_t, std::uint32_t, std::uint32_t, const void* const*, const void* const*, const void* const*, void* const*);
int APS5_VABI sceAcmBatchJobPriority(AcmBatchInfo*, std::uint32_t);
}

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

    Require(sceAcm_ConvReverb_SharedInput(nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr) == 0);
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
    Require(sceAcm_FFT(&commandBatch, 2048, 1, 0, sources, 0, outputs, 0) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 512, 1, 0, sources, 0, outputs, 3) == BadPointer);
    Require(sceAcm_FFT(&commandBatch, 512, 1, 0, sources, 0, outputs, 0) == 0);
    Require(commandBatch.offset == 72 && commandBatch.buffer_size == sizeof(commands));
    std::uint64_t fftWords[9];
    std::memcpy(fftWords, commands, sizeof(fftWords));
    Require(fftWords[0] == 0x1C030400 && fftWords[1] == 0xC0A);
    Require(fftWords[2] == 0x400000006 && fftWords[3] == reinterpret_cast<std::uintptr_t>(sourceData));
    Require(fftWords[4] == 0x90A && fftWords[5] == 0xB0A && fftWords[6] == 0x40A);
    Require(fftWords[7] == 0x400000008 && fftWords[8] == reinterpret_cast<std::uintptr_t>(sourceData));
    commandBatch = {commands, 0, 71};
    Require(sceAcm_FFT(&commandBatch, 512, 1, 0, sources, 0, outputs, 0) == static_cast<int>(0x81940009));
    Require(commandBatch.offset == 0);
    commandBatch = {commands, 0, sizeof(commands)};
    Require(sceAcm_FFT(&commandBatch, 2048, 1, 1, sources, 1, outputs, 9) == 0);
    std::memcpy(fftWords, commands, sizeof(fftWords));
    Require(fftWords[0] == 0x600 && fftWords[1] == 0x80A && fftWords[4] == 0x20A && fftWords[5] == 0x60A && fftWords[6] == 0x50A);
    sources[1] = sourceData;
    outputs[1] = sourceData;
    for (const auto size : {512, 1024, 2048}) {
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
                }
            }
        }
    }
    commandBatch = {commands, 0, 152};
    Require(sceAcm_Panner(&commandBatch, 1, sources, 0, 0, 1, sources, sources, nullptr, outputs) == static_cast<int>(0x81940009));
    Require(commandBatch.offset == 0 && commandBatch.buffer_size == 152);
    Require(sceAcmBatchJobPriority(nullptr, 0) == 0);

    Require(sceAcmContextDestroy(context1) == 0);
    Require(sceAcmContextDestroy(context1) == BadContext);
    Require(sceAcmBatchStartBuffer(context1, nullptr, 0, nullptr, &batch1) == BadContext);
    Require(sceAcmBatchWait(context1, batch1, 0) == BadContext);

    Require(sceAcmContextDestroy(context2) == 0);
    Require(sceAcmContextDestroy(context2) == BadContext);

    return 0;
}
