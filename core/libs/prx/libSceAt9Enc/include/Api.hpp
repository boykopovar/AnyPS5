#pragma once

#include "SceTypes.hpp"

extern "C" {

int32_t APS5_VABI sceAt9EncClearContext(void *pAt9EncWorkArea, int32_t *pInternalResult);
int32_t APS5_VABI sceAt9EncCreateEncoder(void *pAt9EncWorkArea, void *pAt9EncParam, int32_t memSize, int32_t *pInternalResult);
int32_t APS5_VABI sceAt9EncEncode(void *pAt9EncWorkArea, const void *pInputBuf, void *pOutputBuf, uint32_t inputBufSize, uint32_t outputBufSize, uint32_t *pOutputDataByte, uint32_t *pUsedBufSize, int32_t *pInternalResult);
int32_t APS5_VABI sceAt9EncFlush(void *pAt9EncWorkArea, void *pOutputBuf, uint32_t outputBufSize, uint32_t *pOutputDataByte, int32_t *pInternalResult);
int32_t APS5_VABI sceAt9EncQueryMemSize(void *pAt9EncParam, int32_t *pMemSize, int32_t *pInternalResult);

}
