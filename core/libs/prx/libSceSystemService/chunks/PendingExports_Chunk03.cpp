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

bool APS5_VABI sceShellCoreUtilIsBgmCpuBudgetAcquired() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceShellCoreUtilIsBgmCpuBudgetAvailable() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsIDUState() {
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

}
