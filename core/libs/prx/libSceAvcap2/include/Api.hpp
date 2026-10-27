#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAvcap2Initialize(void);
int APS5_VABI sceAvcap2Terminate(void);
int APS5_VABI sceAvcap2OpenAudio(void);
int APS5_VABI sceAvcap2Close(void);
int APS5_VABI sceAvcap2Start(void);
int APS5_VABI sceAvcap2Stop(void);
int APS5_VABI sceAvcap2ReadAudio(void);

}
