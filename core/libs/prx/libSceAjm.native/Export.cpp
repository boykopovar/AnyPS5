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

int APS5_VABI sceAjmBatchInitialize(void* buffer, size_t size, AjmBatchInfo* info) {
 (void)buffer;
 (void)size;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobClearContext(AjmBatchInfo* info, uint32_t instance, void* result) {
 (void)info;
 (void)instance;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobControl(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* sideband_input, size_t sideband_input_size, void* sideband_output, size_t sideband_output_size) {
 (void)info;
 (void)instance;
 (void)flags;
 (void)sideband_input;
 (void)sideband_input_size;
 (void)sideband_output;
 (void)sideband_output_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobDecode(AjmBatchInfo* info, uint32_t instance, const void* bitstream_input, size_t bitstream_input_size, void* pcm_output, size_t pcm_output_size, void* result) {
 (void)info;
 (void)instance;
 (void)bitstream_input;
 (void)bitstream_input_size;
 (void)pcm_output;
 (void)pcm_output_size;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobDecodeSingle(AjmBatchInfo* info, uint32_t instance, const void* bitstream_input, size_t bitstream_input_size, void* pcm_output, size_t pcm_output_size, void* result) {
 (void)info;
 (void)instance;
 (void)bitstream_input;
 (void)bitstream_input_size;
 (void)pcm_output;
 (void)pcm_output_size;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobDecodeSplit(AjmBatchInfo* info, uint32_t instance, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* result) {
 (void)info;
 (void)instance;
 (void)input_buffers;
 (void)input_buffers_num;
 (void)output_buffers;
 (void)output_buffers_num;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobEncode(AjmBatchInfo* info, uint32_t instance, const void* pcm_input, size_t pcm_input_size, void* bitstream_output, size_t bitstream_output_size, void* result) {
 (void)info;
 (void)instance;
 (void)pcm_input;
 (void)pcm_input_size;
 (void)bitstream_output;
 (void)bitstream_output_size;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobGetCodecInfo(AjmBatchInfo* info, uint32_t instance, void* result, size_t result_size) {
 (void)info;
 (void)instance;
 (void)result;
 (void)result_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobGetGaplessDecode(AjmBatchInfo* info, uint32_t instance, void* result) {
 (void)info;
 (void)instance;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobGetInfo(AjmBatchInfo* info, uint32_t instance, void* result) {
 (void)info;
 (void)instance;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobGetResampleInfo(AjmBatchInfo* info, uint32_t instance, void* result) {
 (void)info;
 (void)instance;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobGetStatistics(AjmBatchInfo* info, float interval, void* result) {
 (void)info;
 (void)interval;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobInitialize(AjmBatchInfo* info, uint32_t instance, const void* codec_parameters, size_t codec_parameters_size, void* result) {
 (void)info;
 (void)instance;
 (void)codec_parameters;
 (void)codec_parameters_size;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobRun(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size) {
 (void)info;
 (void)instance;
 (void)flags;
 (void)data_input;
 (void)data_input_size;
 (void)data_output;
 (void)data_output_size;
 (void)sideband_output;
 (void)sideband_output_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobRunSplit(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size) {
 (void)info;
 (void)instance;
 (void)flags;
 (void)input_buffers;
 (void)input_buffers_num;
 (void)output_buffers;
 (void)output_buffers_num;
 (void)sideband_output;
 (void)sideband_output_size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobSetGaplessDecode(AjmBatchInfo* info, uint32_t instance, const void* gapless_decode, int reset, void* result) {
 (void)info;
 (void)instance;
 (void)gapless_decode;
 (void)reset;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobSetResampleParameters(AjmBatchInfo* info, uint32_t instance, float ratio, uint32_t flags, void* result) {
 (void)info;
 (void)instance;
 (void)ratio;
 (void)flags;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobSetResampleParametersEx(AjmBatchInfo* info, uint32_t instance, float ratio_start, float ratio_change_per_sample, uint32_t flags, void* result) {
 (void)info;
 (void)instance;
 (void)ratio_start;
 (void)ratio_change_per_sample;
 (void)flags;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchStart(uint32_t context, const AjmBatchInfo* info, int priority, AjmBatchError* error, uint32_t* batch) {
 (void)context;
 (void)info;
 (void)priority;
 (void)error;
 (void)batch;
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
