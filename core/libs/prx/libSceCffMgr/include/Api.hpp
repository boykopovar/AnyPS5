#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceConsoleFeatureFlagManagerIsOn(void);
int APS5_VABI sceConsoleFeatureFlagManagerSetOn(void);
int APS5_VABI sceCffMgrUnknown00(void);
int APS5_VABI sceConsoleFeatureFlagManagerIsWaitingReboot(void);
int APS5_VABI sceConsoleFeatureFlagManagerGetVersion(void);
int APS5_VABI sceCffMgrUnknown01(void);
int APS5_VABI sceConsoleFeatureFlagManagerEventOpen(void);
int APS5_VABI sceConsoleFeatureFlagManagerEventClose(void);
int APS5_VABI sceCffMgrUnknown02(void);
int APS5_VABI sceCffMgrUnknown03(void);
int APS5_VABI sceCffMgrUnknown04(void);
int APS5_VABI sceCffMgrUnknown05(void);
int APS5_VABI sceCffMgrUnknown06(void);
int APS5_VABI sceConsoleFeatureFlagManagerDebugInit(void);

}
