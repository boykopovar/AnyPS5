#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceAjmBatchCancel(uint32_t context, uint32_t batch) {
 (void)context;
 (void)batch;
 return 0;
}

int sceAjmBatchErrorDump(const AjmBatchInfo* info, AjmBatchError* error) {
 (void)info;
 (void)error;
 return 0;
}

int sceAjmBatchWait(uint32_t context, uint32_t batch, uint32_t timeout, AjmBatchError* error) {
 (void)context;
 (void)batch;
 (void)timeout;
 (void)error;
 return 0;
}

int sceAjmDecAt9ParseConfigData(const void* config_data, AjmDecAt9ConfigDataInfo* config_info) {
 (void)config_data;
 (void)config_info;
 return 0;
}

int sceAjmFinalize(uint32_t context) {
 (void)context;
 return 0;
}

int sceAjmInitialize(int64_t reserved, uint32_t* context) {
 (void)reserved;
 (void)context;
 return 0;
}

int sceAjmInstanceCreate(uint32_t context, uint32_t codec, uint64_t flags, uint32_t* instance) {
 (void)context;
 (void)codec;
 (void)flags;
 (void)instance;
 return 0;
}

int sceAjmInstanceDestroy(uint32_t context, uint32_t instance) {
 (void)context;
 (void)instance;
 return 0;
}

int sceAjmMemoryRegister(uint32_t context, void* ptr, size_t pages) {
 (void)context;
 (void)ptr;
 (void)pages;
 return 0;
}

int sceAjmMemoryUnregister(uint32_t context, void* ptr) {
 (void)context;
 (void)ptr;
 return 0;
}

int sceAjmModuleRegister(uint32_t context, uint32_t codec, int64_t reserved) {
 (void)context;
 (void)codec;
 (void)reserved;
 return 0;
}

int sceAjmModuleUnregister(uint32_t context, uint32_t codec) {
 (void)context;
 (void)codec;
 return 0;
}

const char* sceAjmStrError(int error) {
 (void)error;
 return nullptr;
}

}
