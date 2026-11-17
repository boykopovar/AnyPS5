#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceGpuTraceParametersSetDuration(void);
int APS5_VABI sceGpuTraceParametersInit(void);
int APS5_VABI sceGpuTraceParametersSetMemorySize(void);
int APS5_VABI sceGpuTraceParametersSetGroup(void);
int APS5_VABI sceGpuTraceStart(void);
int APS5_VABI sceGpuTraceStop(void);
int APS5_VABI sceGpuTraceMonitorTrigger(void);
int APS5_VABI sceGpuTraceCancel(void);

}
