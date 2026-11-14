#include "prx/libSceSystemService/include/SystemService.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#endif
#include "prx/libc/include/Shutdown.hpp"
#include "SceTypes.hpp"
#include "LoadExec.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostExecutable.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "prx/libSceSystemService/HostUrlBackends.hpp"
#include "prx/libSceSystemService/DateTimeLocale.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"

namespace {

HostBrowserLauncher g_defaultUrlLauncher;
HostMessageBoxConfirmation g_defaultUrlConfirmation;
std::atomic<IUrlLauncher*> g_urlLauncher{&g_defaultUrlLauncher};
std::atomic<IUrlConfirmation*> g_urlConfirmation{&g_defaultUrlConfirmation};
SteadyUrlClock g_steadyUrlClock;
OpenUrlGate g_urlGate(g_steadyUrlClock);

int OpenWebBrowserUrl(const char* uri) {
    const OpenUrlMode mode = OpenUrlModeFromEnvironment("sceSystemServiceLaunchWebBrowser");
    if (mode == OpenUrlMode::Never) return SCE_KERNEL_ERROR_EOPNOTSUPP;
    const OpenUrlGateOutcome outcome = g_urlGate.Run(mode, uri, *g_urlLauncher.load(), *g_urlConfirmation.load());
    return outcome == OpenUrlGateOutcome::Launched ? SYSTEM_SERVICE_OK : SCE_KERNEL_ERROR_EOPNOTSUPP;
}

struct MeasuredParameter {
    int id;
    int value;
};

constexpr MeasuredParameter RegistryParameters[] = {
    {SYSTEM_SERVICE_PARAM_ID_100, 0}, {101, 1}, {102, 2}, {103, 100}, {104, 0x20202}, {105, 0}, {106, 0}, {107, 100}, {108, 0},
    {109, 100}, {110, 0x20202}, {111, 0}, {200, 0}, {201, 0}, {202, 0}, {203, 0}, {204, 0}, {205, 0}, {206, 0}, {207, 0},
    {208, 0}, {209, 1}, {210, 71}, {211, 0}, {212, 0}, {213, 0}, {300, 0},
};

struct ConsoleTimeZone {
    int minutes;
    bool summer;
};

ConsoleTimeZone HostTimeZone() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    const bool converted = localtime_s(&local, &now) == 0;
#else
    const bool converted = localtime_r(&now, &local) != nullptr;
#endif
    if (!converted) throw std::runtime_error("sceSystemServiceParamGetInt: the host local time is unavailable");
    const std::chrono::sys_days date = std::chrono::year{local.tm_year + 1900} / std::chrono::month{static_cast<unsigned>(local.tm_mon + 1)} / std::chrono::day{static_cast<unsigned>(local.tm_mday)};
    const auto localSeconds = date.time_since_epoch() + std::chrono::hours{local.tm_hour} + std::chrono::minutes{local.tm_min} + std::chrono::seconds{local.tm_sec};
    const auto offset = std::chrono::duration_cast<std::chrono::minutes>(localSeconds - std::chrono::seconds{now});
    const bool summer = local.tm_isdst > 0;
#if defined(_WIN32)
    long daylightBias = 0;
    _get_dstbias(&daylightBias);
    const int summerMinutes = summer ? static_cast<int>(-daylightBias / 60) : 0;
#else
    const int summerMinutes = summer ? 60 : 0;
#endif
    return {static_cast<int>(offset.count()) - summerMinutes, summer};
}

std::mutex appSuspendBlockMutex;
int appSuspendBlocks = 0;

int WriteStatus(bool* status, bool value, const char* function) {
    if (status == nullptr) NotImplemented_nid_no_patch((std::string(function) + " with a null output").c_str());
    *status = value;
    return SYSTEM_SERVICE_OK;
}

int WriteStatusOrRefuse(bool* status, bool value, int nullOutputError) {
    if (status == nullptr) return nullOutputError;
    if (!GuestCopyOut_nid_no_patch(status, &value, sizeof(value))) return nullOutputError;
    return SYSTEM_SERVICE_OK;
}

constexpr char MainExecutable[] = "/app0/eboot.bin";
constexpr char ApplicationRoot[] = "/app0/";
constexpr char MainExecutableVariable[] = "APS5_EBOOT";
#if defined(_WIN32)
constexpr wchar_t MainExecutableVariableWide[] = L"APS5_EBOOT";
#endif

[[noreturn]] void UnconvertedExecutable(const std::string& message) {
    throw std::runtime_error("sceSystemServiceLoadExec: " + message);
}

