#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAudiodecCpuClearContext(void);
int APS5_VABI sceAudiodecCpuDecode(void);
int APS5_VABI sceAudiodecCpuInitDecoder(void);
int APS5_VABI sceAudiodecCpuInternalClearContext(void);
int APS5_VABI sceAudiodecCpuInternalDecode(void);
int APS5_VABI sceAudiodecCpuInternalInitDecoder(void);
int APS5_VABI sceAudiodecCpuInternalQueryMemSize(void);
int APS5_VABI sceAudiodecCpuQueryMemSize(void);

}
