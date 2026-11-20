#pragma once

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAjmGetFailedInstance_nid_disambig1(uint32_t context, void* failed);
int APS5_VABI sceAjmInstanceGetSize_nid_disambig1(uint32_t context, uint32_t codec, uint64_t flags);
int APS5_VABI sceAjmTelemetryGetLog_nid_disambig1(uint32_t context, uint8_t* flag, void* log);

int APS5_VABI sceAjmBatchCancel_nid_postfix(uint32_t context, uint32_t batch);
int APS5_VABI sceAjmBatchErrorDump_nid_postfix(const AjmBatchInfo* info, AjmBatchError* error);
int APS5_VABI sceAjmBatchGetLog_nid_postfix();
int APS5_VABI sceAjmBatchInitialize_nid_postfix(void* buffer, size_t size, AjmBatchInfo* info);
int APS5_VABI sceAjmBatchJobClearContext_nid_postfix(AjmBatchInfo* info, uint32_t instance, void* result);
int APS5_VABI sceAjmBatchJobControl_nid_postfix(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* sidebandInput, size_t sidebandInputSize, void* sidebandOutput, size_t sidebandOutputSize);
void* APS5_VABI sceAjmBatchJobControlBufferRa_nid_postfix(void* position, uint32_t instance, uint64_t flags, const void* sidebandInput, size_t sidebandInputSize, void* sidebandOutput, size_t sidebandOutputSize, void* returnAddress);
int APS5_VABI sceAjmBatchJobDecode_nid_postfix(AjmBatchInfo* info, uint32_t instance, const void* bitstreamInput, size_t bitstreamInputSize, void* pcmOutput, size_t pcmOutputSize, void* result);
int APS5_VABI sceAjmBatchJobDecodeSingle_nid_postfix(AjmBatchInfo* info, uint32_t instance, const void* bitstreamInput, size_t bitstreamInputSize, void* pcmOutput, size_t pcmOutputSize, void* result);
int APS5_VABI sceAjmBatchJobDecodeSplit_nid_postfix(AjmBatchInfo* info, uint32_t instance, const AjmBuffer* inputBuffers, size_t inputBuffersNum, const AjmBuffer* outputBuffers, size_t outputBuffersNum, void* result);
int APS5_VABI sceAjmBatchJobEncode_nid_postfix(AjmBatchInfo* info, uint32_t instance, const void* pcmInput, size_t pcmInputSize, void* bitstreamOutput, size_t bitstreamOutputSize, void* result);
int APS5_VABI sceAjmBatchJobGetCodecInfo_nid_postfix(AjmBatchInfo* info, uint32_t instance, void* result, size_t resultSize);
int APS5_VABI sceAjmBatchJobGetGaplessDecode_nid_postfix(AjmBatchInfo* info, uint32_t instance, void* result);
int APS5_VABI sceAjmBatchJobGetInfo_nid_postfix(AjmBatchInfo* info, uint32_t instance, void* result);
int APS5_VABI sceAjmBatchJobGetResampleInfo_nid_postfix(AjmBatchInfo* info, uint32_t instance, void* result);
int APS5_VABI sceAjmBatchJobGetStatistics_nid_postfix(AjmBatchInfo* info, float interval, void* result);
int APS5_VABI sceAjmBatchJobInitialize_nid_postfix(AjmBatchInfo* info, uint32_t instance, const void* codecParameters, size_t codecParametersSize, void* result);
void* APS5_VABI sceAjmBatchJobInlineBuffer_nid_postfix(void* position, const void* dataInput, size_t dataInputSize, const void** batchAddress);
int APS5_VABI sceAjmBatchJobRun_nid_postfix(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* dataInput, size_t dataInputSize, void* dataOutput, size_t dataOutputSize, void* sidebandOutput, size_t sidebandOutputSize);
void* APS5_VABI sceAjmBatchJobRunBufferRa_nid_postfix(void* position, uint32_t instance, uint64_t flags, const void* dataInput, size_t dataInputSize, void* dataOutput, size_t dataOutputSize, void* sidebandOutput, size_t sidebandOutputSize, void* returnAddress);
int APS5_VABI sceAjmBatchJobRunSplit_nid_postfix(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const AjmBuffer* inputBuffers, size_t inputBuffersNum, const AjmBuffer* outputBuffers, size_t outputBuffersNum, void* sidebandOutput, size_t sidebandOutputSize);
void* APS5_VABI sceAjmBatchJobRunSplitBufferRa_nid_postfix(void* position, uint32_t instance, uint64_t flags, const AjmBuffer* inputBuffers, size_t inputBuffersNum, const AjmBuffer* outputBuffers, size_t outputBuffersNum, void* sidebandOutput, size_t sidebandOutputSize, void* returnAddress);
int APS5_VABI sceAjmBatchJobSetGaplessDecode_nid_postfix(AjmBatchInfo* info, uint32_t instance, const void* gaplessDecode, int reset, void* result);
int APS5_VABI sceAjmBatchJobSetResampleParameters_nid_postfix(AjmBatchInfo* info, uint32_t instance, float ratio, uint32_t flags, void* result);
int APS5_VABI sceAjmBatchJobSetResampleParametersEx_nid_postfix(AjmBatchInfo* info, uint32_t instance, float ratioStart, float ratioChangePerSample, uint32_t flags, void* result);
int APS5_VABI sceAjmBatchStart_nid_postfix(uint32_t context, const AjmBatchInfo* info, int priority, AjmBatchError* error, uint32_t* batch);
int APS5_VABI sceAjmBatchStartBuffer_nid_postfix(uint32_t context, const uint8_t* batch, uint32_t batchSize, int priority, AjmBatchError* error, uint32_t* batchId);
int APS5_VABI sceAjmBatchWait_nid_postfix(uint32_t context, uint32_t batch, uint32_t timeout, AjmBatchError* error);
int APS5_VABI sceAjmDecAt9ParseConfigData_nid_postfix(const void* configData, AjmDecAt9ConfigDataInfo* configInfo);
int APS5_VABI sceAjmDecMp3ParseFrame_nid_postfix(const uint8_t* stream, uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame);
int APS5_VABI sceAjmFinalize_nid_postfix(uint32_t context);
int APS5_VABI sceAjmInitialize_nid_postfix(int64_t reserved, uint32_t* context);
int APS5_VABI sceAjmInstanceAvailable_nid_postfix();
uint32_t APS5_VABI sceAjmInstanceCodecType_nid_postfix(uint32_t instanceId);
int APS5_VABI sceAjmInstanceCreate_nid_postfix(uint32_t context, uint32_t codec, uint64_t flags, uint32_t* instance);
int APS5_VABI sceAjmInstanceDestroy_nid_postfix(uint32_t context, uint32_t instance);
int APS5_VABI sceAjmInstanceExtend_nid_postfix(uint32_t context, uint32_t codec, uint64_t flags, uint32_t instance);
int APS5_VABI sceAjmInstanceSwitch_nid_postfix(uint32_t context, uint32_t codec, uint64_t flags, uint32_t instance);
int APS5_VABI sceAjmMemoryRegister_nid_postfix(uint32_t context, void* ptr, size_t pages);
int APS5_VABI sceAjmMemoryUnregister_nid_postfix(uint32_t context, void* ptr);
int APS5_VABI sceAjmModuleRegister_nid_postfix(uint32_t context, uint32_t codec, int64_t reserved);
int APS5_VABI sceAjmModuleUnregister_nid_postfix(uint32_t context, uint32_t codec);
const char* APS5_VABI sceAjmStrError_nid_postfix(int error);

}
