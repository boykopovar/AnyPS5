#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceOpusCeltDecInitialize(std::uint32_t* context);
int APS5_VABI sceOpusCeltDecTerminate(const std::uint32_t* context);
int APS5_VABI sceOpusCeltDecGetSize(int channels);
int APS5_VABI sceOpusCeltDecCreateEx(const std::uint32_t* context, void* state, int sampleRate, int channels);
int APS5_VABI sceOpusCeltDecDecode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity);
int APS5_VABI sceOpusCeltDecDestroy(void* state);
int APS5_VABI sceOpusCeltDecCreate();
int APS5_VABI sceOpusCeltDecCtl();
int APS5_VABI sceOpusCeltDecDecodeFloat(void* state, const std::uint8_t* data, int bytes, float* pcm, int capacity);
int APS5_VABI sceOpusCeltDecDecodeFloatWithPriority(void* state, const std::uint8_t* data, int bytes, float* pcm, int capacity, int priority);
int APS5_VABI sceOpusCeltDecDecodeWithPriority(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity, int priority);

}
