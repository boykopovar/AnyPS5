#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAcmBatchStartBuffer(AcmContextId context, const void* batchCommands, size_t batchSize, AcmBatchError* batchError, AcmBatchId* batch);
int APS5_VABI sceAcmBatchStartBuffers(AcmContextId context, uint32_t batchInfoCount, const AcmBatchInfo* const batch_info[], AcmBatchError* batchError, AcmBatchId* batch);
int APS5_VABI sceAcmBatchWait(AcmContextId context, AcmBatchId batch, uint32_t timeout);
int APS5_VABI sceAcmContextCreate(AcmContextId* context);
int APS5_VABI sceAcmContextDestroy(AcmContextId context);
int APS5_VABI sceAcm_ConvReverb_SharedInput(AcmBatchInfo* batch, std::uint32_t mode, AcmReverbInput* input, std::uint32_t outputCount, const AcmReverbFilter* const* filters, const float* gains, const AcmReverbOutput* const* outputs);
int APS5_VABI sceAcm_FFT(AcmBatchInfo* batch, std::int32_t transformSize, std::uint32_t transformCount, std::int32_t inputFormat, const void* const* inputs, std::int32_t outputFormat, void* const* outputs, std::int32_t flags);
int APS5_VABI sceAcm_Panner(AcmBatchInfo* batch, std::uint32_t sourceCount, const void* const* sources, std::uint32_t sourceMode, std::uint32_t outputMode, std::uint32_t channelCount, const void* const* sourceParameters, const void* const* sourceMatrices, const void* const* inputChannels, void* const* outputChannels);
int APS5_VABI sceAcmBatchJobPriority(AcmBatchInfo* batch, std::uint32_t priority);
int APS5_VABI sceAcm_ConvReverb_SharedIr();
int APS5_VABI sceAcm_IFFT();
int APS5_VABI sceAcmBatchJobNotification();

}
