#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceAcmContextCreate(std::uint32_t*);
int APS5_VABI sceAcmContextDestroy(std::uint32_t);
int APS5_VABI sceAcmBatchStartBuffer(std::uint32_t, const void*, std::size_t, void*, std::uint32_t*);
int APS5_VABI sceAcmBatchStartBuffers(std::uint32_t, std::uint32_t, const void* const[], void*, std::uint32_t*);
int APS5_VABI sceAcmBatchWait(std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceAcm_ConvReverb_SharedInput(void);
int APS5_VABI sceAcm_FFT(void);
int APS5_VABI sceAcm_Panner(void);
int APS5_VABI sceAcmBatchJobPriority(void);
}

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    std::uint32_t context = 0;
    Require(sceAcmContextCreate(nullptr) != 0);
    Require(sceAcmContextCreate(&context) == 0 && context != 0);

    std::uint32_t batch = 0;
    Require(sceAcmBatchStartBuffer(context, nullptr, 0, nullptr, nullptr) != 0);
    Require(sceAcmBatchStartBuffer(context, nullptr, 0, nullptr, &batch) == 0 && batch != 0);
    Require(sceAcmBatchWait(context, batch, 0) == 0);

    std::uint32_t batch2 = 0;
    Require(sceAcmBatchStartBuffers(context, 0, nullptr, nullptr, nullptr) != 0);
    Require(sceAcmBatchStartBuffers(context, 0, nullptr, nullptr, &batch2) == 0 && batch2 != 0);
    Require(sceAcmBatchWait(context, batch2, 0) == 0);

    Require(sceAcm_ConvReverb_SharedInput() == 0);
    Require(sceAcm_FFT() == 0);
    Require(sceAcm_Panner() == 0);
    Require(sceAcmBatchJobPriority() == 0);

    Require(sceAcmContextDestroy(context) == 0);
}
