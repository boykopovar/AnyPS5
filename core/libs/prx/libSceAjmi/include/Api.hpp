#pragma once

#include "SceTypes.hpp"

extern "C" {

void* APS5_VABI sceAjmBatchJobInlineBuffer_nid_disambig1(void* position, const void* data_input, size_t data_input_size, const void** batch_address);
void* APS5_VABI sceAjmBatchJobControlBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* sideband_input, size_t sideband_input_size, void* sideband_output, size_t sideband_output_size, void* return_address);
void* APS5_VABI sceAjmBatchJobRunSplitBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size, void* return_address);
void* APS5_VABI sceAjmBatchJobRunBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size, void* return_address);

}
