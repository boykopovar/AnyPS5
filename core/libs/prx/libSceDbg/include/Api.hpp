#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceDbgUserChannelPrint(void);
int APS5_VABI sceDbgAmprAprGetNumberOfErrors(void);
int APS5_VABI sceDbgMapperVirtualQuery(void);
int APS5_VABI sceDbgUserChannelPrintf(void);
int APS5_VABI sceDbgAssertionHandler(void);
int APS5_VABI sceDbgGetSystemSwVersion(void);
int APS5_VABI sceDbgSetBreakOnWarningState(void);
int APS5_VABI sceDbgSetBreakOnErrorState(void);
int APS5_VABI sceDbgDeleteExternalTriggerEvent(void);
int APS5_VABI sceDbgAmprAmmGetNumberOfErrors(void);
int APS5_VABI sceDbgGetModuleInfo(void);
int APS5_VABI sceDbgBacktraceSelf(void);
int APS5_VABI sceDbgAmprAmmGetErrors(void);
int APS5_VABI sceDbgGetDebugSuspendCount(void);
int APS5_VABI sceDbgUserChannelVPrintf(void);
int APS5_VABI sceDbgSetMinimumLogLevel(void);
int APS5_VABI sceDbgGetModuleList(void);
int APS5_VABI sceDbgClearHardwareBreakPoint(void);
int APS5_VABI sceDbgAddExternalTriggerEvent(void);
int APS5_VABI sceDbgLoggingHandler(void);
int APS5_VABI sceDbgEnableExtraHeapDebugInfo(void);
int APS5_VABI sceDbgAmprAprGetErrors(void);
int APS5_VABI sceDbgVLoggingHandler(void);
int APS5_VABI sceDbgGetReleaseCheckMode(void);
int APS5_VABI sceDbgSetHardwareBreakPoint(void);
int APS5_VABI sceDbgGetExecutablePath(void);
int APS5_VABI sceDbgGetCpuGpuFrequencySetting(void);
int APS5_VABI sceDbgRequireSystemSwVersion(void);
int APS5_VABI sceDbgIsDebuggerAttached(void);

}
