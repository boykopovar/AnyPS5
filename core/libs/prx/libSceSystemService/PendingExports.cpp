#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int SystemServiceUnavailable = static_cast<int>(0x80A10002);
constexpr int SystemServiceInvalidArgument = static_cast<int>(0x80A10003);
constexpr int SystemServiceRejected = static_cast<int>(0x80A10005);
constexpr int ShellCoreInvalidArgument = static_cast<int>(0x80E30005);

constexpr std::int32_t ActivationTypeMpeg2 = 0;
constexpr std::int32_t ActivationTypeHevc = 3;
constexpr std::int32_t ActivationTypeHevcSoft = 4;
constexpr std::int32_t ActivationStatus = 2;
constexpr std::intptr_t ActivationHandleStep = 4;
constexpr std::int32_t FirstPSButtonEventType = 0x20000001;
constexpr std::int32_t LastPSButtonEventType = 0x20000002;

std::atomic<std::intptr_t> nextActivationHandle{0x1000};

}

static_assert(sizeof(ShellCoreErrorDialogParam) == 24);
static_assert(offsetof(ShellCoreErrorDialogParam, message) == 0);
static_assert(offsetof(ShellCoreErrorDialogParam, formatArgs) == 8);
static_assert(offsetof(ShellCoreErrorDialogParam, flag) == 16);

struct SystemServiceEvent;

static_assert(sizeof(SystemServicePSButtonEvent) == 16);
static_assert(offsetof(SystemServicePSButtonEvent, dataAt8) == 4);
static_assert(offsetof(SystemServicePSButtonEvent, dataAt0) == 8);

static_assert(sizeof(SystemServiceStoreParam) == 4);

static_assert(sizeof(ShellCoreLaunchByUriParam) == 8);
static_assert(offsetof(ShellCoreLaunchByUriParam, option) == 0);
static_assert(offsetof(ShellCoreLaunchByUriParam, userId) == 4);

static_assert(sizeof(ShellCoreUmmStatus) == 8);
static_assert(offsetof(ShellCoreUmmStatus, state) == 0);
static_assert(offsetof(ShellCoreUmmStatus, inDeviceMuteState) == 4);

static_assert(sizeof(LncGpuCrashAppStatus) == 1320);
static_assert(offsetof(LncGpuCrashAppStatus, titleId) == 0);
static_assert(offsetof(LncGpuCrashAppStatus, appId) == 16);
static_assert(offsetof(LncGpuCrashAppStatus, appLocalPid) == 20);
static_assert(offsetof(LncGpuCrashAppStatus, path) == 24);
static_assert(offsetof(LncGpuCrashAppStatus, appType) == 1048);
static_assert(offsetof(LncGpuCrashAppStatus, coredumpReason) == 1052);
static_assert(offsetof(LncGpuCrashAppStatus, corefileName) == 1056);
static_assert(offsetof(LncGpuCrashAppStatus, exceptionCode) == 1312);
static_assert(offsetof(LncGpuCrashAppStatus, appCategoryType) == 1316);

static_assert(sizeof(LncLaunchParam) == 40);
static_assert(offsetof(LncLaunchParam, size) == 0);
static_assert(offsetof(LncLaunchParam, userId) == 4);
static_assert(offsetof(LncLaunchParam, appAttr) == 8);
static_assert(offsetof(LncLaunchParam, enableCrashReport) == 12);
static_assert(offsetof(LncLaunchParam, checkFlag) == 16);
static_assert(offsetof(LncLaunchParam, contextId) == 24);
static_assert(offsetof(LncLaunchParam, isSpeculativeLaunch) == 32);

static_assert(sizeof(LncLocalProcessStatus) == 36);
static_assert(offsetof(LncLocalProcessStatus, appLocalPid) == 0);
static_assert(offsetof(LncLocalProcessStatus, reserved) == 4);

static_assert(sizeof(LncCoredumpState) == 12);
static_assert(offsetof(LncCoredumpState, progress) == 0);
static_assert(offsetof(LncCoredumpState, isFinished) == 4);
static_assert(offsetof(LncCoredumpState, result) == 8);

static_assert(sizeof(LncVideoFinalizeState) == 8);
static_assert(offsetof(LncVideoFinalizeState, progress) == 0);
static_assert(offsetof(LncVideoFinalizeState, result) == 4);

static_assert(sizeof(LncAppStatus) == 64);
static_assert(offsetof(LncAppStatus, appId) == 0);
static_assert(offsetof(LncAppStatus, launchRequestAppId) == 4);
static_assert(offsetof(LncAppStatus, appType) == 8);
static_assert(offsetof(LncAppStatus, appAttr) == 12);
static_assert(offsetof(LncAppStatus, titleId) == 16);
static_assert(offsetof(LncAppStatus, appCategoryType) == 32);
static_assert(offsetof(LncAppStatus, appBootMode) == 36);
static_assert(offsetof(LncAppStatus, appBootSubMode) == 40);
static_assert(offsetof(LncAppStatus, appVer) == 44);
static_assert(offsetof(LncAppStatus, contentVersion) == 50);
static_assert(offsetof(LncAppStatus, isSystemExApp) == 61);
static_assert(offsetof(LncAppStatus, isCdlgInitialized) == 62);

