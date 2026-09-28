#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>

// Audio co-processor batches complete immediately without producing output; audio is silent.
static constexpr int SCE_ACM_ERROR_INVALID_PARAMETER = static_cast<int>(0x80E30002);
static std::atomic<uint32_t> g_nextId{1};

extern "C" {

int APS5_VABI sceAcmBatchStartBuffer(AcmContextId context, const void* batch_commands, size_t batch_size, AcmBatchError* batch_error, AcmBatchId* batch) {
    (void)context;
    (void)batch_commands;
    (void)batch_size;
    (void)batch_error;
    if (!batch) return SCE_ACM_ERROR_INVALID_PARAMETER;
    *batch = g_nextId.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceAcmBatchStartBuffers(AcmContextId context, uint32_t batch_info_count, const AcmBatchInfo* const batch_info[], AcmBatchError* batch_error, AcmBatchId* batch) {
    (void)context;
    (void)batch_info_count;
    (void)batch_info;
    (void)batch_error;
    if (!batch) return SCE_ACM_ERROR_INVALID_PARAMETER;
    *batch = g_nextId.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceAcmBatchWait(AcmContextId context, AcmBatchId batch, uint32_t timeout) {
    (void)context;
    (void)batch;
    (void)timeout;
    return 0;
}

int APS5_VABI sceAcmContextCreate(AcmContextId* context) {
    if (!context) return SCE_ACM_ERROR_INVALID_PARAMETER;
    *context = g_nextId.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceAcmContextDestroy(AcmContextId context) {
    (void)context;
    return 0;
}

// Builds a convolution-reverb command into a batch; batches execute as no-ops, so nothing is encoded.
int APS5_VABI sceAcm_ConvReverb_SharedInput(void) {
    return 0;
}

}
