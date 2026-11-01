#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpSpeechToTextCreateContext();
int APS5_VABI sceNpSpeechToTextDestroyContext();
int APS5_VABI sceNpSpeechToTextGetEvent();
int APS5_VABI sceNpSpeechToTextInitialize();
int APS5_VABI sceNpSpeechToTextStart();
int APS5_VABI sceNpSpeechToTextStop();
int APS5_VABI sceNpSpeechToTextTerminate();

}
