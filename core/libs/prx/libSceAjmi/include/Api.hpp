#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

extern "C" {

void* APS5_VABI sceAjmBatchJobInlineBuffer_nid_disambig1(void* position, const void* data_input, size_t data_input_size, const void** batch_address);
void* APS5_VABI sceAjmBatchJobRunBufferRa_nid_disambig1(void* position, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size, void* return_address);
void* APS5_VABI sceAjmiBatchJobInlineBuffer_nid_no_patch(void* position, const void* data, std::size_t size, const void** address);
void* APS5_VABI sceAjmiBatchJobRunBufferRa_nid_no_patch(void* position, std::uint32_t instance, std::uint64_t flags, const void* input, std::size_t inputSize, void* output, std::size_t outputSize, void* sideband, std::size_t sidebandSize, void* returnAddress);

}