void RequireExecutableBeside(const std::filesystem::path& directory, const std::filesystem::path& target, const char* guestPath) {
    if (target.lexically_normal().parent_path() != directory.lexically_normal()) {
        UnconvertedExecutable(std::string("an executable outside /app0 (") + guestPath + ")");
    }
}

#if defined(_WIN32)
struct HostLaunch {
    std::wstring executable;
    std::wstring commandLine;
};

HostLaunch PrepareLaunch(const char* path, const char* const* arguments) {
    const std::wstring own = HostExecutable::Own("sceSystemServiceLoadExec").wstring();
    HostLaunch launch;
    const std::filesystem::path ownPath(own);
    if (std::strcmp(path, MainExecutable) == 0) {
        std::wstring main(32768, L'\0');
        const DWORD mainLength = GetEnvironmentVariableW(MainExecutableVariableWide, main.data(), static_cast<DWORD>(main.size()));
        launch.executable = mainLength > 0 && mainLength < main.size() ? main.substr(0, mainLength) : own;
    } else {
        const auto target = ownPath.parent_path() / (HostExecutable::Wide(HostExecutable::ConvertedName(path, UnconvertedExecutable), "sceSystemServiceLoadExec") + ownPath.extension().wstring());
        RequireExecutableBeside(ownPath.parent_path(), target, path);
        if (!std::filesystem::is_regular_file(target)) {
            UnconvertedExecutable(std::string(path) + " without its converted executable; relink with --executable " + (path + std::size(ApplicationRoot) - 1));
        }
        launch.executable = target.wstring();
    }
    if (GetEnvironmentVariableW(MainExecutableVariableWide, nullptr, 0) == 0 && !SetEnvironmentVariableW(MainExecutableVariableWide, own.c_str())) {
        throw std::runtime_error(std::string("sceSystemServiceLoadExec: cannot set ") + MainExecutableVariable);
    }
    launch.commandLine = L"\"" + launch.executable + L"\"";
    for (std::size_t index = 0; arguments != nullptr && arguments[index] != nullptr; ++index) {
        launch.commandLine += L" " + HostExecutable::QuotedArgument(HostExecutable::Wide(arguments[index], "sceSystemServiceLoadExec"));
    }
    return launch;
}

void StartHostExecutable(HostLaunch& launch) {
    LogFileRelease_nid_no_patch();
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(launch.executable.c_str(), launch.commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process)) {
        throw std::runtime_error("sceSystemServiceLoadExec: CreateProcessW failed with error " + std::to_string(GetLastError()));
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}
#elif defined(__linux__) || defined(__APPLE__)
struct HostLaunch {
    std::string executable;
    std::vector<std::string> arguments;
};

HostLaunch PrepareLaunch(const char* path, const char* const* arguments) {
    const auto own = HostExecutable::Own("sceSystemServiceLoadExec");
    HostLaunch launch;
    if (std::strcmp(path, MainExecutable) == 0) {
        const char* main = std::getenv(MainExecutableVariable);
        launch.executable = main != nullptr && *main != '\0' ? main : own.string();
    } else {
        const auto target = own.parent_path() / (HostExecutable::ConvertedName(path, UnconvertedExecutable) + own.extension().string());
        RequireExecutableBeside(own.parent_path(), target, path);
        if (!std::filesystem::is_regular_file(target)) {
            UnconvertedExecutable(std::string(path) + " without its converted executable; relink with --executable " + (path + std::size(ApplicationRoot) - 1));
        }
        launch.executable = target.string();
    }
    if (setenv(MainExecutableVariable, own.c_str(), 0) != 0) {
        throw std::runtime_error(std::string("sceSystemServiceLoadExec: cannot set ") + MainExecutableVariable);
    }
    launch.arguments.push_back(launch.executable);
    for (std::size_t index = 0; arguments != nullptr && arguments[index] != nullptr; ++index) launch.arguments.emplace_back(arguments[index]);
    return launch;
}

