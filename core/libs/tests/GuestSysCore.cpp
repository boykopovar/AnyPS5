#include "prx/libSceSysCore/libSceSysCore.hpp"

#include <cstddef>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int invalidArgument = static_cast<int>(0x80AA0001u);
    constexpr int notInitialized = static_cast<int>(0x80AA0004u);
    constexpr int shellCoreNotInitialized = static_cast<int>(0x80AA000Bu);
    for (int call = 0; call < 2; ++call) {
        ApplicationInfo info{};
        Require(sceApplicationInitializeForShellCore(nullptr) == invalidArgument);
        Require(_sceApplicationGetAppId(0, nullptr) == invalidArgument);
        Require(sceApplicationIsCrashReportBeingProcessed());
        Require(sceApplicationGetAppInfoByAppId(0, &info) == notInitialized);
        Require(sceApplicationAddProcess2() == notInitialized);
        Require(sceApplicationBeginSclkChange() == notInitialized);
        Require(sceApplicationBlockingKill2() == notInitialized);
        Require(sceApplicationBlockingKill3() == notInitialized);
        Require(sceApplicationContinue() == notInitialized);
        Require(sceApplicationCrashSyscore() == notInitialized);
        Require(sceApplicationCrashSystemProcess() == notInitialized);
        Require(sceApplicationEndSclkChange() == notInitialized);
        Require(sceApplicationExitSpawn() == shellCoreNotInitialized);
        Require(sceApplicationExitSpawn2() == shellCoreNotInitialized);
        Require(sceApplicationFinalize() == notInitialized);
        Require(sceApplicationGetAppPackageInfoForCoredump() == notInitialized);
        Require(sceApplicationGetCompiledSdkVersionByAppId() == notInitialized);
        Require(sceApplicationGetCoredumpDirAndFileName() == notInitialized);
        Require(sceApplicationGetCoredumpState() == notInitialized);
        Require(sceApplicationGetCpuUsage() == notInitialized);
        Require(sceApplicationGetCpuUsage2() == notInitialized);
        Require(sceApplicationGetDbgExecutablePath() == notInitialized);
        Require(sceApplicationGetMode() == notInitialized);
        Require(sceApplicationGetProcs() == notInitialized);
        Require(sceApplicationGetShellCoreAppId() == notInitialized);
        Require(sceApplicationGetStateForDebugger() == notInitialized);
        Require(sceApplicationIsPrimaryProcess() == notInitialized);
        Require(sceApplicationIsResumable() == notInitialized);
        Require(sceApplicationIsSuspendable() == notInitialized);
        Require(sceApplicationKickCoredump() == notInitialized);
        Require(sceApplicationKickCoredump2() == notInitialized);
        Require(sceApplicationKill() == notInitialized);
        Require(sceApplicationLocalProcessKill() == notInitialized);
        Require(sceApplicationLocalProcessKill2() == notInitialized);
        Require(sceApplicationLocalProcessResume() == notInitialized);
        Require(sceApplicationLocalProcessSuspend() == notInitialized);
        Require(sceApplicationNotifyCoredumpRequestEnd() == notInitialized);
        Require(sceApplicationNotifyCoredumpRequestProgress() == notInitialized);
        Require(sceApplicationNotifyVshMainOnStandby() == notInitialized);
        Require(sceApplicationNotifyVshReady() == notInitialized);
        Require(sceApplicationOnAllResourceReleased() == notInitialized);
        Require(sceApplicationRaiseException() == notInitialized);
        Require(sceApplicationRaiseExceptionToLocalPid() == notInitialized);
        Require(sceApplicationRaiseExceptionToPid() == notInitialized);
        Require(sceApplicationRequestToChangeRenderingMode() == shellCoreNotInitialized);
        Require(sceApplicationRestore() == notInitialized);
        Require(sceApplicationResume() == notInitialized);
        Require(sceApplicationSave() == notInitialized);
        Require(sceApplicationSendDebugSpawnResult2() == notInitialized);
        Require(sceApplicationSendResultOfDebuggerKillRequest() == notInitialized);
        Require(sceApplicationSendResultOfDebuggerResumeRequest() == notInitialized);
        Require(sceApplicationSendResultOfDebuggerSuspendRequest() == notInitialized);
        Require(sceApplicationSendResultOfDebuggerTitleIdLaunchRequest() == notInitialized);
        Require(sceApplicationSetApplicationFocus() == notInitialized);
        Require(sceApplicationSetControllerFocus() == notInitialized);
        Require(sceApplicationSetControllerFocusPermissionToSubProcess() == notInitialized);
        Require(sceApplicationSetMemoryPstate() == notInitialized);
        Require(sceApplicationSignalShellCoreHeartBeat() == notInitialized);
        Require(sceApplicationSpawn2() == notInitialized);
        Require(sceApplicationSuspend() == notInitialized);
        Require(sceApplicationSuspendSystemForDebugger() == notInitialized);
        Require(sceApplicationSwitchToBaseMode() == notInitialized);
        Require(sceApplicationSwitchToNeoMode() == notInitialized);
        Require(sceApplicationSwitchToNeoMode2() == notInitialized);
        Require(sceApplicationSystemReboot() == notInitialized);
        Require(sceApplicationSystemShutdown2() == notInitialized);
        Require(sceApplicationSystemSuspend() == notInitialized);
        Require(sceApplicationSystemSuspend2() == notInitialized);
        Require(sceApplictionGetStateForDebugger() == notInitialized);
        Require(sceSysCoreGetEventCount() == notInitialized);
        Require(sceSysCoreReceiveEvent() == notInitialized);
        Require(sceApplicationExitSpawn3() == shellCoreNotInitialized);
        Require(sceApplicationKickCoredump3() == notInitialized);
        Require(sceApplicationNotifyUnmountForExitSpawn() == shellCoreNotInitialized);
        Require(sceApplicationSendResultOfDebuggerRestoreRequest() == notInitialized);
        Require(sceApplicationSendResultOfDebuggerSaveRequest() == notInitialized);
        Require(sceApplicationSwitchToPprNativeMode() == notInitialized);
        Require(sceApplicationSwitchToPprNativeMode2() == notInitialized);
        Require(libSceSysCoreUnknown_Q5nkKEHtW18_nid_no_patch() == notInitialized);
        Require(libSceSysCoreUnknown_V7awqOXNG30_nid_no_patch() == notInitialized);
        Require(libSceSysCoreUnknown_lGA8mxzKIhk_nid_no_patch() == notInitialized);
        Require(libSceSysCoreUnknown_piCjhbSYauo_nid_no_patch() == notInitialized);
        for (const std::byte value : info.data) Require(value == std::byte{0});
    }
    return 0;
}
