#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAjmiBatchCancel(std::uint32_t context, std::uint32_t batch);
int APS5_VABI sceAjmiBatchWait(std::uint32_t context, std::uint32_t batch, std::uint32_t timeout, AjmBatchError* error);
int APS5_VABI sceAjmiModuleUnregister();
int APS5_VABI sceAjmiBatchStartBuffer(std::uint32_t context, const std::uint8_t* batch, std::uint32_t batchSize, int priority, AjmBatchError* error, std::uint32_t* batchId);
int APS5_VABI sceAjmiFinalize();
const char* APS5_VABI sceAjmiStrError(int error);
int APS5_VABI sceAjmiInstanceSwitch();
int APS5_VABI sceAjmiDecAt9ParseConfigData(const void* configData, AjmDecAt9ConfigDataInfo* configInfo);
int APS5_VABI sceAjmiInitialize(std::int64_t reserved, std::uint32_t* context);
int APS5_VABI sceAjmiBatchErrorDump();
std::uint32_t APS5_VABI sceAjmiInstanceCodecType(std::uint32_t instanceId);
int APS5_VABI sceAjmiMemoryUnregister();
int APS5_VABI sceAjmiInstanceDestroy(std::uint32_t context, std::uint32_t instance);
int APS5_VABI sceAjmiDecMp3ParseFrame(const std::uint8_t* stream, std::uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame);
int APS5_VABI sceAjmiMemoryRegister();
int APS5_VABI sceAjmiModuleRegister();
int APS5_VABI sceAjmiInstanceCreate(std::uint32_t context, std::uint32_t codec, std::uint64_t flags, std::uint32_t* instance);
int APS5_VABI sceAjmiInstanceExtend();

}
