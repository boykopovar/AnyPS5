#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstdio>

// Entry points this library does not implement: reported before the unimplemented-call exception so an
// APS5_TRACE_AJM run shows which one a title reached.
static void AjmStub(const char* name) {
    std::fprintf(stderr, "[ajm] unimplemented %s called\n", name);
    NotImplemented_nid_no_patch(name);
}

extern "C" {

int APS5_VABI sceAjmBatchCancel(uint32_t context, uint32_t batch) {
 (void)context;
 (void)batch;
 AjmStub(__func__);
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
 AjmStub(__func__);
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
 AjmStub(__func__);
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
 AjmStub(__func__);
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
 AjmStub(__func__);
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
 AjmStub(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobGetCodecInfo(AjmBatchInfo* info, uint32_t instance, void* result, size_t result_size) {
 (void)info;
 (void)instance;
 (void)result;
 (void)result_size;
 AjmStub(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobGetGaplessDecode(AjmBatchInfo* info, uint32_t instance, void* result) {
 (void)info;
 (void)instance;
 (void)result;
 AjmStub(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobGetInfo(AjmBatchInfo* info, uint32_t instance, void* result) {
 (void)info;
 (void)instance;
 (void)result;
 AjmStub(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobGetResampleInfo(AjmBatchInfo* info, uint32_t instance, void* result) {
 (void)info;
 (void)instance;
 (void)result;
 AjmStub(__func__);
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
 AjmStub(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobSetResampleParameters(AjmBatchInfo* info, uint32_t instance, float ratio, uint32_t flags, void* result) {
 (void)info;
 (void)instance;
 (void)ratio;
 (void)flags;
 (void)result;
 AjmStub(__func__);
 return 0;
}

int APS5_VABI sceAjmBatchJobSetResampleParametersEx(AjmBatchInfo* info, uint32_t instance, float ratio_start, float ratio_change_per_sample, uint32_t flags, void* result) {
 (void)info;
 (void)instance;
 (void)ratio_start;
 (void)ratio_change_per_sample;
 (void)flags;
 (void)result;
 AjmStub(__func__);
 return 0;
}

const char* APS5_VABI sceAjmStrError(int error) {
 (void)error;
 AjmStub(__func__);
 return nullptr;
}

int APS5_VABI sceAjmDecMp3ParseFrame() {
 AjmStub(__func__);
 return 0;
}

}
