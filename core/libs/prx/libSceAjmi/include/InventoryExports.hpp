#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAjmiBatchCancel();
int APS5_VABI sceAjmiBatchWait();
int APS5_VABI sceAjmiModuleUnregister();
int APS5_VABI sceAjmiBatchStartBuffer();
int APS5_VABI sceAjmiFinalize();
const char* APS5_VABI sceAjmiStrError(int error);
int APS5_VABI sceAjmiBatchJobControlBufferRa();
int APS5_VABI sceAjmiInstanceSwitch();
int APS5_VABI sceAjmiDecAt9ParseConfigData(const void* configData, AjmDecAt9ConfigDataInfo* configInfo);
int APS5_VABI sceAjmiInitialize();
int APS5_VABI sceAjmiBatchErrorDump();
int APS5_VABI sceAjmiInstanceCodecType();
int APS5_VABI sceAjmiMemoryUnregister();
int APS5_VABI sceAjmiInstanceDestroy();
int APS5_VABI sceAjmiDecMp3ParseFrame(const std::uint8_t* stream, std::uint32_t streamSize, int parseOfl, AjmDecMp3ParseFrame* frame);
int APS5_VABI sceAjmiMemoryRegister();
int APS5_VABI sceAjmiModuleRegister();
int APS5_VABI sceAjmiInstanceCreate();
int APS5_VABI sceAjmiInstanceExtend();
int APS5_VABI sceAjmiBatchJobRunSplitBufferRa();

}
