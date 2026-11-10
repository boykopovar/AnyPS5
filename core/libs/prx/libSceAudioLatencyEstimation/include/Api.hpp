#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudioLatencyEstimationAddData();
int APS5_VABI sceAudioLatencyEstimationGetDelayTime();
int APS5_VABI sceAudioLatencyEstimationInitialize();
int APS5_VABI sceAudioLatencyEstimationRelease();

}
