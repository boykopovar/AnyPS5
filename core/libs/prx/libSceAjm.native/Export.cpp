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

const char* APS5_VABI sceAjmStrError(int error) {
 (void)error;
 AjmStub(__func__);
 return nullptr;
}

APS5_EXPORT("Z4HDpe1ZEa4", sceAjmWVorbisUnknown00);
int APS5_VABI sceAjmWVorbisUnknown00(void) {
    NotImplemented_nid_no_patch("Z4HDpe1ZEa4");
    return 0;
}

APS5_EXPORT("oXShHcqMcX0", sceAjmWVorbisUnknown01);
int APS5_VABI sceAjmWVorbisUnknown01(void) {
    NotImplemented_nid_no_patch("oXShHcqMcX0");
    return 0;
}

}