static_assert(sizeof(LncShellUiAppStatus) == 1336);
static_assert(offsetof(LncShellUiAppStatus, appId) == 0);
static_assert(offsetof(LncShellUiAppStatus, appType) == 4);
static_assert(offsetof(LncShellUiAppStatus, appAttr) == 8);
static_assert(offsetof(LncShellUiAppStatus, launchRequestAppId) == 12);
static_assert(offsetof(LncShellUiAppStatus, userId) == 16);
static_assert(offsetof(LncShellUiAppStatus, isActiveCdlg) == 20);
static_assert(offsetof(LncShellUiAppStatus, path) == 24);
static_assert(offsetof(LncShellUiAppStatus, sceSysDir) == 1048);
static_assert(offsetof(LncShellUiAppStatus, isCoredumped) == 1304);
static_assert(offsetof(LncShellUiAppStatus, isPrxModuleLoadFailed) == 1308);
static_assert(offsetof(LncShellUiAppStatus, appLocalPid) == 1312);
static_assert(offsetof(LncShellUiAppStatus, crashReportMode) == 1316);
static_assert(offsetof(LncShellUiAppStatus, category) == 1320);
static_assert(offsetof(LncShellUiAppStatus, appCategoryType) == 1324);
static_assert(offsetof(LncShellUiAppStatus, appBootMode) == 1328);
static_assert(offsetof(LncShellUiAppStatus, appBootSubMode) == 1332);

static_assert(sizeof(AppMessagingMessage) == 8216);
static_assert(offsetof(AppMessagingMessage, sender) == 0);
static_assert(offsetof(AppMessagingMessage, messageType) == 4);
static_assert(offsetof(AppMessagingMessage, payload) == 8);
static_assert(offsetof(AppMessagingMessage, payloadSize) == 8200);
static_assert(offsetof(AppMessagingMessage, timestamp) == 8208);

