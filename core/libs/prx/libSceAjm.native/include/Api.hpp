#pragma once

#include "SceTypes.hpp"

struct WVorbisHeaderPacketParameter {
    std::size_t size;
    std::int32_t channels;
    std::uint32_t sampleRate;
    std::int32_t bytesPerSecond;
    std::int32_t shortBlockExponent;
    std::int32_t longBlockExponent;
    std::uint32_t reserved;
    const void* setupPacket;
    std::size_t setupPacketSize;
    void* output;
    std::size_t outputCapacity;
    std::size_t outputSize;
};

struct WVorbisSetupPacketParameter {
    std::size_t size;
    std::int32_t channels;
    std::uint32_t reserved;
    const void* input;
    std::size_t inputSize;
    void* output;
    std::size_t outputCapacity;
    std::size_t outputSize;
};

extern "C" {

int APS5_VABI sceAjmBatchJobEncode(AjmBatchInfo* info, uint32_t instance, const void* pcmInput, size_t pcmInputSize, void* bitstreamOutput, size_t bitstreamOutputSize, void* result);
const char* APS5_VABI sceAjmStrError(int error);
int APS5_VABI sceAjmDecWVorbisCreateHeaderPacket(WVorbisHeaderPacketParameter* param);
int APS5_VABI sceAjmDecWVorbisCreateSetupPacket(WVorbisSetupPacketParameter* parameter);
int APS5_VABI sceAjmBatchGetLog();
int APS5_VABI sceAjmInstanceAvailable();
uint32_t APS5_VABI sceAjmInstanceCodecType(uint32_t instanceId);
int APS5_VABI sceAjmInstanceExtend(uint32_t context, uint32_t codec, uint64_t flags, uint32_t instance);
int APS5_VABI sceAjmInstanceSwitch(uint32_t context, uint32_t codec, uint64_t flags, uint32_t instance);
int APS5_VABI sceAjmInitialize(int64_t reserved, uint32_t* context);
int APS5_VABI sceAjmFinalize(uint32_t context);
int APS5_VABI sceAjmModuleRegister(uint32_t context, uint32_t codec, int64_t reserved);
int APS5_VABI sceAjmModuleUnregister(uint32_t context, uint32_t codec);
int APS5_VABI sceAjmMemoryRegister(uint32_t context, void* ptr, size_t pages);
int APS5_VABI sceAjmMemoryUnregister(uint32_t context, void* ptr);
int APS5_VABI sceAjmInstanceCreate(uint32_t context, uint32_t codec, uint64_t flags, uint32_t* instance);
int APS5_VABI sceAjmInstanceDestroy(uint32_t context, uint32_t instance);
int APS5_VABI sceAjmDecAt9ParseConfigData(const void* configData, AjmDecAt9ConfigDataInfo* configInfo);
int APS5_VABI sceAjmDecMp3ParseFrame(const uint8_t* stream, uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame);
int APS5_VABI sceAjmBatchInitialize(void* buffer, size_t size, AjmBatchInfo* info);
int APS5_VABI sceAjmBatchJobInitialize(AjmBatchInfo* info, uint32_t instance, const void* codecParameters, size_t codecParametersSize, void* result);
int APS5_VABI sceAjmBatchJobClearContext(AjmBatchInfo* info, uint32_t instance, void* result);
int APS5_VABI sceAjmBatchJobSetGaplessDecode(AjmBatchInfo* info, uint32_t instance, const void* gaplessDecode, int reset, void* result);
int APS5_VABI sceAjmBatchJobControl(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* sidebandInput, size_t sidebandInputSize, void* sidebandOutput, size_t sidebandOutputSize);
int APS5_VABI sceAjmBatchJobRunSplit(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const AjmBuffer* inputBuffers, size_t inputBuffersNum, const AjmBuffer* outputBuffers, size_t outputBuffersNum, void* sidebandOutput, size_t sidebandOutputSize);
int APS5_VABI sceAjmBatchJobRun(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* dataInput, size_t dataInputSize, void* dataOutput, size_t dataOutputSize, void* sidebandOutput, size_t sidebandOutputSize);
int APS5_VABI sceAjmBatchJobSetResampleParameters(AjmBatchInfo* info, uint32_t instance, float ratio, uint32_t flags, void* result);
int APS5_VABI sceAjmBatchJobSetResampleParametersEx(AjmBatchInfo* info, uint32_t instance, float ratioStart, float ratioChangePerSample, uint32_t flags, void* result);
int APS5_VABI sceAjmBatchJobGetResampleInfo(AjmBatchInfo* info, uint32_t instance, void* result);
int APS5_VABI sceAjmBatchJobDecode(AjmBatchInfo* info, uint32_t instance, const void* bitstreamInput, size_t bitstreamInputSize, void* pcmOutput, size_t pcmOutputSize, void* result);
int APS5_VABI sceAjmBatchJobDecodeSplit(AjmBatchInfo* info, uint32_t instance, const AjmBuffer* inputBuffers, size_t inputBuffersNum, const AjmBuffer* outputBuffers, size_t outputBuffersNum, void* result);
int APS5_VABI sceAjmBatchJobDecodeSingle(AjmBatchInfo* info, uint32_t instance, const void* bitstreamInput, size_t bitstreamInputSize, void* pcmOutput, size_t pcmOutputSize, void* result);
int APS5_VABI sceAjmBatchJobGetGaplessDecode(AjmBatchInfo* info, uint32_t instance, void* result);
int APS5_VABI sceAjmBatchJobGetInfo(AjmBatchInfo* info, uint32_t instance, void* result);
int APS5_VABI sceAjmBatchJobGetCodecInfo(AjmBatchInfo* info, uint32_t instance, void* result, size_t resultSize);
int APS5_VABI sceAjmBatchJobGetStatistics(AjmBatchInfo* info, float interval, void* result);
int APS5_VABI sceAjmBatchStart(uint32_t context, const AjmBatchInfo* info, int priority, AjmBatchError* error, uint32_t* batch);
int APS5_VABI sceAjmBatchWait(uint32_t context, uint32_t batch, uint32_t timeout, AjmBatchError* error);
int APS5_VABI sceAjmBatchCancel(uint32_t context, uint32_t batch);
int APS5_VABI sceAjmBatchErrorDump(const AjmBatchInfo* info, AjmBatchError* error);
void* APS5_VABI sceAjmBatchJobControlBufferRa(void* position, uint32_t instance, uint64_t flags, const void* sideband_input, size_t sideband_input_size, void* sideband_output, size_t sideband_output_size, void* return_address);
void* APS5_VABI sceAjmBatchJobRunBufferRa(void* position, uint32_t instance, uint64_t flags, const void* data_input, size_t data_input_size, void* data_output, size_t data_output_size, void* sideband_output, size_t sideband_output_size, void* return_address);
void* APS5_VABI sceAjmBatchJobRunSplitBufferRa(void* position, uint32_t instance, uint64_t flags, const AjmBuffer* input_buffers, size_t input_buffers_num, const AjmBuffer* output_buffers, size_t output_buffers_num, void* sideband_output, size_t sideband_output_size, void* return_address);
int APS5_VABI sceAjmBatchStartBuffer(uint32_t context, const uint8_t* batch, uint32_t batch_size, int priority, AjmBatchError* error, uint32_t* batch_id);

}