void StartHostExecutable(HostLaunch& launch) {
    std::vector<char*> argv;
    for (auto& argument : launch.arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    if (const int host = HostStandardError_nid_no_patch(); host >= 0 && ::dup2(host, 2) != 2) {
        throw std::runtime_error("sceSystemServiceLoadExec: cannot give the host standard error back to descriptor 2: " + std::string(std::strerror(errno)));
    }
    LogFileRelease_nid_no_patch();
    ::execv(launch.executable.c_str(), argv.data());
    throw std::runtime_error("sceSystemServiceLoadExec: execv failed: " + std::string(std::strerror(errno)));
}
#else
struct HostLaunch {};

HostLaunch PrepareLaunch(const char*, const char* const*) {
    UnconvertedExecutable("starting an executable on this host");
}

void StartHostExecutable(HostLaunch&) {}
#endif

constexpr std::size_t TitleIdBufferSize = 10;

std::string RunningTitleId() {
    const AppTitleId titleId = GetAppTitleId_nid_postfix();
    return std::string(titleId.value);
}

void CopyRunningTitleId(const std::string& caller, bool running, char* destination) {
    if (!running) throw std::runtime_error(caller + ": application other than the running title");
    if (destination == nullptr) throw std::runtime_error(caller + ": null title ID buffer");
    const std::string titleId = RunningTitleId();
    if (titleId.size() >= TitleIdBufferSize) throw std::runtime_error(caller + ": title ID that does not fit 10 bytes");
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, titleId.c_str(), titleId.size() + 1);
    errno = savedErrno;
    if (!copied) throw std::runtime_error(caller + ": unwritable title ID buffer");
}

bool WellFormedTitleId(const char* titleId) {
    return titleId != nullptr && std::strlen(titleId) == SYSTEM_SERVICE_TITLE_ID_LENGTH;
}

int RunningAppId(const std::string& caller, const char* titleId) {
    if (!WellFormedTitleId(titleId)) return SYSTEM_SERVICE_INVALID_TITLE_APP_ID;
    if (RunningTitleId() != titleId) NotImplemented_nid_no_patch((caller + ": title other than the running title").c_str());
    return SYSTEM_SERVICE_RUNNING_APP_ID;
}

template<typename TValue>
int AnswerForRunningApp(const std::string& caller, std::int32_t appId, TValue* destination, std::type_identity_t<TValue> value, int nullOutputResult) {
    if (destination == nullptr) return nullOutputResult;
    if (appId != SYSTEM_SERVICE_RUNNING_APP_ID) throw std::runtime_error(caller + ": application other than the running title");
    *destination = value;
    return SYSTEM_SERVICE_OK;
}

int ListRunningBigApp(const std::string& caller, std::int32_t* appIds, std::uint32_t capacity, std::uint32_t* count) {
    if (appIds == nullptr || count == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
    if (capacity == 0) throw std::runtime_error(caller + ": a list with no room for the running title");
    std::int32_t currentId = 0;
    std::uint32_t currentCount = 0;
    const int savedErrno = errno;
    const bool valid = GuestCopyIn_nid_no_patch(&currentId, appIds, sizeof(currentId)) &&
                       GuestCopyOut_nid_no_patch(appIds, &currentId, sizeof(currentId)) &&
                       GuestCopyIn_nid_no_patch(&currentCount, count, sizeof(currentCount)) &&
                       GuestCopyOut_nid_no_patch(count, &currentCount, sizeof(currentCount));
    if (!valid) {
        errno = savedErrno;
        return LNC_UTIL_ERROR_INVALID_ARGUMENT;
    }
    const std::int32_t runningId = SYSTEM_SERVICE_RUNNING_APP_ID;
    const std::uint32_t one = 1;
    GuestCopyOut_nid_no_patch(appIds, &runningId, sizeof(runningId));
    GuestCopyOut_nid_no_patch(count, &one, sizeof(one));
    errno = savedErrno;
    return SYSTEM_SERVICE_OK;
}

constexpr std::uint32_t GpuLoadEmulationModeOff = 0;
constexpr std::uint32_t GpuLoadEmulationModeNormal = 1;

std::atomic<std::uint32_t> GpuLoadEmulationMode{GpuLoadEmulationModeNormal};

int SetGpuLoadEmulationMode(const char* caller, std::uint32_t mode) {
    if (mode != GpuLoadEmulationModeOff && mode != GpuLoadEmulationModeNormal) {
        NotImplemented_nid_no_patch((std::string(caller) + ": a mode other than 0 and 1").c_str());
    }
    GpuLoadEmulationMode.store(mode, std::memory_order_relaxed);
    return SYSTEM_SERVICE_OK;
}

int GetGpuLoadEmulationModeOfApp(const char* caller, std::int32_t appId) {
    if (appId != SYSTEM_SERVICE_RUNNING_APP_ID) NotImplemented_nid_no_patch((std::string(caller) + ": application other than the running title").c_str());
    return static_cast<int>(GpuLoadEmulationMode.load(std::memory_order_relaxed));
}

std::atomic<bool> SuspendConfirmationDialogEnabled{true};

}

