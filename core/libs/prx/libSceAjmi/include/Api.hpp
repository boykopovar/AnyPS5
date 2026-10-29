#pragma once

#include "SceTypes.hpp"

extern "C" {

void* APS5_VABI sceAjmBatchJobInlineBuffer_nid_disambig1(void* position, const void* data_input, size_t data_input_size, const void** batch_address);
int APS5_VABI sceAjmBatchJobRunBufferRa_nid_disambig1(void);

}
