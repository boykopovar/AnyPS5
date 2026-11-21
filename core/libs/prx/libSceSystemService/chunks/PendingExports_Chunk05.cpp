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
    return sceShellCoreUtilNavigateToAnotherApp(target);
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
    return sceShellCoreUtilNavigateToGoHome(target);
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
    return sceLncUtilSetControllerFocusPermission(permission);
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

int APS5_VABI sceSystemStateMgrIsGpuPerformanceNormal() {
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