extern "C" {

int APS5_VABI sceAppMessagingClearEventFlag(std::int32_t appId, std::uint64_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppMessagingReceiveMsg(AppMessagingMessage* message) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppMessagingSendMsg(std::uint32_t appId, std::uint32_t messageId, const void* data, std::size_t size, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppMessagingSendMsgToShellCore(std::uint32_t messageId, const void* data, std::size_t size, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppMessagingSendMsgToShellUI(std::uint32_t messageId, const void* data, std::size_t size, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppMessagingSetEventFlag(std::int32_t appId, std::uint64_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppMessagingTryGetEventFlag(std::uint64_t mask, std::uint64_t* flags, std::uint8_t clear) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAppMessagingTryReceiveMsg(AppMessagingMessage* message) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilAcquireCpuBudgetOfExtraAudioDevices() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilAcquireCpuBudgetOfImeForBigApp() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilAcquireCpuBudgetOfInGameStore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilActivateCdlg() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilAddLocalProcess(std::int32_t appId, const char* path, const char* const* argv, const LncLocalProcessParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilBlockAppSuspend(const char* reason) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilBlockingGetEventForDaemon(LncDaemonEvent* event, std::uint64_t receiveArgument) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilContinueApp(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilCrashSyscore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilDeactivateCdlg() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilDeclareReadyForSuspend() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilDisableSuspendNotification() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilEnableSuspendNotification() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilFinishSpecialResume() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilForceKillApp(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilForceKillLocalProcess(std::int32_t appId, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetAppCategory(std::int32_t appId, char* category) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetAppCategoryType(std::int32_t appId, std::uint32_t* categoryType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetAppFocusedAppStatus(LncAppStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetAppStatus(LncAppStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetAppStatusList(LncAppStatus* statuses, std::uint32_t capacity, std::uint32_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetAppStatusListForShellUIReboot(LncShellUiAppStatus* statuses, std::uint32_t capacity, std::uint32_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetAppType(std::int32_t appId, std::uint32_t* type) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetCdlgExec(std::int32_t appId, bool* executing) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetCdlgType(std::int32_t appId, std::uint32_t* type) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetCoredumpState(std::int32_t appId, std::int32_t processId, LncCoredumpState* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetDbgExecutablePath(char* path, std::uint64_t selector) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetEventForDaemon(LncDaemonEvent* event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetEventForShellUI(LncShellUiEvent* event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetGpuCrashFullDumpAppStatus(LncGpuCrashAppStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetLocalProcessStatusList(LncLocalProcessStatus* statuses, std::uint32_t capacity, std::uint32_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetParentSocket() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetResultKillApp(std::uint32_t requestId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetResultLaunchAppByTitleId(std::uint32_t requestId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetVideoFinalizeState(std::int32_t appId, std::int32_t processId, LncVideoFinalizeState* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilIsActiveCdlg(bool* active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilIsCdlgActivated(std::uint8_t* activated) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceLncUtilIsCpuBudgetOfExtraAudioDevicesAvailable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilIsHostBoot(std::int32_t appId, bool* hostBoot) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilIsPs2Emu(bool* emulated) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceLncUtilIsShellUiFgAndGameBgCpuMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilIsSpeculativeLaunch(std::int32_t appId, bool* speculative) {
    if (speculative == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
    if (appId != sceSystemServiceGetAppIdOfRunningBigApp()) throw std::runtime_error("sceLncUtilIsSpeculativeLaunch: application other than the running title");
    *speculative = false;
    return 0;
}

int APS5_VABI sceLncUtilKickCoredumpOnlyProcMem(std::int32_t appId, std::int32_t processId, const char* first, const char* second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilKillLocalProcess(std::int32_t appId, std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilLaunchApp(const char* titleId, const char* const* argv, const LncLaunchParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilNotifyCoredumpRequestEnd(std::int32_t appId, std::int32_t processId, std::int32_t result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilNotifyCoredumpRequestProgress(std::int32_t appId, std::int32_t processId, std::int32_t progress) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilNotifyVshReady(std::uint32_t errorCode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilRaiseException(std::int32_t appId, std::uint64_t exception) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilRaiseExceptionLocalProcess(std::int32_t appId, std::int32_t processId, std::uint64_t exception) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilRegisterCdlgSharedMemoryName(const char* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilRegisterDaemon() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilRegisterShellUI() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilReleaseCpuBudgetOfExtraAudioDevices() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilReleaseCpuBudgetOfImeForBigApp() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilReleaseCpuBudgetOfInGameStore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilResumeApp(std::int32_t appId, std::uint64_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilResumeLocalProcess(std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilSetAppFocus(std::int32_t appId, std::uint64_t flags, std::uint64_t contextId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilSetCdlgExec(std::int32_t appId, std::uint8_t executing) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilSetControllerFocus(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilSetControllerFocusPermission(std::uint8_t permission) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilStartKillApp(std::int32_t appId, std::uint32_t* requestId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilStartLaunchAppByTitleId(const char* titleId, const char* const* argv, const LncLaunchParam* param, std::uint32_t* requestId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilSuspendApp(std::int32_t appId, std::uint64_t flags, std::int32_t reason) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilSuspendBackgroundApp(std::int32_t first, std::int32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilSuspendLocalProcess(std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilTryBlockAppSuspend(const char* reason) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilUnblockAppSuspend(const char* reason) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilUnregisterCdlgSharedMemoryName(const char* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilUnregisterDaemon() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilUnregisterShellUI() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilAccessibilityZoomLock() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilAccessibilityZoomUnlock() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilAcquireBgmCpuBudget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilAcquireRemotePlayCpuBudget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilAcquireSharePlayCpuBudget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilActivateAbort(std::intptr_t handle) {
    return 0;
}

int APS5_VABI sceShellCoreUtilActivateGetStatus(std::intptr_t handle, std::int32_t* status, std::int32_t* result) {
    if (status == nullptr || result == nullptr) return ShellCoreInvalidArgument;
    *status = ActivationStatus;
    *result = 0;
    return 0;
}

int APS5_VABI sceShellCoreUtilActivateInit(std::int32_t type, std::intptr_t* handle) {
    if (handle == nullptr) return ShellCoreInvalidArgument;
    *handle = nextActivationHandle.fetch_add(ActivationHandleStep);
    return 0;
}

int APS5_VABI sceShellCoreUtilActivateIsActivated(std::int32_t type, bool* activated) {
    if (activated == nullptr) return ShellCoreInvalidArgument;
    *activated = true;
    return 0;
}

int APS5_VABI sceShellCoreUtilActivateRecordActivation() {
    return 0;
}

int APS5_VABI sceShellCoreUtilActivateStart(std::intptr_t handle) {
    return 0;
}

int APS5_VABI sceShellCoreUtilActivateStartAsync(std::intptr_t handle) {
    return 0;
}

int APS5_VABI sceShellCoreUtilActivateTerm(std::intptr_t handle) {
    return 0;
}

int APS5_VABI sceShellCoreUtilChangeRunLevel(std::int32_t runLevel) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilChangeToStaffModeForIDU() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilCheckerAbort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilCleanupCrashReport() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilClearAppData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilClearPs4AppCategoryDefaultsListUpdate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilClearPsnAccountInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilCrashReportRequestCancel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDeclareBeginOfExternalStorageAppMove() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDeclareEndOfExternalStorageAppMove() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDeleteDiscInstalledTitleWorkaroundFile() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDeleteDownloadedHidConfigFile(std::int32_t configIndex) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDeleteDownloadedNetEvConfigFile() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDeleteDownloadedTitleWorkaroundFile() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDoFsck(const char* path) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDownloadHidConfigFileFromServer(std::int32_t configIndex) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDownloadNetEvConfigFileFromServer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDownloadTitleWorkaroundFileFromServer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilEndUmmDictation() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilEnterPowerLockSection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilExecuteCrashReport() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilExfatFormatExternalHdd(const char* path) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilExitApp() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilExitMiniApp() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilExitMiniAppWithValue(const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilFormatExternalHdd(const char* path) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilFormatHddForRestore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAppData(const char* key, void* data, std::int32_t capacity, std::int32_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAppEnableTTS(std::int32_t appId, bool* enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAppEnterButtonAssign(std::int32_t appId, std::int32_t* assignment) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAppLaunchTypeInfo(std::int32_t appId, ShellCoreAppLaunchTypeInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAppLaunchedParamInt(std::int32_t appId, const char* key, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAppLaunchedParamIntByBudgetType(std::int32_t budgetType, const char* key, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAppLaunchedParamString(std::int32_t appId, const char* key, char* value, std::int32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAppLaunchedParamStringByBudgetType(std::int32_t budgetType, const char* key, char* value, std::int32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAutoPowerDownRemainingSeconds() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetBasicProductShape() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCheckerString(const char* key, char* value, std::size_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCheckerStringEx(const char* firstKey, const char* secondKey, char* value, std::size_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCloudClientStatus(std::uint32_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCrashReportCoreFileSetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCrashReportFilterInfoStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCrashReportInfoForBoot() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCrashReportInfoForBootStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCrashReportInfoStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCrashReportResult() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCrashReportStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCrashReportUploadStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetDeviceIndexBehavior(std::uint32_t* behavior, std::int32_t* userId, std::uint32_t* scene) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetDeviceIndexBehaviorWithTimeout(std::uint32_t* behavior, std::int32_t* userId, std::uint32_t* scene, std::uint8_t timeout) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetEffectiveTotalSizeOfUserPartition(std::uint64_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI sceShellCoreUtilGetFreeSizeOfAvContentsTmp() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetFreeSizeOfUserPartition(std::uint64_t* freeSize, std::uint64_t* totalSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetFsckProgress(const char* titleId, std::uint32_t* progress) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetGameLiveStreamingStatus(bool* active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilGetGnmCompositorOnScreenProfilerFlag() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetGpuLoadEmulationMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetGpuLoadEmulationModeByAppId(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetHidConfigFileInfoString(char* value, std::size_t capacity, std::int32_t configIndex) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetHidConfigFileString(char* value, std::size_t capacity, std::int32_t configIndex) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetHidConfigName(char* value, std::size_t capacity, std::int32_t configIndex) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetHidConfigNum(std::int32_t* configNum) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetIDUMode(std::uint8_t* mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilGetImposeMenuFlagForPs2Emu(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetManifestFileStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetNeedSizeOfAppContent(std::uint64_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetNetEvConfigFileInfoString(char* value, std::size_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetOutOfVrPlayZoneWarning() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPapcGamePcl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPbtcUserInfoList(ShellCorePbtcUserInfoList* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPlatformPrivacyDefinitionEventData(const char* firstKey, const char* secondKey, ShellCorePrivacyEventData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

float APS5_VABI sceShellCoreUtilGetProgressOfFormatExternalHdd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

float APS5_VABI sceShellCoreUtilGetProgressOfFsck() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPs4AppCategoryDefaultsListId(void* data, std::size_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPs4AppCategoryForTitleId(const char* titleId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPsStoreIconLayout(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPsStoreIconState(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPsnAccountInfo(ShellCorePsnAccountInfoFirst* first, ShellCorePsnAccountInfoSecond* second, ShellCorePsnAccountInfoThird* third, ShellCorePsnAccountInfoFourth* fourth, ShellCorePsnAccountInfoFifth* fifth) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetRemotePlayStatus(bool* active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetRunLevel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetSharePlayStatus(bool* active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetSocialScreenStatus(bool* active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetSplashScreenState(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetSupportSiteURL(std::uint32_t first, std::uint32_t second, char* value, std::size_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilGetSuspendConfirmationDialogFlag(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetSystemBGState(std::int32_t appId, std::uint32_t* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetSystemBGWaveColor(std::int32_t appId, std::uint32_t* color) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetSystemBGWaveState(std::int32_t appId, std::uint32_t* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetTelemetryToken(ShellCoreTelemetryToken* token) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetTitleWorkaroundFileInfoString(char* value, std::size_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetTitleWorkaroundFileString(char* value, std::size_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetUIStatus(std::uint64_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetUmmStatus(std::int32_t userId, ShellCoreUmmStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetUserIdOfMorpheusUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetVMStatsForTelemetry(char* value, std::size_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetVersionNumberOfCameraCalibrationData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGoBackToKratosCurrentSessionGame() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilHideBlocksFromUser(std::int64_t bytes, std::int32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIncrementVersionNumberOfCameraCalibrationData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsAccessibilityZoomLocked(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsBgmCpuBudgetAcquired() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsBgmCpuBudgetAvailable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsExternalStorageAppMoveInProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsEyeToEyeDistanceAdjusted(std::uint32_t userId, std::uint8_t* adjusted) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsGameLiveStreamingOnAir() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsIDUState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsImposeScreenOverlaid() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsInSystemSuspendBlackList(std::uint32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsInternalKratosUser() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsKilledOrSuspendedByLogout(std::int32_t first, std::int32_t second, std::uint8_t* result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsNeededCrashReport() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsPowerSaveAlertRequested() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsRemotePlayCpuBudgetAcquired() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsScreenSaverOn() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsSharePlayCpuBudgetAcquired() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsShowCrashReport() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsTemperatureDanger() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsTitleWorkaroundEnabled(std::int32_t selector, const char* key) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsUsbMassStorageMounted(std::uint32_t deviceIndex) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilLaunchByUri(const char* uri, const ShellCoreLaunchByUriParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilLeavePowerLockSection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilMakeManifestFile() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilMountAppRight(const char* key) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilMountDownloadDataForShellUI(const char* key) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilMountHddForBackup() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilMountHddForRestore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNavigateToAnotherApp(std::uint32_t target) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNavigateToGoHome(std::uint32_t target) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNavigateToLaunchedApp(std::uint32_t target) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotificationCancelForIDU(std::int64_t notificationId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotificationRequestedForIDU(std::int64_t notificationId, const char* text, std::uint32_t type) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotifyBgmCoreTermination() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotifyFsReadError(std::int32_t first, std::int32_t second, std::int32_t third, const char* detail) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotifyImeStatusChanged(std::int32_t imeStatus, std::uint8_t isCalledFromGame) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotifyPsnAccountInfoReceived() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotifySystemSuspendTelemetryInfo(std::uint32_t info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilPfAuthClientConsoleTokenClearCache() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilPostActivityForPsNow(std::int32_t type, const char* activity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilPostErrorLog(const char* titleId, std::int32_t internalErrorCode, const char* shortError, const char* messageBuffer, std::int32_t suggestionType, const char* logReport) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilPostLaunchConfirmResult(std::uint32_t first, std::uint32_t second) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilPostPsmEventToShellUI(std::int32_t eventId, const char* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilPreNotifyOfGameLiveStreaming() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilPreNotifyOfRemotePlay() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilPreNotifyOfSharePlay() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilReleaseBgmCpuBudget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilReleaseRemotePlayCpuBudget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilReleaseSharePlayCpuBudget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilReportSessionErrorToGaikaiController(std::int32_t error, const char* detail) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilReportUnexpectedFatalErrorToSystemTelemetry(std::int32_t error, const char* detail) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilRequestCameraCalibration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilRequestEjectDevice(const char* path) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilRequestRebootApp(std::int32_t appId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilRequestShutdown(std::int32_t reason) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilResetAutoPowerDownTimer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilResetBgdcConfig() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetAppData(const char* key, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetBgmProhibition(std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetDeviceIndexBehavior(std::uint32_t behavior, std::int32_t userId, std::uint8_t scene) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetGameLiveStreamingOnAirFlag(std::int32_t enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetGameLiveStreamingStatus(std::int32_t active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetGnmCompositorOnScreenProfilerFlag(std::int32_t enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetGpuLoadEmulationMode(std::uint32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetGpuLoadEmulationModeByAppId(std::int32_t appId, std::uint32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetIDUMode(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetImposeStatusFlag(std::int32_t enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetPsStoreIconLayout(std::uint32_t layout) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetPsStoreIconState(std::uint32_t state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetRemotePlayStatus(std::int32_t active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetSharePlayStatus(std::int32_t active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetSkipUpdateCheck(std::uint8_t skip) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetSocialScreenStatus(std::int32_t active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetSplashScreenState(std::uint32_t state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetSystemBGState(std::uint32_t state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetSystemBGWaveColor(std::uint32_t color) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetSystemBGWaveState(std::uint32_t state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetUIStatus(std::uint64_t status, std::uint64_t mask) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetUserFocus(std::int32_t userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilShowCriticalErrorDialog(std::uint32_t error, const char* message) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilShowErrorDialog(std::uint32_t error, const char* message) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilShowErrorDialogWithFormatArgs(std::uint32_t error, const char* message, const char* const* formatArgs) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilShowErrorDialogWithParam(std::uint32_t error, const ShellCoreErrorDialogParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilShowPsUnderLockIndicator() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSignalUserInput() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilStartPsNowGame(const char* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilStopPsNowGame(const char* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilTalkKeyEventReceived(std::int64_t deviceId, std::int32_t userId, std::int32_t keyEventType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilTestBusTransferSpeed(const char* path, std::uint8_t* result) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilTickHeartBeat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilTriggerPapcUpdate(std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilTurnOffScreenSaver() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilUnmountAppRight(const char* key) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilUnmountDownloadDataForShellUI(const char* key) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilUnmountHddForBackup() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilUnmountHddForRestore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreutilGetCrashReportProcessInformation() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemModalDialogClose(std::uint32_t dialogId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemModalDialogOpen(const char* message) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceAcquireBgmCpuBudget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceAcquireFb0() {
    return SystemServiceUnavailable;
}

int APS5_VABI sceSystemServiceActivateHevc() {
    return sceShellCoreUtilActivateRecordActivation();
}

int APS5_VABI sceSystemServiceActivateHevcAbort(std::uintptr_t handle) {
    if (handle == 0) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateAbort(static_cast<std::intptr_t>(handle));
}

int APS5_VABI sceSystemServiceActivateHevcGetStatus(std::uintptr_t handle, std::uint32_t* status, std::int32_t* result) {
    if (handle == 0 || status == nullptr || result == nullptr) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateGetStatus(static_cast<std::intptr_t>(handle), reinterpret_cast<std::int32_t*>(status), result);
}

int APS5_VABI sceSystemServiceActivateHevcInit(std::uintptr_t* handle) {
    if (handle == nullptr) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateInit(ActivationTypeHevc, reinterpret_cast<std::intptr_t*>(handle));
}

int APS5_VABI sceSystemServiceActivateHevcIsActivated(std::uint8_t* activated) {
    if (activated == nullptr) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateIsActivated(ActivationTypeHevc, reinterpret_cast<bool*>(activated));
}

int APS5_VABI sceSystemServiceActivateHevcSoft() {
    return sceShellCoreUtilActivateRecordActivation();
}

int APS5_VABI sceSystemServiceActivateHevcSoftAbort(std::uintptr_t handle) {
    if (handle == 0) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateAbort(static_cast<std::intptr_t>(handle));
}

int APS5_VABI sceSystemServiceActivateHevcSoftGetStatus(std::uintptr_t handle, std::uint32_t* status, std::int32_t* result) {
    if (handle == 0 || status == nullptr || result == nullptr) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateGetStatus(static_cast<std::intptr_t>(handle), reinterpret_cast<std::int32_t*>(status), result);
}

int APS5_VABI sceSystemServiceActivateHevcSoftInit(std::uintptr_t* handle) {
    if (handle == nullptr) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateInit(ActivationTypeHevcSoft, reinterpret_cast<std::intptr_t*>(handle));
}

int APS5_VABI sceSystemServiceActivateHevcSoftIsActivated(std::uint8_t* activated) {
    if (activated == nullptr) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateIsActivated(ActivationTypeHevcSoft, reinterpret_cast<bool*>(activated));
}

int APS5_VABI sceSystemServiceActivateHevcSoftStart(std::uintptr_t handle) {
    if (handle == 0) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateStartAsync(static_cast<std::intptr_t>(handle));
}

int APS5_VABI sceSystemServiceActivateHevcSoftTerm(std::uintptr_t handle) {
    if (handle == 0) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateTerm(static_cast<std::intptr_t>(handle));
}

int APS5_VABI sceSystemServiceActivateHevcStart(std::uintptr_t handle) {
    if (handle == 0) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateStartAsync(static_cast<std::intptr_t>(handle));
}

int APS5_VABI sceSystemServiceActivateHevcTerm(std::uintptr_t handle) {
    if (handle == 0) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateTerm(static_cast<std::intptr_t>(handle));
}

int APS5_VABI sceSystemServiceActivateMpeg2Abort(std::uintptr_t handle) {
    if (handle == 0) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateAbort(static_cast<std::intptr_t>(handle));
}

int APS5_VABI sceSystemServiceActivateMpeg2GetStatus(std::uintptr_t handle, std::uint32_t* status, std::int32_t* result) {
    if (handle == 0 || status == nullptr || result == nullptr) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateGetStatus(static_cast<std::intptr_t>(handle), reinterpret_cast<std::int32_t*>(status), result);
}

int APS5_VABI sceSystemServiceActivateMpeg2Init(std::uintptr_t* handle) {
    if (handle == nullptr) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateInit(ActivationTypeMpeg2, reinterpret_cast<std::intptr_t*>(handle));
}

int APS5_VABI sceSystemServiceActivateMpeg2IsActivated(std::uint8_t* activated) {
    if (activated == nullptr) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateIsActivated(ActivationTypeMpeg2, reinterpret_cast<bool*>(activated));
}

int APS5_VABI sceSystemServiceActivateMpeg2Start(std::uintptr_t handle) {
    if (handle == 0) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateStartAsync(static_cast<std::intptr_t>(handle));
}

int APS5_VABI sceSystemServiceActivateMpeg2Term(std::uintptr_t handle) {
    if (handle == 0) return SystemServiceInvalidArgument;
    return sceShellCoreUtilActivateTerm(static_cast<std::intptr_t>(handle));
}

int APS5_VABI sceSystemServiceAddLocalProcess(std::int32_t appId, const char* path, const char* const* argv, const SystemServiceLocalProcessParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceAddLocalProcessForJvm(const char* path, const char* const* argv, const SystemServiceJvmProcessParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceAddLocalProcessForPs2Emu(const char* path, const char* const* argv, const SystemServicePs2ProcessParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceAppGetAppInstallStatus(const char* titleId, SystemServiceAppInstallStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceChangeAcpClock() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeClock(const char* clock, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceChangeCpuClock() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeCpuPstate(const SystemServiceCpuPstateParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceChangeGpuClock() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeMemPstate(std::uint32_t state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceChangeMemoryClock() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeMemoryClockToBaseMode() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeMemoryClockToDefault() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeMemoryClockToMultiMediaMode() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeNumberOfGpuCu() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeSamuClock() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeUvdClock() {
    return 0;
}

int APS5_VABI sceSystemServiceChangeVceClock() {
    return 0;
}

int APS5_VABI sceSystemServiceDeclareReadyForSuspend() {
    return sceLncUtilDeclareReadyForSuspend();
}

int APS5_VABI sceSystemServiceDisablePartyVoice() {
    return SystemServiceUnavailable;
}

int APS5_VABI sceSystemServiceDisablePersonalEyeToEyeDistanceSetting() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceDisableSuspendConfirmationDialog() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceDisableSuspendNotification() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceDisableVoiceRecognition() {
    return SystemServiceUnavailable;
}

int APS5_VABI sceSystemServiceEnablePersonalEyeToEyeDistanceSetting() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceEnableSuspendConfirmationDialog() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceEnableSuspendNotification() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetAppCategoryType(std::int32_t appId, std::uint32_t* categoryType) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetAppFocusedAppStatus(SystemServiceAppStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetAppStatus(SystemServiceAppStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetAppType(std::int32_t appId, std::int32_t* type) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetDbgExecutablePath(char* path, std::uint64_t selector) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetEventForDaemon(SystemServiceDaemonEvent* event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetGpuLoadEmulationMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetLocalProcessStatusList(SystemServiceLocalProcessStatus* statuses, std::uint32_t capacity, std::uint32_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetMainAppTitleId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetPSButtonEvent(const SystemServiceEvent* event, SystemServicePSButtonEvent* buttonEvent) {
    if (event == nullptr || buttonEvent == nullptr) return SystemServiceInvalidArgument;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(event);
    SystemServicePSButtonEvent result{};
    std::memcpy(&result.eventType, bytes, sizeof(result.eventType));
    if (result.eventType < FirstPSButtonEventType || result.eventType > LastPSButtonEventType) return SystemServiceInvalidArgument;
    std::memcpy(&result.dataAt0, bytes + 4, sizeof(result.dataAt0));
    std::memcpy(&result.dataAt8, bytes + 12, sizeof(result.dataAt8));
    *buttonEvent = result;
    return 0;
}

int APS5_VABI sceSystemServiceGetParentSocket() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetParentSocketForJvm() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetParentSocketForPs2Emu() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetParentSocketForPsmKit() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetPlatformPrivacyDefinitionData(const char* key, SystemServicePrivacyDefinitionData* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetPlatformPrivacyDefinitionVersion(const char* key, SystemServicePrivacyDefinitionVersion* version) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetPlatformPrivacySetting(std::uint32_t* setting) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetRecentLotusPushedButton(std::uint32_t timeout, std::int32_t* deviceId, std::uint32_t* buttons) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetRemoteplayStatus(bool* active) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetRenderingMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetSubAppTitleId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetSubAppTitleIdForSP() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetTitleWorkaroundInfo(SystemServiceTitleWorkaroundInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetUIStatus(std::uint64_t* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetVersionNumberOfCameraCalibrationData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceInitializeForShellCore() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceInvokeAppLaunchLink(const char* uri) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceSystemServiceIsBgmCpuBudgetAvailable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceIsEyeToEyeDistanceAdjusted(std::uint32_t userId, std::uint8_t* adjusted) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceSystemServiceIsGameLiveStreamingOnAir() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceSystemServiceIsScreenSaverOn() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceSystemServiceIsShellUiFgAndGameBgCpuMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceKillLocalProcess(std::int32_t appId, std::int32_t processId) {
    return sceLncUtilKillLocalProcess(appId, processId);
}

int APS5_VABI sceSystemServiceKillLocalProcessForJvm(std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceKillLocalProcessForPs2Emu(std::int32_t processId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceLaunchEventDetails(std::uint64_t unusedSlot, const char* eventId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceLaunchStore(const char* uri, const SystemServiceStoreParam* param) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceLaunchTournamentList() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceLaunchTournamentsTeamProfile(std::uint64_t unusedSlot, const char* profile, const char* tournament, const char* context) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceLaunchUdsApp() {
    return SystemServiceRejected;
}

int APS5_VABI sceSystemServiceLaunchWebApp(const char* argument) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceLoadExecVideoServiceWebApp() {
    return SystemServiceUnavailable;
}

int APS5_VABI sceSystemServiceNavigateToAnotherApp(std::uint32_t target) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceNavigateToGoBack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceNavigateToGoBackWithValue(const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceNavigateToGoHome(std::uint32_t target) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceNotifyBgmCoreTermination() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceRaiseExceptionLocalProcess(std::int32_t appId, std::int32_t processId, std::uint64_t exception) {
    return sceLncUtilRaiseExceptionLocalProcess(appId, processId, exception);
}

int APS5_VABI sceSystemServiceReenablePartyVoice() {
    return SystemServiceUnavailable;
}

int APS5_VABI sceSystemServiceReenableVoiceRecognition() {
    return SystemServiceUnavailable;
}

int APS5_VABI sceSystemServiceRegisterDaemon() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceReleaseBgmCpuBudget() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceReleaseFb0() {
    return SystemServiceUnavailable;
}

int APS5_VABI sceSystemServiceRequestCameraCalibration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceRequestPowerOff() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceRequestReboot() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceRequestToChangeRenderingMode() {
    return SystemServiceRejected;
}

int APS5_VABI sceSystemServiceResumeLocalProcess(std::int32_t processId) {
    return sceLncUtilResumeLocalProcess(processId);
}

int APS5_VABI sceSystemServiceSaveVideoToken(std::int32_t userId, const char* token) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceSetControllerFocusPermission(std::uint8_t permission) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceSetGpuLoadEmulationMode(std::uint32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceSetOutOfVrPlayAreaFlag(std::uint8_t flag) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceSetOutOfVrPlayZoneWarning() {
    return 0;
}

int APS5_VABI sceSystemServiceSetPowerSaveLevel(std::uint32_t level) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceShowClosedCaptionAdvancedSettings() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceShowClosedCaptionSettings() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceShowErrorDialog(std::uint32_t error, const char* message) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceShowEyeToEyeDistanceSetting() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceShowImposeMenuForPs2Emu(std::uint8_t enabled, std::uint64_t reserved) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceSuspendBackgroundApp(std::int32_t first, std::int32_t second) {
    return sceLncUtilSuspendBackgroundApp(first, second);
}

int APS5_VABI sceSystemServiceSuspendLocalProcess(std::int32_t processId) {
    return sceLncUtilSuspendLocalProcess(processId);
}

int APS5_VABI sceSystemServiceTelemetrySetData(const char* key, const void* data, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceTelemetrySetData2(const char* key, const char* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceTickVideoPlayback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceTurnOffScreenSaver() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceUsbStorageGetDeviceInfo(std::uint32_t device, UsbStorageDeviceInfo* info) {
    return sceUsbStorageGetDeviceInfo(device, info);
}

int APS5_VABI sceSystemServiceUsbStorageGetDeviceList(std::uint32_t* devices, std::int32_t* count) {
    return sceUsbStorageGetDeviceList(devices, count);
}

int APS5_VABI sceSystemServiceUsbStorageGetdentsClose(std::int32_t fd) {
    return sceUsbStorageGetdentsClose(fd);
}

int APS5_VABI sceSystemServiceUsbStorageGetdentsOpen(std::uint32_t device, std::int32_t* fd, std::uint64_t* bufferSize) {
    return sceUsbStorageGetdentsOpen(device, fd, bufferSize);
}

int APS5_VABI sceSystemServiceUsbStorageGetdentsRead(std::int32_t fd, void* buffer, std::uint64_t bufferSize, std::uint64_t* bytesRead) {
    return sceUsbStorageGetdentsRead(fd, buffer, bufferSize, bytesRead);
}

int APS5_VABI sceSystemServiceUsbStorageInit(void* attr) {
    return sceUsbStorageInit(attr);
}

bool APS5_VABI sceSystemServiceUsbStorageIsExist(std::uint32_t device, const char* directory, std::uint8_t* exists) {
    return sceUsbStorageIsExist(device, directory, exists);
}

int APS5_VABI sceSystemServiceUsbStorageRegisterCallback(std::int32_t eventType, UsbStorageCallback callback, void* arg) {
    return sceUsbStorageRegisterCallback(eventType, callback, arg);
}

int APS5_VABI sceSystemServiceUsbStorageRegisterCallbackForMapAvailable(std::uint32_t device, const char* directory, UsbStorageCallback callback, void* arg) {
    return sceUsbStorageRegisterCallbackForMapAvailable(device, directory, callback, arg);
}

int APS5_VABI sceSystemServiceUsbStorageRegisterCallbackForMapUnavailable(std::uint32_t device, const char* directory, UsbStorageCallback callback, void* arg) {
    return sceUsbStorageRegisterCallbackForMapUnavailable(device, directory, callback, arg);
}

int APS5_VABI sceSystemServiceUsbStorageRequestMap(std::uint32_t device, const char* directory, std::int32_t flags, std::uint64_t volumeSize, char* mountPoint, std::uint64_t* mountId) {
    return sceUsbStorageRequestMap(device, directory, flags, volumeSize, mountPoint, mountId);
}

int APS5_VABI sceSystemServiceUsbStorageRequestMapWSB(std::uint32_t device, const char* directory, std::int32_t flags, std::uint64_t volumeSize, char* mountPoint, std::uint64_t* mountId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceUsbStorageRequestUnmap(std::uint32_t device, const char* directory) {
    return sceUsbStorageRequestUnmap(device, directory);
}

int APS5_VABI sceSystemServiceUsbStorageTerm() {
    return sceUsbStorageTerm();
}

int APS5_VABI sceSystemServiceUsbStorageUnregisterCallback(std::int32_t eventType, UsbStorageCallback callback) {
    return sceUsbStorageUnregisterCallback(eventType, callback);
}

int APS5_VABI sceSystemServiceUsbStorageUnregisterCallbackForMapAvailable(std::uint32_t device, const char* directory, UsbStorageCallback callback) {
    return sceUsbStorageUnregisterCallbackForMapAvailable(device, directory, callback);
}

int APS5_VABI sceSystemServiceUsbStorageUnregisterCallbackForMapUnavailable(std::uint32_t device, const char* directory, UsbStorageCallback callback) {
    return sceUsbStorageUnregisterCallbackForMapUnavailable(device, directory, callback);
}

int APS5_VABI sceSystemStateMgrCancelShutdownTimer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrEnterMediaPlaybackMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrEnterStandby() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrExtendShutdownTimer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrExtendShutdownTimerForPostAutoUpdateProcess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrGetCurrentState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrGetTriggerCode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceSystemStateMgrIsBdDriveReady() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrIsGpuPerformanceNormal() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceSystemStateMgrIsShellUIShutdownInProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceSystemStateMgrIsStandbyModeEnabled() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrLeaveMediaPlaybackMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrNotifySystemSuspendResumeProgress(const char* progress) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrReboot() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrRequestToKeepMainOnStandby(const char* reason) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrSendCecOneTouchPlayCommand() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrStartRebootTimer(std::int32_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrStartShutdownTimer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrStartStadbyTimer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrStartVshAutoUpdateTimer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrTickMusicPlayback() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrTickPartyChat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrTurnOff() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrVshAutoUpdate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrWaitVshAutoUpdateVerifyDone() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrWakeUp(std::uint16_t trigger) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
