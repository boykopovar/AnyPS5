#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudiodecClearContextEx(void);
int APS5_VABI sceAudiodecCreateDecoderEx(void);
int APS5_VABI sceAudiodecDecode2(void);
int APS5_VABI sceAudiodecDecode2Ex(void);
int APS5_VABI sceAudiodecDecode2WithPriority(void);
int APS5_VABI sceAudiodecDecode2WithPriorityEx(void);
int APS5_VABI sceAudiodecDecodeEx(void);
int APS5_VABI sceAudiodecDecodeWithPriority(void);
int APS5_VABI sceAudiodecDecodeWithPriorityEx(void);
int APS5_VABI sceAudiodecDeleteDecoderEx(void);
int APS5_VABI sceAudiodecGetAjmContext(const void* context, std::uint32_t* ajmiContext, std::uint32_t* ajmContext);
int APS5_VABI sceAudiodecGetContext(void);
int APS5_VABI sceAudiodecInitialize(void);
int APS5_VABI sceAudiodecRegisterCodec(void);
int APS5_VABI sceAudiodecTerminate(void);
int APS5_VABI sceAudiodecUnregisterCodec(void);

}
