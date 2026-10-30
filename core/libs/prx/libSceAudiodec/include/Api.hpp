#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudiodecUnregisterCodec();
int APS5_VABI sceAudiodecGetAjmContext();

int32_t APS5_VABI sceAudiodecInitLibrary(uint32_t codecType);
int32_t APS5_VABI sceAudiodecTermLibrary(uint32_t codecType);
int32_t APS5_VABI sceAudiodecCreateDecoder(AudiodecCtrl* ctrl, uint32_t codecType);
int32_t APS5_VABI sceAudiodecDeleteDecoder(int32_t handle);
int32_t APS5_VABI sceAudiodecClearContext(int32_t handle);
int32_t APS5_VABI sceAudiodecDecode(int32_t handle, AudiodecCtrl* ctrl);

}
