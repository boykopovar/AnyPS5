#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceOpusDecInitialize(std::uint32_t* context);
int APS5_VABI sceOpusDecTerminate(const std::uint32_t* context);
int APS5_VABI sceOpusDecGetSize(int channels);
int APS5_VABI sceOpusDecCreateEx(const std::uint32_t* context, void* state, int sampleRate, int channels);
int APS5_VABI sceOpusDecDecode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity);
int APS5_VABI sceOpusDecDestroy(void* state);
int APS5_VABI sceOpusDecCancel();
int APS5_VABI sceOpusDecCreate();
int APS5_VABI sceOpusDecCtl(void* state, int request, ...);
int APS5_VABI sceOpusDecDecodeFloat();
int APS5_VABI sceOpusDecDecodeFloatWithPriority();
int APS5_VABI sceOpusDecDecodeFloatWithPriorityAndTimeout();
int APS5_VABI sceOpusDecDecodeWithPriority();
int APS5_VABI sceOpusDecDecodeWithPriorityAndTimeout();

}
