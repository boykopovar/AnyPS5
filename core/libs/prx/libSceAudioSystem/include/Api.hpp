#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudioSystemSystemResume();
int APS5_VABI sceAudioSystemSystemSuspend();
int APS5_VABI sceAudioSystemInit();
int APS5_VABI sceAudioSystemProcessAdd();
int APS5_VABI sceAudioSystemProcessRemove();
int APS5_VABI sceAudioSystemProcessResume();
int APS5_VABI sceAudioSystemProcessSuspend();
int APS5_VABI sceAudioSystemTerminate();

}
