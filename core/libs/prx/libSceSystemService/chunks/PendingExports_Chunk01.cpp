extern "C" {
#include "prx/libSceSystemService/include/SystemService.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>

namespace {

std::filesystem::space_info HostSpace() {
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::current_path(error);
    if (error) return {};
    const std::filesystem::space_info space = std::filesystem::space(directory, error);
    return error ? std::filesystem::space_info{} : space;
}

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

bool EmptyPrivacyDefinitionKey(const char* key) {
    if (key == nullptr) return false;
    char first = 1;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&first, key, sizeof(first));
    errno = savedErrno;
    return copied && first == '\0';
}

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

int APS5_VABI sceLncUtilIsSpeculativeLaunch(std::int32_t appId, bool* speculative) {
    if (speculative == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
    if (appId != sceSystemServiceGetAppIdOfRunningBigApp()) throw std::runtime_error("sceLncUtilIsSpeculativeLaunch: application other than the running title");
    const bool localSpeculative = false;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(speculative, &localSpeculative, sizeof(localSpeculative));
    errno = savedErrno;
    if (!copied) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
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
    if (titleId == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
    std::string id;
    const int savedErrno = errno;
    const int error = CopyGuestString_nid_no_patch(titleId, 10, id);
    errno = savedErrno;
    if (error == 14) throw std::logic_error("sceLncUtilLaunchApp: reading a title id where it is not readable faults in firmware 13.60");
    if (error != 0 && error != 63) throw std::bad_alloc();
    if (error == 63 || id.size() != 9 || param == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
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
    const std::int32_t localStatus = ActivationStatus;
    const std::int32_t localResult = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &localStatus, sizeof(localStatus)) && GuestCopyOut_nid_no_patch(result, &localResult, sizeof(localResult));
    errno = savedErrno;
    if (!copied) return ShellCoreInvalidArgument;
    return 0;
}

int APS5_VABI sceShellCoreUtilActivateInit(std::int32_t type, std::intptr_t* handle) {
    if (handle == nullptr) return ShellCoreInvalidArgument;
    const std::intptr_t cleared = 0;
    const int savedErrno = errno;
    const bool writable = GuestCopyOut_nid_no_patch(handle, &cleared, sizeof(cleared));
    errno = savedErrno;
    if (!writable) return ShellCoreInvalidArgument;
    const std::intptr_t localHandle = nextActivationHandle.fetch_add(ActivationHandleStep);
    const bool copied = GuestCopyOut_nid_no_patch(handle, &localHandle, sizeof(localHandle));
    errno = savedErrno;
    if (!copied) return ShellCoreInvalidArgument;
    return 0;
}

}
