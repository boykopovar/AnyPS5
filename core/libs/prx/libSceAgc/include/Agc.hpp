#pragma once

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcSuspendPointAndCheckStatus(void);
std::uint32_t* APS5_VABI sceAgcAcbPushMarkerSpan(CommandBuffer* buffer, const char* text, std::uint32_t length, std::uint32_t color);
std::uint32_t* APS5_VABI sceAgcAcbSetMarkerSpan(CommandBuffer* buffer, const char* text, std::uint32_t length, std::uint32_t color);
int APS5_VABI sceAgcAcquireMemSetEngine(std::uint32_t* packet, std::uint8_t engine);
std::uint32_t* APS5_VABI sceAgcDcbPushMarkerSpan(CommandBuffer* buffer, const char* text, std::uint32_t length, std::uint32_t color);
std::uint32_t* APS5_VABI sceAgcDcbSetMarkerSpan(CommandBuffer* buffer, const char* text, std::uint32_t length, std::uint32_t color);
int APS5_VABI sceAgcSetSemaphoreMemory(void* memory, std::uint64_t size);
std::uint32_t* APS5_VABI sceAgcCbMemsetExclusive(CommandBuffer* buffer, void* destination, const void* pattern, std::uint64_t size);

int APS5_VABI sceAgcAcbInitializeDefaultHardwareState();

}
