#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceRazorCpuStartCapture(void);
int APS5_VABI sceRazorCpuStopCapture(void);
int APS5_VABI sceRazorCpuSetLiveServerThreadSettings(void);

}
