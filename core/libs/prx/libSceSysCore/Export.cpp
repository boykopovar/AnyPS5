#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceSysCore/include/InventoryExports.hpp"
#include "prx/libSceSysCore/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(ApplicationInfo) == 152);

namespace {

constexpr int kInvalidArgument = static_cast<int>(0x80AA0001u);
constexpr int kAlreadyInitialized = static_cast<int>(0x80AA0003u);
constexpr int kNotInitialized = static_cast<int>(0x80AA0004u);
constexpr int kShellCoreNotInitialized = static_cast<int>(0x80AA000Bu);

std::atomic<bool> g_initialized{false};
std::atomic<bool> g_shellCoreInitialized{false};

} // namespace

extern "C" {

int APS5_VABI sceApplicationGetAppInfoByAppId(std::uint32_t applicationId, ApplicationInfo* info) {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _sceApplicationGetAppId(std::uint64_t, std::uint32_t* appId) {
    if (appId == nullptr) return kInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationAddProcess2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationBeginSclkChange() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationBlockingKill2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationBlockingKill3() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationContinue() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationCrashSyscore() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationCrashSystemProcess() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationEndSclkChange() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationExitSpawn() {
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationExitSpawn2() {
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationFinalize() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetAppPackageInfoForCoredump() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetCompiledSdkVersionByAppId() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetCoredumpDirAndFileName() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetCoredumpState() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetCpuUsage() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetCpuUsage2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetDbgExecutablePath() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetMode() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetProcs() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetShellCoreAppId() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationGetStateForDebugger() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationInitialize() {
    if (g_initialized.load(std::memory_order_acquire)) return kAlreadyInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationInitializeForShellCore(const void* parameter) {
    if (parameter == nullptr) return kInvalidArgument;
    if (g_initialized.load(std::memory_order_acquire)) return kAlreadyInitialized;
    NotImplemented_nid_no_patch(__func__);
}
bool APS5_VABI sceApplicationIsCrashReportBeingProcessed() {
    if (!g_initialized.load(std::memory_order_acquire) || !g_shellCoreInitialized.load(std::memory_order_acquire)) return true;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationIsPrimaryProcess() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationIsResumable() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationIsSuspendable() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationKickCoredump() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationKickCoredump2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationKill() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationLocalProcessKill() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationLocalProcessKill2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationLocalProcessResume() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationLocalProcessSuspend() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationNotifyCoredumpRequestEnd() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationNotifyCoredumpRequestProgress() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationNotifyVshMainOnStandby() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationNotifyVshReady() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationOnAllResourceReleased() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationRaiseException() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationRaiseExceptionToLocalPid() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationRaiseExceptionToPid() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationRequestToChangeRenderingMode() {
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationRestore() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationResume() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSave() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSendDebugSpawnResult2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSendResultOfDebuggerKillRequest() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSendResultOfDebuggerResumeRequest() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSendResultOfDebuggerSuspendRequest() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSendResultOfDebuggerTitleIdLaunchRequest() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSetApplicationFocus() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSetControllerFocus() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSetControllerFocusPermissionToSubProcess() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSetMemoryPstate() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSignalShellCoreHeartBeat() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSpawn2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSuspend() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSuspendSystemForDebugger() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSwitchToBaseMode() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSwitchToNeoMode() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSwitchToNeoMode2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSystemReboot() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSystemShutdown2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSystemSuspend() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplicationSystemSuspend2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceApplictionGetStateForDebugger() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceSysCoreGetEventCount() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
int APS5_VABI sceSysCoreReceiveEvent() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationExitSpawn3() {
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationKickCoredump3() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationNotifyUnmountForExitSpawn() {
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationSendResultOfDebuggerRestoreRequest() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationSendResultOfDebuggerSaveRequest() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationSwitchToPprNativeMode() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceApplicationSwitchToPprNativeMode2() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
}

extern "C" {

int APS5_VABI libSceSysCoreUnknown_F2qydFnZWfY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("F2qydFnZWfY", libSceSysCoreUnknown_F2qydFnZWfY_nid_no_patch);

int APS5_VABI libSceSysCoreUnknown_Q5nkKEHtW18_nid_no_patch() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Q5nkKEHtW18", libSceSysCoreUnknown_Q5nkKEHtW18_nid_no_patch);

int APS5_VABI libSceSysCoreUnknown_V7awqOXNG30_nid_no_patch() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("V7awqOXNG30", libSceSysCoreUnknown_V7awqOXNG30_nid_no_patch);

int APS5_VABI libSceSysCoreUnknown_lGA8mxzKIhk_nid_no_patch() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    if (!g_shellCoreInitialized.load(std::memory_order_acquire)) return kShellCoreNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lGA8mxzKIhk", libSceSysCoreUnknown_lGA8mxzKIhk_nid_no_patch);

int APS5_VABI libSceSysCoreUnknown_piCjhbSYauo_nid_no_patch() {
    if (!g_initialized.load(std::memory_order_acquire)) return kNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("piCjhbSYauo", libSceSysCoreUnknown_piCjhbSYauo_nid_no_patch);

}
