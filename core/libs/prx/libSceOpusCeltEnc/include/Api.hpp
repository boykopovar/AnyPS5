#pragma once

#include "SceTypes.hpp"

struct OpusCeltEncoder;

extern "C" {

int APS5_VABI sceOpusCeltEncCreate(OpusCeltEncoder* encoder, std::int32_t sampleRate, std::int32_t channels, std::int32_t application);
int APS5_VABI sceOpusCeltEncCreateEx(const std::uint32_t* contextId, OpusCeltEncoder* encoder, std::int32_t sampleRate, std::int32_t channels, std::int32_t application);
int APS5_VABI sceOpusCeltEncCtl(OpusCeltEncoder* encoder, std::int32_t request, ...);
int APS5_VABI sceOpusCeltEncDestroy(OpusCeltEncoder* encoder);
int APS5_VABI sceOpusCeltEncEncode(OpusCeltEncoder* encoder, const std::int16_t* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes);
int APS5_VABI sceOpusCeltEncEncodeFloat(OpusCeltEncoder* encoder, const float* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes);
int APS5_VABI sceOpusCeltEncEncodeFloatWithPriority(OpusCeltEncoder* encoder, const float* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes, std::uint32_t priority);
int APS5_VABI sceOpusCeltEncEncodeWithPriority(OpusCeltEncoder* encoder, const std::int16_t* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes, std::uint32_t priority);
int APS5_VABI sceOpusCeltEncGetAjmContext();
int APS5_VABI sceOpusCeltEncGetSize(std::int32_t channels);
int APS5_VABI sceOpusCeltEncInitialize(std::uint32_t* contextId);
int APS5_VABI sceOpusCeltEncTerminate(const std::uint32_t* contextId);
int APS5_VABI sceOpusCeltMultistreamEncCreate();
int APS5_VABI sceOpusCeltMultistreamEncCreateEx();
int APS5_VABI sceOpusCeltMultistreamEncCtl();

}
