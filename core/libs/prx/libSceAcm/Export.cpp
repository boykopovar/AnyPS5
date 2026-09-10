#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAcmBatchStartBuffer(AcmContextId context, const void* batch_commands, size_t batch_size, AcmBatchError* batch_error, AcmBatchId* batch) {
 (void)context;
 (void)batch_commands;
 (void)batch_size;
 (void)batch_error;
 (void)batch;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAcmBatchStartBuffers(AcmContextId context, uint32_t batch_info_count, const AcmBatchInfo* const batch_info[], AcmBatchError* batch_error, AcmBatchId* batch) {
 (void)context;
 (void)batch_info_count;
 (void)batch_info;
 (void)batch_error;
 (void)batch;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAcmBatchWait(AcmContextId context, AcmBatchId batch, uint32_t timeout) {
 (void)context;
 (void)batch;
 (void)timeout;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAcmContextCreate(AcmContextId* context) {
 (void)context;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAcmContextDestroy(AcmContextId context) {
 (void)context;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
