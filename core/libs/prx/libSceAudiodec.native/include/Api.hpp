#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceAudiodecClearContextEx(void* context, std::uint32_t handle);
int APS5_VABI sceAudiodecCreateDecoderEx(void* context, AudiodecCtrl* ctrl, std::uint32_t codecType);
int APS5_VABI sceAudiodecDecode2(std::uint32_t handle, AudiodecCtrl* ctrl);
int APS5_VABI sceAudiodecDecode2Ex(void* context, std::uint32_t handle, AudiodecCtrl* ctrl);
int APS5_VABI sceAudiodecDecode2WithPriority(std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority);
int APS5_VABI sceAudiodecDecode2WithPriorityEx(void* context, std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority);
int APS5_VABI sceAudiodecDecodeEx(void* context, std::uint32_t handle, AudiodecCtrl* ctrl);
int APS5_VABI sceAudiodecDecodeWithPriority(std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority);
int APS5_VABI sceAudiodecDecodeWithPriorityEx(void* context, std::uint32_t handle, AudiodecCtrl* ctrl, std::int32_t priority);
int APS5_VABI sceAudiodecDeleteDecoderEx(void* context, std::uint32_t handle);
int APS5_VABI sceAudiodecGetAjmContext(const void* context, std::uint32_t* ajmiContext, std::uint32_t* ajmContext);
int APS5_VABI sceAudiodecGetContext(void* out);
int APS5_VABI sceAudiodecInitialize(void* context);
int APS5_VABI sceAudiodecRegisterCodec(void* context, std::uint32_t codecType);
int APS5_VABI sceAudiodecTerminate(void* context);
int APS5_VABI sceAudiodecUnregisterCodec(const void* context, std::uint32_t codecType);

}
