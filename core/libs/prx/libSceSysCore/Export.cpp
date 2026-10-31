#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceSysCore/include/InventoryExports.hpp"
#include "prx/libSceSysCore/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(ApplicationInfo) == 152);

extern "C" {

int APS5_VABI sceApplicationGetAppInfoByAppId(std::uint32_t applicationId, ApplicationInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceApplicationGetAppId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationAddProcess2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationBeginSclkChange() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationBlockingKill2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationBlockingKill3() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationContinue() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationCrashSyscore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationCrashSystemProcess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationEndSclkChange() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationExitSpawn() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationExitSpawn2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetAppPackageInfoForCoredump() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetCompiledSdkVersionByAppId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetCoredumpDirAndFileName() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetCoredumpState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetCpuUsage() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetCpuUsage2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetDbgExecutablePath() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetProcs() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetShellCoreAppId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationGetStateForDebugger() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationInitializeForShellCore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationIsCrashReportBeingProcessed() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationIsPrimaryProcess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationIsResumable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationIsSuspendable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationKickCoredump() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationKickCoredump2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationKill() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationLocalProcessKill() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationLocalProcessKill2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationLocalProcessResume() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationLocalProcessSuspend() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationNotifyCoredumpRequestEnd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationNotifyCoredumpRequestProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationNotifyVshMainOnStandby() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationNotifyVshReady() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationOnAllResourceReleased() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationRaiseException() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationRaiseExceptionToLocalPid() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationRaiseExceptionToPid() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationRequestToChangeRenderingMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationRestore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationResume() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSave() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSendDebugSpawnResult2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSendResultOfDebuggerKillRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSendResultOfDebuggerResumeRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSendResultOfDebuggerSuspendRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSendResultOfDebuggerTitleIdLaunchRequest() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSetApplicationFocus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSetControllerFocus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSetControllerFocusPermissionToSubProcess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSetMemoryPstate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSignalShellCoreHeartBeat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSpawn2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSuspend() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSuspendSystemForDebugger() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSwitchToBaseMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSwitchToNeoMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSwitchToNeoMode2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSystemReboot() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSystemShutdown2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSystemSuspend() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplicationSystemSuspend2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceApplictionGetStateForDebugger() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceSysCoreGetEventCount() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceSysCoreReceiveEvent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceApplicationExitSpawn3() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationKickCoredump3() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationNotifyUnmountForExitSpawn() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationSendResultOfDebuggerRestoreRequest() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationSendResultOfDebuggerSaveRequest() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationSwitchToPprNativeMode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationSwitchToPprNativeMode2() {
    NotImplemented_nid_no_patch(__func__);
}
}

extern "C" {

int APS5_VABI libSceSysCoreUnknown_F2qydFnZWfY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("F2qydFnZWfY", libSceSysCoreUnknown_F2qydFnZWfY_nid_no_patch);

int APS5_VABI libSceSysCoreUnknown_Q5nkKEHtW18_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Q5nkKEHtW18", libSceSysCoreUnknown_Q5nkKEHtW18_nid_no_patch);

int APS5_VABI libSceSysCoreUnknown_V7awqOXNG30_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("V7awqOXNG30", libSceSysCoreUnknown_V7awqOXNG30_nid_no_patch);

int APS5_VABI libSceSysCoreUnknown_lGA8mxzKIhk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lGA8mxzKIhk", libSceSysCoreUnknown_lGA8mxzKIhk_nid_no_patch);

int APS5_VABI libSceSysCoreUnknown_piCjhbSYauo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("piCjhbSYauo", libSceSysCoreUnknown_piCjhbSYauo_nid_no_patch);

}
