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

int APS5_VABI libSceVideoOutUnknown__plus_ZuoGRI_plus_U_minus_g_nid_no_patch();
int APS5_VABI libSceVideoOutUnknown_96hq3WwT3DE_nid_no_patch();
int APS5_VABI libSceVideoOutUnknown_B8siTU85n8M_nid_no_patch();
int APS5_VABI libSceVideoOutUnknown_IkyDS23qu4g_nid_no_patch();
int APS5_VABI libSceVideoOutUnknown_gQwfLPPIZQ8_nid_no_patch();
int APS5_VABI libSceVideoOutUnknown_rJmjlPlreu4_nid_no_patch();

int APS5_VABI sceVideoOutGetFlipStatusNative();
int APS5_VABI sceVideoOutSysLatencyMeasureGetLatestLatency();

}
