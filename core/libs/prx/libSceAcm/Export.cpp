#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceAcmBatchStartBuffer(AcmContextId context, const void* batch_commands, size_t batch_size, AcmBatchError* batch_error, AcmBatchId* batch) {
 (void)context;
 (void)batch_commands;
 (void)batch_size;
 (void)batch_error;
 (void)batch;
 return 0;
}

int sceAcmBatchStartBuffers(AcmContextId context, uint32_t batch_info_count, const AcmBatchInfo* const batch_info[], AcmBatchError* batch_error, AcmBatchId* batch) {
 (void)context;
 (void)batch_info_count;
 (void)batch_info;
 (void)batch_error;
 (void)batch;
 return 0;
}

int sceAcmBatchWait(AcmContextId context, AcmBatchId batch, uint32_t timeout) {
 (void)context;
 (void)batch;
 (void)timeout;
 return 0;
}

int sceAcmContextCreate(AcmContextId* context) {
 (void)context;
 return 0;
}

int sceAcmContextDestroy(AcmContextId context) {
 (void)context;
 return 0;
}

}
