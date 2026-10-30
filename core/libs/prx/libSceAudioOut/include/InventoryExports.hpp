#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudioOut2GetSystemDebugState();
int APS5_VABI sceAudioOut2LoContextCreateEx();
int APS5_VABI sceAudioOut2LoContextGetAttributes();
int APS5_VABI sceAudioOut2LoContextQueryMemoryEx();
int APS5_VABI sceAudioOut2LoPortCreateEx();
int APS5_VABI sceAudioOut2LoPortGetAttributes();
int APS5_VABI sceAudioOut2LoPortSetAttributesEx();
int APS5_VABI sceAudioOut2PortSetAttributesEx();
int APS5_VABI sceAudioOut2SetHrtfIdForCronos();
int APS5_VABI sceAudioOut2SetSpeakerInfo();
int APS5_VABI sceAudioOut2SetTvCorrectionInfo();

}
