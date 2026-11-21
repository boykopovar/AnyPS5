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

int APS5_VABI sceSystemServiceDisableVoiceRecognition() {
    return SystemServiceUnavailable;
}

int APS5_VABI sceSystemServiceEnablePersonalEyeToEyeDistanceSetting() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetAppFocusedAppStatus(SystemServiceAppStatus* status) {
    if (status == nullptr) NotImplemented_nid_no_patch(__func__);
    return LNC_UTIL_ERROR_GAME_PROCESS;
}

int APS5_VABI sceSystemServiceGetAppStatus(SystemServiceAppStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetAppType(std::int32_t appId, std::int32_t* type) {
    if (type == nullptr || appId == SYSTEM_SERVICE_RUNNING_APP_ID) NotImplemented_nid_no_patch(__func__);
    return LNC_UTIL_ERROR_GAME_PROCESS;
}

int APS5_VABI sceSystemServiceGetDbgExecutablePath(char* path, std::uint64_t) {
    if (path == nullptr) NotImplemented_nid_no_patch(__func__);
    return SystemServiceRejected;
}

int APS5_VABI sceSystemServiceGetEventForDaemon(SystemServiceDaemonEvent* event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetLocalProcessStatusList(SystemServiceLocalProcessStatus* statuses, std::uint32_t capacity, std::uint32_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetPSButtonEvent(const SystemServiceEvent* event, SystemServicePSButtonEvent* buttonEvent) {
    if (event == nullptr || buttonEvent == nullptr) return SystemServiceInvalidArgument;
    std::uint8_t bytes[20]{};
    const int savedErrno = errno;
    const bool copiedInput = GuestCopyIn_nid_no_patch(bytes, event, sizeof(bytes));
    errno = savedErrno;
    if (!copiedInput) return SystemServiceInvalidArgument;
    SystemServicePSButtonEvent result{};
    std::memcpy(&result.eventType, bytes, sizeof(result.eventType));
    if (result.eventType < FirstPSButtonEventType || result.eventType > LastPSButtonEventType) return SystemServiceInvalidArgument;
    std::memcpy(&result.dataAt0, bytes + 4, sizeof(result.dataAt0));
    std::memcpy(&result.dataAt8, bytes + 12, sizeof(result.dataAt8));
    const int copyErrno = errno;
    const bool copiedOutput = GuestCopyOut_nid_no_patch(buttonEvent, &result, sizeof(result));
    errno = copyErrno;
    return copiedOutput ? 0 : SystemServiceInvalidArgument;
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
    if (data == nullptr || !EmptyPrivacyDefinitionKey(key)) NotImplemented_nid_no_patch(__func__);
    return SYSTEM_SERVICE_ERROR_INTERNAL;
}

int APS5_VABI sceSystemServiceGetPlatformPrivacyDefinitionVersion(const char* key, SystemServicePrivacyDefinitionVersion* version) {
    if (version == nullptr || !EmptyPrivacyDefinitionKey(key)) NotImplemented_nid_no_patch(__func__);
    return SYSTEM_SERVICE_ERROR_INTERNAL;
}

int APS5_VABI sceSystemServiceGetPlatformPrivacySetting(std::uint32_t* setting) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetRecentLotusPushedButton(std::uint32_t timeout, std::int32_t* deviceId, std::uint32_t* buttons) {
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
    return sceShellCoreUtilGetUIStatus(status);
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
    return sceShellCoreUtilIsBgmCpuBudgetAvailable();
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

}
