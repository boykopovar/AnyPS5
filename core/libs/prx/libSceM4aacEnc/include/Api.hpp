#pragma once

#include "SceTypes.hpp"

extern "C" {

int32_t APS5_VABI sceM4aacEncClearContext(int32_t m4aacEncHandle);
int APS5_VABI sceM4aacEncClearContextEx();
int32_t APS5_VABI sceM4aacEncCreateEncoder(int32_t channels, int32_t samplingRate, int32_t bitRate, int32_t inputFormat, int32_t outputFormat, int32_t *pInternalError);
int APS5_VABI sceM4aacEncCreateEncoderEx();
int32_t APS5_VABI sceM4aacEncDeleteEncoder(int32_t m4aacEncHandle, int32_t *pInternalError);
int APS5_VABI sceM4aacEncDeleteEncoderEx();
int32_t APS5_VABI sceM4aacEncEncode(int32_t m4aacEncHandle, void *pInput, uint32_t inputByte, void *pOutput, uint32_t *pOutputByte, int32_t *pInternalError);
int APS5_VABI sceM4aacEncEncodeEx();
int APS5_VABI sceM4aacEncEncodeWithPriority();
int APS5_VABI sceM4aacEncEncodeWithPriorityEx();
int32_t APS5_VABI sceM4aacEncFlush(int32_t m4aacEncHandle, void *pOutput, uint32_t *pOutputByte, int32_t *pInternalError);
int APS5_VABI sceM4aacEncFlushEx();
int APS5_VABI sceM4aacEncFlushWithPriority();
int APS5_VABI sceM4aacEncFlushWithPriorityEx();
int APS5_VABI sceM4aacEncGetContext();
int APS5_VABI sceM4aacEncInitialize();
int APS5_VABI sceM4aacEncTerminate();

}
