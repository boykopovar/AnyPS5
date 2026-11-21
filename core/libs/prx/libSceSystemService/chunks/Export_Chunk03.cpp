extern "C" {
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

int WriteStatusOrRefuse(bool* status, bool value, int nullOutputError) {
    if (status == nullptr) return nullOutputError;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return nullOutputError;
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
    std::wstring recorded(32768, L'\0');
    const DWORD recordedLength = GetEnvironmentVariableW(MainExecutableVariableWide, recorded.data(), static_cast<DWORD>(recorded.size()));
    const std::filesystem::path main = recordedLength > 0 && recordedLength < recorded.size() ? std::filesystem::path(recorded.substr(0, recordedLength)) : ownPath;
    if (std::strcmp(path, MainExecutable) == 0) {
        launch.executable = main.wstring();
    } else {
        const auto target = ownPath.parent_path() / (HostExecutable::Wide(HostExecutable::ConvertedName(path, UnconvertedExecutable), "sceSystemServiceLoadExec") + main.extension().wstring());
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
    const char* recorded = std::getenv(MainExecutableVariable);
    const std::filesystem::path main = recorded != nullptr && *recorded != '\0' ? std::filesystem::path(recorded) : own;
    HostLaunch launch;
    if (std::strcmp(path, MainExecutable) == 0) {
        launch.executable = main.string();
    } else {
        const auto target = own.parent_path() / (HostExecutable::ConvertedName(path, UnconvertedExecutable) + main.extension().string());
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
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &value, sizeof(value));
    errno = savedErrno;
    return copied ? SYSTEM_SERVICE_OK : nullOutputResult;
}

int AnswerRunningAppCategoryType(const char* caller, std::int32_t appId, std::uint32_t* categoryType, int nullOutputResult) {
    if (categoryType == nullptr) return nullOutputResult;
    if (appId != SYSTEM_SERVICE_RUNNING_APP_ID) throw std::runtime_error(std::string(caller) + ": application other than the running title");
    std::uint32_t value = 0;
    if (!GetAppCategoryType_nid_no_patch(&value)) {
        NotImplemented_nid_no_patch((std::string(caller) + ": a title whose param.json has no applicationCategoryType").c_str());
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(categoryType, &value, sizeof(value));
    errno = savedErrno;
    return copied ? SYSTEM_SERVICE_OK : nullOutputResult;
}

constexpr std::uint32_t BigAppListCapacityLimit = 4;

int ListRunningBigApp(const std::string& caller, std::int32_t* appIds, std::uint32_t capacity, std::uint32_t* count) {
    static_cast<void>(caller);
    if (appIds == nullptr || count == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
    if (capacity >= BigAppListCapacityLimit) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
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
    if (capacity == 0) {
        const std::uint32_t none = 0;
        GuestCopyOut_nid_no_patch(count, &none, sizeof(none));
        errno = savedErrno;
        return SYSTEM_SERVICE_OK;
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

std::atomic<bool> AccessibilityZoomLocked{false};
std::atomic<bool> NoticeScreenSkipFlag{false};

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
