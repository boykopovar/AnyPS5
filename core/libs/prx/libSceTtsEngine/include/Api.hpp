#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceTtsEngineInitialize(void);
int APS5_VABI sceTtsEngineTerminate(void);
int APS5_VABI sceTtsEngineUnknown00(void);
int APS5_VABI sceTtsEngineUnknown01(void);
int APS5_VABI sceTtsEngineSetText(void);
int APS5_VABI sceTtsEngineGetAudio(void);
int APS5_VABI sceTtsEngineClear(void);
int APS5_VABI sceTtsEngineSetPitch(void);
int APS5_VABI sceTtsEngineSetSpeed(void);
int APS5_VABI sceTtsEngineSetVolume(void);
int APS5_VABI sceTtsEngineSetDepth(void);

}