extern "C" {

int APS5_VABI sceSystemServiceEnableSuspendConfirmationDialog() {
    SuspendConfirmationDialogEnabled.store(true, std::memory_order_relaxed);
    return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceDisableSuspendConfirmationDialog() {
    SuspendConfirmationDialogEnabled.store(false, std::memory_order_relaxed);
    return SYSTEM_SERVICE_OK;
}

bool APS5_VABI sceShellCoreUtilGetSuspendConfirmationDialogFlag(std::int32_t appId) {
    if (appId != SYSTEM_SERVICE_RUNNING_APP_ID) NotImplemented_nid_no_patch("sceShellCoreUtilGetSuspendConfirmationDialogFlag: application other than the running title");
    return SuspendConfirmationDialogEnabled.load(std::memory_order_relaxed);
}

int APS5_VABI sceSystemServiceSetGpuLoadEmulationMode(std::uint32_t mode) {
    return SetGpuLoadEmulationMode("sceSystemServiceSetGpuLoadEmulationMode", mode);
}

int APS5_VABI sceSystemServiceGetGpuLoadEmulationMode() {
    return static_cast<int>(GpuLoadEmulationMode.load(std::memory_order_relaxed));
}

int APS5_VABI sceShellCoreUtilSetGpuLoadEmulationMode(std::uint32_t mode) {
    return SetGpuLoadEmulationMode("sceShellCoreUtilSetGpuLoadEmulationMode", mode);
}

int APS5_VABI sceShellCoreUtilGetGpuLoadEmulationMode() {
    return static_cast<int>(GpuLoadEmulationMode.load(std::memory_order_relaxed));
}

int APS5_VABI sceShellCoreUtilSetGpuLoadEmulationModeByAppId(std::int32_t appId, std::uint32_t mode) {
    if (appId != SYSTEM_SERVICE_RUNNING_APP_ID) NotImplemented_nid_no_patch("sceShellCoreUtilSetGpuLoadEmulationModeByAppId: application other than the running title");
    return SetGpuLoadEmulationMode("sceShellCoreUtilSetGpuLoadEmulationModeByAppId", mode);
}

int APS5_VABI sceShellCoreUtilGetGpuLoadEmulationModeByAppId(std::int32_t appId) {
    return GetGpuLoadEmulationModeOfApp("sceShellCoreUtilGetGpuLoadEmulationModeByAppId", appId);
}

int APS5_VABI sceSystemServiceLoadExec(const char* path, const char* const* arguments) {
    if (!path || !*path) return SYSTEM_SERVICE_ERROR_PARAMETER;
    if (std::strcmp(path, "exit") == 0) LibcExitWithoutHandlers_nid_no_patch(0);
    auto launch = PrepareLaunch(path, arguments);
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    LoadConvertedExecutable_nid_no_patch(std::filesystem::path(launch.executable), arguments);
#endif
    LibcStartExit_nid_postfix();
    LibcRunShutdown_nid_postfix();
    std::fflush(nullptr);
    StartHostExecutable(launch);
    LibcTerminate_nid_no_patch(0);
}

int APS5_VABI sceSystemServiceDisableNoticeScreenSkipFlagAutoSet(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceGetDisplaySafeAreaInfo(SystemServiceDisplaySafeAreaInfo* info) {
 if (info == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 SystemServiceDisplaySafeAreaInfo localInfo{};
 localInfo.ratio = 1.0f;
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
 errno = savedErrno;
 if (!copied) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceShowDisplaySafeAreaSettings(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceGetHdrToneMapLuminance(SystemServiceHdrToneMapLuminance* luminance) {
 if (luminance == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 const auto hdr = AgcDriver::CurrentHostHdr();
 luminance->max_tone_map_luminance = AgcDriver::ToneMapPeakNits(hdr);
 luminance->max_full_frame_tone_map_luminance = hdr.available ? hdr.display.maxFullFrameLuminance : AgcDriver::SdrToneMapLuminanceNits;
 luminance->min_tone_map_luminance = hdr.available ? hdr.display.minLuminance : 0.0f;
 AgcDriverNoteReportedToneMapPeak_nid_postfix(luminance->max_tone_map_luminance);
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceGetNoticeScreenSkipFlag(bool* value) {
 if (value == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 *value = false;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceGetStatus(SystemServiceStatus* status) {
 if (status == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 *status = SystemServiceStatus{};
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceLncUtilBlockAppSuspend(const char* reason) {
    if (reason == nullptr) NotImplemented_nid_no_patch("sceLncUtilBlockAppSuspend with a null reason");
    std::lock_guard lock(appSuspendBlockMutex);
    ++appSuspendBlocks;
    return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceLncUtilUnblockAppSuspend(const char* reason) {
    std::lock_guard lock(appSuspendBlockMutex);
    if (appSuspendBlocks == 0) return SCE_KERNEL_ERROR_EBUSY;
    if (reason == nullptr) NotImplemented_nid_no_patch("sceLncUtilUnblockAppSuspend with a null reason while a block is held");
    --appSuspendBlocks;
    return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceLncUtilEnableSuspendNotification() {
    return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceLncUtilDisableSuspendNotification() {
    return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceEnableSuspendNotification() {
    return sceLncUtilEnableSuspendNotification();
}

int APS5_VABI sceSystemServiceDisableSuspendNotification() {
    return sceLncUtilDisableSuspendNotification();
}

int APS5_VABI sceShellCoreUtilGetRemotePlayStatus(bool* active) {
    return WriteStatusOrRefuse(active, false, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
}

int APS5_VABI sceSystemServiceGetRemoteplayStatus(bool* active) {
    return sceShellCoreUtilGetRemotePlayStatus(active);
}

int APS5_VABI sceShellCoreUtilGetSharePlayStatus(bool* active) {
    return WriteStatusOrRefuse(active, false, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
}

bool APS5_VABI sceShellCoreUtilIsGameLiveStreamingOnAir() {
    return false;
}

bool APS5_VABI sceSystemServiceIsGameLiveStreamingOnAir() {
    return sceShellCoreUtilIsGameLiveStreamingOnAir();
}

int APS5_VABI sceShellCoreUtilGetGameLiveStreamingStatus(bool* active) {
    return WriteStatusOrRefuse(active, sceShellCoreUtilIsGameLiveStreamingOnAir(), SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
}

bool APS5_VABI sceLncUtilIsShellUiFgAndGameBgCpuMode() {
    return false;
}

bool APS5_VABI sceSystemServiceIsShellUiFgAndGameBgCpuMode() {
    return sceLncUtilIsShellUiFgAndGameBgCpuMode();
}

bool APS5_VABI sceShellCoreUtilIsImposeScreenOverlaid() {
    return false;
}

bool APS5_VABI sceShellCoreUtilIsScreenSaverOn() {
    return false;
}

bool APS5_VABI sceSystemServiceIsScreenSaverOn() {
    return sceShellCoreUtilIsScreenSaverOn();
}

bool APS5_VABI sceSystemStateMgrIsShellUIShutdownInProgress() {
    return false;
}

int APS5_VABI sceShellCoreUtilGetSocialScreenStatus(bool* active) {
    return WriteStatus(active, false, __func__);
}

int APS5_VABI sceShellCoreUtilIsEyeToEyeDistanceAdjusted(std::uint32_t userId, std::uint8_t* adjusted) {
    if (adjusted == nullptr) NotImplemented_nid_no_patch("sceShellCoreUtilIsEyeToEyeDistanceAdjusted with a null output");
    if (userId != static_cast<std::uint32_t>(USER_SERVICE_INITIAL_USER_ID)) {
        NotImplemented_nid_no_patch("sceShellCoreUtilIsEyeToEyeDistanceAdjusted for a user other than the logged-in user");
    }
    const std::uint8_t result = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(adjusted, &result, sizeof(result));
    errno = savedErrno;
    if (!copied) {
        NotImplemented_nid_no_patch("sceShellCoreUtilIsEyeToEyeDistanceAdjusted with an unwritable output");
    }
    return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceIsEyeToEyeDistanceAdjusted(std::uint32_t userId, std::uint8_t* adjusted) {
    if (adjusted == nullptr) return SYSTEM_SERVICE_ERROR_PARAMETER;
    std::uint8_t result = 0;
    const int status = sceShellCoreUtilIsEyeToEyeDistanceAdjusted(userId, &result);
    if (status != SYSTEM_SERVICE_OK) return status;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(adjusted, &result, sizeof(result));
    errno = savedErrno;
    return copied ? SYSTEM_SERVICE_OK : SYSTEM_SERVICE_ERROR_PARAMETER;
}

bool APS5_VABI sceSystemStateMgrIsBdDriveReady() {
    return false;
}

bool APS5_VABI sceShellCoreUtilIsUsbMassStorageMounted(std::uint32_t) {
    return false;
}

bool APS5_VABI sceShellCoreUtilIsExternalStorageAppMoveInProgress() {
    return false;
}

int APS5_VABI sceLncUtilIsPs2Emu(bool* emulated) {
    return WriteStatusOrRefuse(emulated, false, LNC_UTIL_ERROR_INVALID_ARGUMENT);
}

int APS5_VABI sceSystemServiceHideSplashScreen(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceParamGetInt(int paramId, int* value) {
 if (value == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 switch (paramId) {
  case SYSTEM_SERVICE_PARAM_ID_LANG:
  case SYSTEM_SERVICE_PARAM_ID_400: *value = SystemServiceConsoleLanguage("sceSystemServiceParamGetInt"); break;
  case SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT: *value = SystemServiceDateTime::ConsoleDateFormat("sceSystemServiceParamGetInt"); break;
  case SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT: *value = SystemServiceDateTime::ConsoleTimeFormat("sceSystemServiceParamGetInt"); break;
  case SYSTEM_SERVICE_PARAM_ID_TIME_ZONE: *value = HostTimeZone().minutes; break;
  case SYSTEM_SERVICE_PARAM_ID_SUMMERTIME: *value = HostTimeZone().summer ? 1 : 0; break;
  default: {
   const auto measured = std::ranges::find(RegistryParameters, paramId, &MeasuredParameter::id);
   if (measured == std::ranges::end(RegistryParameters)) return SYSTEM_SERVICE_ERROR_PARAMETER;
   *value = measured->value;
  }
 }
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceParamGetString(int paramId, char* buf, size_t bufSize) {
 if (paramId != SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME || buf == nullptr || bufSize < SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 constexpr char SystemName[] = "PS5";
 static constexpr char Zeros[256] = {};
 for (std::size_t offset = 0; offset < bufSize; offset += sizeof(Zeros)) {
  if (!GuestCopyOut_nid_no_patch(buf + offset, Zeros, std::min(sizeof(Zeros), bufSize - offset))) return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 if (!GuestCopyOut_nid_no_patch(buf, SystemName, sizeof(SystemName))) return SYSTEM_SERVICE_ERROR_PARAMETER;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServicePowerTick(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceReceiveEvent(SystemServiceEvent* event) {
 (void)event;
 return SYSTEM_SERVICE_ERROR_NO_EVENT;
}

int APS5_VABI sceSystemServiceReportAbnormalTermination(const void* info) {
 (void)info;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceSetNoticeScreenSkipFlag(void) {
 return SYSTEM_SERVICE_OK;
}

void APS5_VABI sceSystemServiceInitializePlayerDialogParam(SystemServicePlayerDialogParam* param) {
 if (param == nullptr) APS5_INVALID_ARG_EX;
 *param = SystemServicePlayerDialogParam{};
 param->size = sizeof(SystemServicePlayerDialogParam);
 param->user_id = -1;
}

int APS5_VABI sceSystemServiceDisableMediaPlay() {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceReenableMediaPlay() {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceLaunchPlayerDialog(const void* param) {
 if (param == nullptr) return SYSTEM_SERVICE_ERROR_PARAMETER;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceLaunchWebBrowser(const char* uri) {
    if (!uri) return static_cast<int>(0x80a10003);
    std::size_t length = 0;
    while (length < 6016 && uri[length] != 0) ++length;
    if (length > 6015) return static_cast<int>(0x80a10007);
    if (std::strncmp(uri, "http:", 5) != 0 && std::strncmp(uri, "https:", 6) != 0 && std::strncmp(uri, "psno:", 5) != 0) return static_cast<int>(0x80a10008);
    return OpenWebBrowserUrl(uri);
}

extern "C" void SystemServiceSetUrlHandlers_nid_no_patch(IUrlLauncher* launcher, IUrlConfirmation* confirmation, IUrlClock* clock) {
    g_urlLauncher = launcher != nullptr ? launcher : &g_defaultUrlLauncher;
    g_urlConfirmation = confirmation != nullptr ? confirmation : &g_defaultUrlConfirmation;
    g_urlGate.Reset(clock != nullptr ? *clock : static_cast<IUrlClock&>(g_steadyUrlClock));
}

int APS5_VABI sceSystemServiceLaunchApp(const char* titleId, const char* const* arguments, const void* param) {
 (void)titleId;
 (void)arguments;
 (void)param;
 NotImplemented_nid_no_patch("sceSystemServiceLaunchApp: launching another application");
}

int APS5_VABI sceSystemServiceDisableMusicPlayer(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceOpenChallengeActivity(void) {
 return SYSTEM_SERVICE_OK;
}

static_assert(sizeof(SystemServiceTournamentOccurrenceParam) == 32);
static_assert(offsetof(SystemServiceTournamentOccurrenceParam, occurrenceId) == 8);
static_assert(offsetof(SystemServiceTournamentOccurrenceParam, userId) == 16);
static_assert(offsetof(SystemServiceTournamentOccurrenceParam, context) == 24);

int APS5_VABI sceSystemServiceOpenTournamentOccurrence(const SystemServiceTournamentOccurrenceParam* param) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceAddLocalProcessForPsmKit(const char*, const char* const*, const void*) {
 NotImplemented_nid_no_patch("sceSystemServiceAddLocalProcessForPsmKit: starting another process");
}

int APS5_VABI sceSystemServiceKillLocalProcessForPsmKit(int) {
 NotImplemented_nid_no_patch("sceSystemServiceKillLocalProcessForPsmKit: stopping another process");
}

int APS5_VABI sceSystemServiceReenableMusicPlayer(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceShowControllerSettings(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceGetAppIdOfRunningBigApp(void) {
 return SYSTEM_SERVICE_RUNNING_APP_ID;
}

int APS5_VABI sceSystemServiceGetAppId(const char* titleId) {
 return RunningAppId("sceSystemServiceGetAppId", titleId);
}

int APS5_VABI sceSystemServiceGetAppTitleId(int appId, char* titleId) {
 CopyRunningTitleId("sceSystemServiceGetAppTitleId", appId == SYSTEM_SERVICE_RUNNING_APP_ID, titleId);
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceLncUtilGetAppTitleId(std::int32_t appId, char* titleId) {
 return sceSystemServiceGetAppTitleId(appId, titleId);
}

int APS5_VABI sceLncUtilGetAppId(const char* titleId) {
 return sceSystemServiceGetAppId(titleId);
}

int APS5_VABI sceLncUtilIsAppLaunched(const char* titleId, bool* launched) {
 if (!WellFormedTitleId(titleId) || launched == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
 static_cast<void>(sceSystemServiceGetAppId(titleId));
 *launched = true;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceLncUtilGetAppIdOfRunningBigApp() {
 return sceSystemServiceGetAppIdOfRunningBigApp();
}

int APS5_VABI sceLncUtilGetAppIdListOfBigApp(std::int32_t* appIds, std::uint32_t capacity, std::uint32_t* count) {
 return ListRunningBigApp("sceLncUtilGetAppIdListOfBigApp", appIds, capacity, count);
}

int APS5_VABI sceSystemServiceGetAppIdListOfBigApp(std::int32_t* appIds, std::uint32_t capacity, std::uint32_t* count) {
 return sceLncUtilGetAppIdListOfBigApp(appIds, capacity, count);
}

int APS5_VABI sceLncUtilGetAppLaunchedUser(std::int32_t appId, std::int32_t* userId) {
 return AnswerForRunningApp("sceLncUtilGetAppLaunchedUser", appId, userId, USER_SERVICE_INITIAL_USER_ID, LNC_UTIL_ERROR_INVALID_ARGUMENT);
}

int APS5_VABI sceSystemServiceGetAppLaunchedUser(std::int32_t appId, std::int32_t* userId) {
 return sceLncUtilGetAppLaunchedUser(appId, userId);
}

int APS5_VABI sceShellCoreUtilGetUserFocus(std::int32_t appId, std::int32_t* userId) {
 if (userId == nullptr) return SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT;
 return sceLncUtilGetAppLaunchedUser(appId, userId);
}

int APS5_VABI sceLncUtilIsAppSuspended(std::int32_t appId, bool* suspended) {
 return AnswerForRunningApp("sceLncUtilIsAppSuspended", appId, suspended, false, LNC_UTIL_ERROR_INVALID_ARGUMENT);
}

int APS5_VABI sceSystemServiceIsAppSuspended(std::int32_t appId, bool* suspended) {
 return sceLncUtilIsAppSuspended(appId, suspended);
}

int APS5_VABI sceLncUtilLoadExec(const char* path, const char* const* argv) {
 if (path == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
 if (path[0] == '\0') throw std::runtime_error("sceLncUtilLoadExec: empty path");
 return sceSystemServiceLoadExec(path, argv);
}

int APS5_VABI sceSystemServiceKillApp(int appId, int how, int reason, int coreDump) {
 if (appId != SYSTEM_SERVICE_RUNNING_APP_ID) {
  throw std::runtime_error("sceSystemServiceKillApp: application other than the running title");
 }
 if (how != -1 || reason != 0 || coreDump != 0) {
  throw std::runtime_error("sceSystemServiceKillApp: arguments other than -1, 0 and 0");
 }
 LibcExit_nid_no_patch(0);
}

int APS5_VABI sceLncUtilKillApp(uint32_t appId) {
 if (appId != static_cast<uint32_t>(SYSTEM_SERVICE_RUNNING_APP_ID)) {
  throw std::runtime_error("sceLncUtilKillApp: application other than the running title");
 }
 LibcExit_nid_no_patch(0);
}

int APS5_VABI sceLncUtilKillAppWithReason(uint32_t appId, int reason) {
 if (appId != static_cast<uint32_t>(SYSTEM_SERVICE_RUNNING_APP_ID)) {
  throw std::runtime_error("sceLncUtilKillAppWithReason: application other than the running title");
 }
 if (reason != 0) {
  throw std::runtime_error("sceLncUtilKillAppWithReason: a reason other than 0");
 }
 LibcExit_nid_no_patch(0);
}

int APS5_VABI sceSystemServiceLaunchSystemDeeplink(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceShellCoreUtilLaunchByUriParamInit(void* param) {
    if (param == nullptr) return;
    const std::uint32_t fields[2] = {0, 0xffffffff};
    std::memcpy(param, fields, sizeof(fields));
}

int APS5_VABI sceSystemStateMgrTickVideoPlayback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceTelemetrySetData2WithUserId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilStartUmmDictation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetSpotifyTitleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceGetAppStatusListContainsDaemon(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceSwitchToVRApp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceSwitchToNonVRApp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetSpmemAllocatorConfigValue(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrInitialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrTerminate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrCreateLocalSandbox(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrDestroyLocalSandbox(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrSwitchLocalSandbox(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrGetCurrentLocalSandboxId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrGetLocalSandboxIdList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrGetLocalSandboxInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrSetLocalSandboxInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCsbMgrIssueStreamingSandboxInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceReportSequenceHandlerRequestReport(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceReportSequenceHandlerCancelReport(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceReportSequenceHandlerExistsEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceReportSequenceHandlerUpdateEventCache(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceReportSequenceHandlerGetEventCache(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilRegisterCallEntries(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilGetSpmemConfigJson(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilExportSpmemConfigJson(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLncUtilImportSpmemConfigJson(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilExitAppWithValue(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNpDebugSpecialNotification(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsDddTemperatureDanger(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsM2StorageTemperatureDanger(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetAvailableSizeM2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotifyConnectAccountStatusChanged(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsInternalCronosUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilChangePbtcMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilTelemetryCacheData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilMicMuteKeyPressed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSwitchMicMute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotifyStartEventToUniversalMicMute(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsLotusMicActivated(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilResetAllNoticeScreenSkipFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilResetNoticeScreenSkipFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetNoticeScreenSkipFlagByAppId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPasscodeInputType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPasscodeDisplayInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilNotifySystemResumeTelemetryInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilUpdateTelemetryToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetVRBaseMatRectangles(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilSetVRBaseMatRectangles(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsLaunchBlockDialog(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetConsoleToken(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetDeviceIdByDeviceUniqueId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetDeviceUniqueIdByDeviceId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetStreamingClientInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilRequestCffUpdate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCffConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilRemoveCffConfigFile(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDebugGetNumberOfCffItems(std::uint32_t* count) {
    if (count == nullptr) return static_cast<int>(0x80e30005);
    *count = 50;
    return 0;
}

int APS5_VABI sceShellCoreUtilDebugGetCffInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilDebugSetCffStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetPeripheralConnectionSessionId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetCdlgEnabledFreezeRendering(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilGetNpTitleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsCronosInGameCommerceEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilLemSettingGet(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilLemSettingSet(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilLemSettingGetList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilLemSettingReset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilIsEChassisVariant(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemStateMgrGetRebootCause(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpSequenceHandlerUpdateEventCache(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpSequenceHandlerGetEventCache(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpSequenceHandlerGetWorkingEventSummary(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpSequenceHandlerGetCoredumpStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpSequenceHandlerGetCoredumpStatusByAppId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpSequenceHandlerCancelCoredump(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemServiceIsBgmPlaying(void) {
    return 0;
}

int APS5_VABI sceShellCoreUtilIsBgmPlaying(void) {
    return 0;
}

APS5_EXPORT("M+KwxF0amiw", sceSystemServiceUnknown_M_KwxF0amiw);
int APS5_VABI sceSystemServiceUnknown_M_KwxF0amiw(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
