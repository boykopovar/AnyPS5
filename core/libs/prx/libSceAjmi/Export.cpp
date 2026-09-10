#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAjmBatchCancel(uint32_t context, uint32_t batch) {
 (void)context;
 (void)batch;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchErrorDump(const AjmBatchInfo* info, AjmBatchError* error) {
 (void)info;
 (void)error;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchWait(uint32_t context, uint32_t batch, uint32_t timeout, AjmBatchError* error) {
 (void)context;
 (void)batch;
 (void)timeout;
 (void)error;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmDecAt9ParseConfigData(const void* config_data, AjmDecAt9ConfigDataInfo* config_info) {
 (void)config_data;
 (void)config_info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmFinalize(uint32_t context) {
 (void)context;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmInitialize(int64_t reserved, uint32_t* context) {
 (void)reserved;
 (void)context;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmInstanceCreate(uint32_t context, uint32_t codec, uint64_t flags, uint32_t* instance) {
 (void)context;
 (void)codec;
 (void)flags;
 (void)instance;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmInstanceDestroy(uint32_t context, uint32_t instance) {
 (void)context;
 (void)instance;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmMemoryRegister(uint32_t context, void* ptr, size_t pages) {
 (void)context;
 (void)ptr;
 (void)pages;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmMemoryUnregister(uint32_t context, void* ptr) {
 (void)context;
 (void)ptr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmModuleRegister(uint32_t context, uint32_t codec, int64_t reserved) {
 (void)context;
 (void)codec;
 (void)reserved;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmModuleUnregister(uint32_t context, uint32_t codec) {
 (void)context;
 (void)codec;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

const char* sceAjmStrError(int error) {
 (void)error;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}
