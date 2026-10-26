#pragma once

#include "SceTypes.hpp"

struct ApplicationInfo {
    std::byte data[152];
};

extern "C" {

int APS5_VABI sceApplicationGetAppInfoByAppId(std::uint32_t applicationId, ApplicationInfo* info);

int APS5_VABI _sceApplicationGetAppId();
int APS5_VABI sceApplicationAddProcess2();
int APS5_VABI sceApplicationBeginSclkChange();
int APS5_VABI sceApplicationBlockingKill2();
int APS5_VABI sceApplicationBlockingKill3();
int APS5_VABI sceApplicationContinue();
int APS5_VABI sceApplicationCrashSyscore();
int APS5_VABI sceApplicationCrashSystemProcess();
int APS5_VABI sceApplicationEndSclkChange();
int APS5_VABI sceApplicationExitSpawn();
int APS5_VABI sceApplicationExitSpawn2();
int APS5_VABI sceApplicationFinalize();
int APS5_VABI sceApplicationGetAppPackageInfoForCoredump();
int APS5_VABI sceApplicationGetCompiledSdkVersionByAppId();
int APS5_VABI sceApplicationGetCoredumpDirAndFileName();
int APS5_VABI sceApplicationGetCoredumpState();
int APS5_VABI sceApplicationGetCpuUsage();
int APS5_VABI sceApplicationGetCpuUsage2();
int APS5_VABI sceApplicationGetDbgExecutablePath();
int APS5_VABI sceApplicationGetMode();
int APS5_VABI sceApplicationGetProcs();
int APS5_VABI sceApplicationGetShellCoreAppId();
int APS5_VABI sceApplicationGetStateForDebugger();
int APS5_VABI sceApplicationInitialize();
int APS5_VABI sceApplicationInitializeForShellCore();
int APS5_VABI sceApplicationIsCrashReportBeingProcessed();
int APS5_VABI sceApplicationIsPrimaryProcess();
int APS5_VABI sceApplicationIsResumable();
int APS5_VABI sceApplicationIsSuspendable();
int APS5_VABI sceApplicationKickCoredump();
int APS5_VABI sceApplicationKickCoredump2();
int APS5_VABI sceApplicationKill();
int APS5_VABI sceApplicationLocalProcessKill();
int APS5_VABI sceApplicationLocalProcessKill2();
int APS5_VABI sceApplicationLocalProcessResume();
int APS5_VABI sceApplicationLocalProcessSuspend();
int APS5_VABI sceApplicationNotifyCoredumpRequestEnd();
int APS5_VABI sceApplicationNotifyCoredumpRequestProgress();
int APS5_VABI sceApplicationNotifyVshMainOnStandby();
int APS5_VABI sceApplicationNotifyVshReady();
int APS5_VABI sceApplicationOnAllResourceReleased();
int APS5_VABI sceApplicationRaiseException();
int APS5_VABI sceApplicationRaiseExceptionToLocalPid();
int APS5_VABI sceApplicationRaiseExceptionToPid();
int APS5_VABI sceApplicationRequestToChangeRenderingMode();
int APS5_VABI sceApplicationRestore();
int APS5_VABI sceApplicationResume();
int APS5_VABI sceApplicationSave();
int APS5_VABI sceApplicationSendDebugSpawnResult2();
int APS5_VABI sceApplicationSendResultOfDebuggerKillRequest();
int APS5_VABI sceApplicationSendResultOfDebuggerResumeRequest();
int APS5_VABI sceApplicationSendResultOfDebuggerSuspendRequest();
int APS5_VABI sceApplicationSendResultOfDebuggerTitleIdLaunchRequest();
int APS5_VABI sceApplicationSetApplicationFocus();
int APS5_VABI sceApplicationSetControllerFocus();
int APS5_VABI sceApplicationSetControllerFocusPermissionToSubProcess();
int APS5_VABI sceApplicationSetMemoryPstate();
int APS5_VABI sceApplicationSignalShellCoreHeartBeat();
int APS5_VABI sceApplicationSpawn2();
int APS5_VABI sceApplicationSuspend();
int APS5_VABI sceApplicationSuspendSystemForDebugger();
int APS5_VABI sceApplicationSwitchToBaseMode();
int APS5_VABI sceApplicationSwitchToNeoMode();
int APS5_VABI sceApplicationSwitchToNeoMode2();
int APS5_VABI sceApplicationSystemReboot();
int APS5_VABI sceApplicationSystemShutdown2();
int APS5_VABI sceApplicationSystemSuspend();
int APS5_VABI sceApplicationSystemSuspend2();
int APS5_VABI sceApplictionGetStateForDebugger();
int APS5_VABI sceSysCoreGetEventCount();
int APS5_VABI sceSysCoreReceiveEvent();

}
