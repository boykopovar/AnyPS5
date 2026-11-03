#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoOutDebugLatencyMeasureGetLatestLatency();
int APS5_VABI sceVideoOutGetHandleByBus();
int APS5_VABI sceVideoOutPostProcessorAddCallback();
int APS5_VABI sceVideoOutPostProcessorRemoveCallback();
int APS5_VABI sceVideoOutSetContentDynamicRange();
int APS5_VABI sceVideoOutSysCursorSetSize();
int APS5_VABI sceVideoOutSysGetCursorInfo();
int APS5_VABI sceVideoOutSysGetMonitorInfo();
int APS5_VABI sceVideoOutSysGetOutputStatusEx();
int APS5_VABI sceVideoOutSysOpenPostProcessor();
int APS5_VABI sceVideoOutSysSetDrrPeg();
int APS5_VABI sceVideoOutSysSetDrrPegRefreshRate();
int APS5_VABI sceVideoOutSysSetVrrSetting();

}
