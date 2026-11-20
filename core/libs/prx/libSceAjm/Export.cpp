#include "prx/libSceAjm/include/Api.hpp"
#include "prx/libSceAjm.native/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_AJM_ERROR_INVALID_PARAMETER = static_cast<int>(0x80930005);
constexpr int SCE_AJM_ERROR_INVALID_FLAGS = static_cast<int>(0x8093000C);
constexpr std::uint64_t ReservedFlags = 0xf0000000ffffc000ull;

[[noreturn]] void AjmDriver(const char* function) {
    throw std::runtime_error(std::string(function) + ": the request to the AJM driver");
}

}

extern "C" {

APS5_DUMMY_FUN

int APS5_VABI sceAjmGetFailedInstance_nid_disambig1(uint32_t, void* failed) {
    if (!failed) return SCE_AJM_ERROR_INVALID_PARAMETER;
    AjmDriver(__func__);
}

int APS5_VABI sceAjmInstanceGetSize_nid_disambig1(uint32_t, uint32_t, uint64_t flags) {
    if ((flags & 0x380u) >= 0x180u || (flags & 0x2400u) == 0x2000u || (flags & ReservedFlags) != 0) return SCE_AJM_ERROR_INVALID_FLAGS;
    AjmDriver(__func__);
}

int APS5_VABI sceAjmTelemetryGetLog_nid_disambig1(uint32_t, uint8_t*, void* log) {
    if (!log) return SCE_AJM_ERROR_INVALID_PARAMETER;
    AjmDriver(__func__);
}

int APS5_VABI sceAjmBatchCancel_nid_disambig1(uint32_t context, uint32_t batch) {
    return sceAjmBatchCancel(context, batch);
}

int APS5_VABI sceAjmBatchErrorDump_nid_disambig1(const AjmBatchInfo* info, AjmBatchError* error) {
    return sceAjmBatchErrorDump(info, error);
}

int APS5_VABI sceAjmBatchGetLog_nid_disambig1() {
    return sceAjmBatchGetLog();
}

int APS5_VABI sceAjmBatchInitialize_nid_disambig1(void* buffer, size_t size, AjmBatchInfo* info) {
    return sceAjmBatchInitialize(buffer, size, info);
}

int APS5_VABI sceAjmBatchJobClearContext_nid_disambig1(AjmBatchInfo* info, uint32_t instance, void* result) {
    return sceAjmBatchJobClearContext(info, instance, result);
}

int APS5_VABI sceAjmBatchJobControl_nid_disambig1(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* sidebandInput, size_t sidebandInputSize, void* sidebandOutput, size_t sidebandOutputSize) {
    return sceAjmBatchJobControl(info, instance, flags, sidebandInput, sidebandInputSize, sidebandOutput, sidebandOutputSize);
}

void* APS5_VABI sceAjmBatchJobControlBufferRa_nid_disambig2(void* position, uint32_t instance, uint64_t flags, const void* sidebandInput, size_t sidebandInputSize, void* sidebandOutput, size_t sidebandOutputSize, void* returnAddress) {
    return sceAjmBatchJobControlBufferRa(position, instance, flags, sidebandInput, sidebandInputSize, sidebandOutput, sidebandOutputSize, returnAddress);
}

int APS5_VABI sceAjmBatchJobDecode_nid_disambig1(AjmBatchInfo* info, uint32_t instance, const void* bitstreamInput, size_t bitstreamInputSize, void* pcmOutput, size_t pcmOutputSize, void* result) {
    return sceAjmBatchJobDecode(info, instance, bitstreamInput, bitstreamInputSize, pcmOutput, pcmOutputSize, result);
}

int APS5_VABI sceAjmBatchJobDecodeSingle_nid_disambig1(AjmBatchInfo* info, uint32_t instance, const void* bitstreamInput, size_t bitstreamInputSize, void* pcmOutput, size_t pcmOutputSize, void* result) {
    return sceAjmBatchJobDecodeSingle(info, instance, bitstreamInput, bitstreamInputSize, pcmOutput, pcmOutputSize, result);
}

int APS5_VABI sceAjmBatchJobDecodeSplit_nid_disambig1(AjmBatchInfo* info, uint32_t instance, const AjmBuffer* inputBuffers, size_t inputBuffersNum, const AjmBuffer* outputBuffers, size_t outputBuffersNum, void* result) {
    return sceAjmBatchJobDecodeSplit(info, instance, inputBuffers, inputBuffersNum, outputBuffers, outputBuffersNum, result);
}

int APS5_VABI sceAjmBatchJobEncode_nid_disambig1(AjmBatchInfo* info, uint32_t instance, const void* pcmInput, size_t pcmInputSize, void* bitstreamOutput, size_t bitstreamOutputSize, void* result) {
    return sceAjmBatchJobEncode(info, instance, pcmInput, pcmInputSize, bitstreamOutput, bitstreamOutputSize, result);
}

int APS5_VABI sceAjmBatchJobGetCodecInfo_nid_disambig1(AjmBatchInfo* info, uint32_t instance, void* result, size_t resultSize) {
    return sceAjmBatchJobGetCodecInfo(info, instance, result, resultSize);
}

int APS5_VABI sceAjmBatchJobGetGaplessDecode_nid_disambig1(AjmBatchInfo* info, uint32_t instance, void* result) {
    return sceAjmBatchJobGetGaplessDecode(info, instance, result);
}

int APS5_VABI sceAjmBatchJobGetInfo_nid_disambig1(AjmBatchInfo* info, uint32_t instance, void* result) {
    return sceAjmBatchJobGetInfo(info, instance, result);
}

int APS5_VABI sceAjmBatchJobGetResampleInfo_nid_disambig1(AjmBatchInfo* info, uint32_t instance, void* result) {
    return sceAjmBatchJobGetResampleInfo(info, instance, result);
}

int APS5_VABI sceAjmBatchJobGetStatistics_nid_disambig1(AjmBatchInfo* info, float interval, void* result) {
    return sceAjmBatchJobGetStatistics(info, interval, result);
}

int APS5_VABI sceAjmBatchJobInitialize_nid_disambig1(AjmBatchInfo* info, uint32_t instance, const void* codecParameters, size_t codecParametersSize, void* result) {
    return sceAjmBatchJobInitialize(info, instance, codecParameters, codecParametersSize, result);
}

void* APS5_VABI sceAjmBatchJobInlineBuffer_nid_disambig2(void* position, const void* dataInput, size_t dataInputSize, const void** batchAddress) {
    return sceAjmBatchJobInlineBuffer(position, dataInput, dataInputSize, batchAddress);
}

int APS5_VABI sceAjmBatchJobRun_nid_disambig1(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const void* dataInput, size_t dataInputSize, void* dataOutput, size_t dataOutputSize, void* sidebandOutput, size_t sidebandOutputSize) {
    return sceAjmBatchJobRun(info, instance, flags, dataInput, dataInputSize, dataOutput, dataOutputSize, sidebandOutput, sidebandOutputSize);
}

void* APS5_VABI sceAjmBatchJobRunBufferRa_nid_disambig2(void* position, uint32_t instance, uint64_t flags, const void* dataInput, size_t dataInputSize, void* dataOutput, size_t dataOutputSize, void* sidebandOutput, size_t sidebandOutputSize, void* returnAddress) {
    return sceAjmBatchJobRunBufferRa(position, instance, flags, dataInput, dataInputSize, dataOutput, dataOutputSize, sidebandOutput, sidebandOutputSize, returnAddress);
}

int APS5_VABI sceAjmBatchJobRunSplit_nid_disambig1(AjmBatchInfo* info, uint32_t instance, uint64_t flags, const AjmBuffer* inputBuffers, size_t inputBuffersNum, const AjmBuffer* outputBuffers, size_t outputBuffersNum, void* sidebandOutput, size_t sidebandOutputSize) {
    return sceAjmBatchJobRunSplit(info, instance, flags, inputBuffers, inputBuffersNum, outputBuffers, outputBuffersNum, sidebandOutput, sidebandOutputSize);
}

void* APS5_VABI sceAjmBatchJobRunSplitBufferRa_nid_disambig2(void* position, uint32_t instance, uint64_t flags, const AjmBuffer* inputBuffers, size_t inputBuffersNum, const AjmBuffer* outputBuffers, size_t outputBuffersNum, void* sidebandOutput, size_t sidebandOutputSize, void* returnAddress) {
    return sceAjmBatchJobRunSplitBufferRa(position, instance, flags, inputBuffers, inputBuffersNum, outputBuffers, outputBuffersNum, sidebandOutput, sidebandOutputSize, returnAddress);
}

int APS5_VABI sceAjmBatchJobSetGaplessDecode_nid_disambig1(AjmBatchInfo* info, uint32_t instance, const void* gaplessDecode, int reset, void* result) {
    return sceAjmBatchJobSetGaplessDecode(info, instance, gaplessDecode, reset, result);
}

int APS5_VABI sceAjmBatchJobSetResampleParameters_nid_disambig1(AjmBatchInfo* info, uint32_t instance, float ratio, uint32_t flags, void* result) {
    return sceAjmBatchJobSetResampleParameters(info, instance, ratio, flags, result);
}

int APS5_VABI sceAjmBatchJobSetResampleParametersEx_nid_disambig1(AjmBatchInfo* info, uint32_t instance, float ratioStart, float ratioChangePerSample, uint32_t flags, void* result) {
    return sceAjmBatchJobSetResampleParametersEx(info, instance, ratioStart, ratioChangePerSample, flags, result);
}

int APS5_VABI sceAjmBatchStart_nid_disambig1(uint32_t context, const AjmBatchInfo* info, int priority, AjmBatchError* error, uint32_t* batch) {
    return sceAjmBatchStart(context, info, priority, error, batch);
}

int APS5_VABI sceAjmBatchStartBuffer_nid_disambig1(uint32_t context, const uint8_t* batch, uint32_t batchSize, int priority, AjmBatchError* error, uint32_t* batchId) {
    return sceAjmBatchStartBuffer(context, batch, batchSize, priority, error, batchId);
}

int APS5_VABI sceAjmBatchWait_nid_disambig1(uint32_t context, uint32_t batch, uint32_t timeout, AjmBatchError* error) {
    return sceAjmBatchWait(context, batch, timeout, error);
}

int APS5_VABI sceAjmDecAt9ParseConfigData_nid_disambig1(const void* configData, AjmDecAt9ConfigDataInfo* configInfo) {
    return sceAjmDecAt9ParseConfigData(configData, configInfo);
}

int APS5_VABI sceAjmDecMp3ParseFrame_nid_disambig1(const uint8_t* stream, uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame) {
    return sceAjmDecMp3ParseFrame(stream, streamSize, parseOfl, frame);
}

int APS5_VABI sceAjmFinalize_nid_disambig1(uint32_t context) {
    return sceAjmFinalize(context);
}

int APS5_VABI sceAjmInitialize_nid_disambig1(int64_t reserved, uint32_t* context) {
    return sceAjmInitialize(reserved, context);
}

int APS5_VABI sceAjmInstanceAvailable_nid_disambig1() {
    return sceAjmInstanceAvailable();
}

uint32_t APS5_VABI sceAjmInstanceCodecType_nid_disambig1(uint32_t instanceId) {
    return sceAjmInstanceCodecType(instanceId);
}

int APS5_VABI sceAjmInstanceCreate_nid_disambig1(uint32_t context, uint32_t codec, uint64_t flags, uint32_t* instance) {
    return sceAjmInstanceCreate(context, codec, flags, instance);
}

int APS5_VABI sceAjmInstanceDestroy_nid_disambig1(uint32_t context, uint32_t instance) {
    return sceAjmInstanceDestroy(context, instance);
}

int APS5_VABI sceAjmInstanceExtend_nid_disambig1(uint32_t context, uint32_t codec, uint64_t flags, uint32_t instance) {
    return sceAjmInstanceExtend(context, codec, flags, instance);
}

int APS5_VABI sceAjmInstanceSwitch_nid_disambig1(uint32_t context, uint32_t codec, uint64_t flags, uint32_t instance) {
    return sceAjmInstanceSwitch(context, codec, flags, instance);
}

int APS5_VABI sceAjmMemoryRegister_nid_disambig1(uint32_t context, void* ptr, size_t pages) {
    return sceAjmMemoryRegister(context, ptr, pages);
}

int APS5_VABI sceAjmMemoryUnregister_nid_disambig1(uint32_t context, void* ptr) {
    return sceAjmMemoryUnregister(context, ptr);
}

int APS5_VABI sceAjmModuleRegister_nid_disambig1(uint32_t context, uint32_t codec, int64_t reserved) {
    return sceAjmModuleRegister(context, codec, reserved);
}

int APS5_VABI sceAjmModuleUnregister_nid_disambig1(uint32_t context, uint32_t codec) {
    return sceAjmModuleUnregister(context, codec);
}

const char* APS5_VABI sceAjmStrError_nid_disambig1(int error) {
    return sceAjmStrError(error);
}

}
