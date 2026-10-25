#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAvControlGetPuTypeByMonitorInfo(void);
int APS5_VABI sceAvControlIsVideoOutModeSupportedByMonitorInfoWithOptions(void);
int APS5_VABI sceAvControlSetDitherModeForDiag(void);
int APS5_VABI sceAvControlSetLayoutModeForDiag(void);
int APS5_VABI sceAvControlSetMonitorFakeMode(void);
int APS5_VABI sceAvControlGetCurrentOutputSysMode(void);
int APS5_VABI sceAvControlGetNativeHdmiMonitorInfo(void);
int APS5_VABI sceAvControlGetRawMonitorInfo(void);
int APS5_VABI sceAvControlGetKsvList(void);
int APS5_VABI sceAvControlGetRawEdid(void);
int APS5_VABI sceAvControlRemoveProcessOutputMode(void);
int APS5_VABI sceAvControlUpdateDriverStatus(void);
int APS5_VABI sceAvControlNotifyUmdEvent(void);
int APS5_VABI sceAvControlAddCallbacksForLnc(void);
int APS5_VABI sceAvControlAddCallbacksForResourceArbitrator(void);
int APS5_VABI sceAvControlRemoveCallbacks(void);
int APS5_VABI sceAvControlCheckCallback(void);
int APS5_VABI sceAvControlWaitCallback(void);
int APS5_VABI sceAvControlSetColorMode(void);
int APS5_VABI sceAvControlCheckMonitorInfoCapability(void);
int APS5_VABI sceAvControlCheckVideoOutMode(void);
int APS5_VABI sceAvControlSetCheckoutputMode(void);
int APS5_VABI sceAvControlSetSystemVideoMode(void);
int APS5_VABI sceAvControlDebugAddCallbacks(void);

}
