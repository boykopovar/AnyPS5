#include "prx/libSceSystemService/include/SystemService.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <filesystem>
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
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"

namespace {

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

constexpr char MainExecutable[] = "/app0/eboot.bin";
constexpr char ApplicationRoot[] = "/app0/";
constexpr char MainExecutableVariable[] = "APS5_EBOOT";
#if defined(_WIN32)
constexpr wchar_t MainExecutableVariableWide[] = L"APS5_EBOOT";
#endif

[[noreturn]] void UnconvertedExecutable(const std::string& message) {
    NotImplemented_nid_no_patch(("sceSystemServiceLoadExec: " + message).c_str());
    std::abort();
}

std::string ConvertedExecutableName(const char* path) {
    std::string_view relative(path);
    if (!relative.starts_with(ApplicationRoot)) UnconvertedExecutable(std::string("executables outside /app0 (") + path + ")");
    relative.remove_prefix(std::size(ApplicationRoot) - 1);
    std::string name;
    while (true) {
        const std::size_t separator = relative.find('/');
        const std::string_view component = relative.substr(0, separator);
        if (component.empty() || component == "." || component == "..") UnconvertedExecutable(std::string("a path with an empty, . or .. component (") + path + ")");
        name += (name.empty() ? "" : ".") + std::string(component);
        if (separator == std::string_view::npos) return name;
        relative.remove_prefix(separator + 1);
    }
}

void RequireExecutableBeside(const std::filesystem::path& directory, const std::filesystem::path& target, const char* guestPath) {
    if (target.lexically_normal().parent_path() != directory.lexically_normal()) {
        UnconvertedExecutable(std::string("an executable outside /app0 (") + guestPath + ")");
    }
}

#if defined(_WIN32)
std::wstring Utf16(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) throw std::runtime_error("sceSystemServiceLoadExec: argument is not valid UTF-8");
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::wstring QuotedArgument(const std::wstring& argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) return argument;
    std::wstring quoted = L"\"";
    for (std::size_t index = 0;; ++index) {
        std::size_t backslashes = 0;
        while (index < argument.size() && argument[index] == L'\\') {
            ++backslashes;
            ++index;
        }
        if (index == argument.size()) {
            quoted.append(backslashes * 2, L'\\');
            break;
        }
        quoted.append(argument[index] == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        quoted.push_back(argument[index]);
    }
    quoted.push_back(L'"');
    return quoted;
}

struct HostLaunch {
    std::wstring executable;
    std::wstring commandLine;
};

HostLaunch PrepareLaunch(const char* path, const char* const* arguments) {
    std::wstring own(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, own.data(), static_cast<DWORD>(own.size()));
    if (length == 0 || length >= own.size()) {
        throw std::runtime_error("sceSystemServiceLoadExec: GetModuleFileNameW failed with error " + std::to_string(GetLastError()));
    }
    own.resize(length);
    HostLaunch launch;
    const std::filesystem::path ownPath(own);
    if (std::strcmp(path, MainExecutable) == 0) {
        std::wstring main(32768, L'\0');
        const DWORD mainLength = GetEnvironmentVariableW(MainExecutableVariableWide, main.data(), static_cast<DWORD>(main.size()));
        launch.executable = mainLength > 0 && mainLength < main.size() ? main.substr(0, mainLength) : own;
    } else {
        const auto target = ownPath.parent_path() / (Utf16(ConvertedExecutableName(path)) + ownPath.extension().wstring());
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
        launch.commandLine += L" " + QuotedArgument(Utf16(arguments[index]));
    }
    return launch;
}

void StartHostExecutable(HostLaunch& launch) {
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

std::filesystem::path OwnExecutable() {
    std::error_code error;
#ifdef __APPLE__
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    std::filesystem::path own;
    if (_NSGetExecutablePath(executable.data(), &size) == 0) own = std::filesystem::canonical(executable.data(), error);
    if (own.empty()) throw std::runtime_error("sceSystemServiceLoadExec: cannot locate the executable");
#else
    const auto own = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) throw std::runtime_error("sceSystemServiceLoadExec: cannot read /proc/self/exe: " + error.message());
#endif
    return own;
}

HostLaunch PrepareLaunch(const char* path, const char* const* arguments) {
    const auto own = OwnExecutable();
    HostLaunch launch;
    if (std::strcmp(path, MainExecutable) == 0) {
        const char* main = std::getenv(MainExecutableVariable);
        launch.executable = main != nullptr && *main != '\0' ? main : own.string();
    } else {
        const auto target = own.parent_path() / (ConvertedExecutableName(path) + own.extension().string());
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
    if (!running) NotImplemented_nid_no_patch((caller + ": application other than the running title").c_str());
    if (destination == nullptr) NotImplemented_nid_no_patch((caller + ": null title ID buffer").c_str());
    const std::string titleId = RunningTitleId();
    if (titleId.size() >= TitleIdBufferSize) NotImplemented_nid_no_patch((caller + ": title ID that does not fit 10 bytes").c_str());
    std::memcpy(destination, titleId.c_str(), titleId.size() + 1);
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
    if (appId != SYSTEM_SERVICE_RUNNING_APP_ID) NotImplemented_nid_no_patch((caller + ": application other than the running title").c_str());
    *destination = value;
    return SYSTEM_SERVICE_OK;
}

int ListRunningBigApp(const std::string& caller, std::int32_t* appIds, std::uint32_t capacity, std::uint32_t* count) {
    if (appIds == nullptr || count == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
    if (capacity == 0) NotImplemented_nid_no_patch((caller + ": a list with no room for the running title").c_str());
    appIds[0] = SYSTEM_SERVICE_RUNNING_APP_ID;
    *count = 1;
    return SYSTEM_SERVICE_OK;
}

}

extern "C" {

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
 *info = SystemServiceDisplaySafeAreaInfo{};
 info->ratio = 1.0f;
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
  case SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT: *value = SYSTEM_SERVICE_PARAM_DATE_FORMAT_DDMMYYYY; break;
  case SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT: *value = SYSTEM_SERVICE_PARAM_TIME_FORMAT_24HOUR; break;
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
 std::memset(buf, 0, bufSize);
 std::memcpy(buf, SystemName, sizeof(SystemName));
 buf[SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH - 1] = '\0';
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServicePowerTick(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceReceiveEvent(SystemServiceEvent* event) {
 if (event == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
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
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceSystemServiceLaunchApp(const char* titleId, const char* const* arguments, const void* param) {
 (void)titleId;
 (void)arguments;
 (void)param;
 NotImplemented_nid_no_patch("sceSystemServiceLaunchApp: launching another application");
 return 0;
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
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemServiceKillLocalProcessForPsmKit(int) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
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
 CopyRunningTitleId("sceLncUtilGetAppTitleId", appId == SYSTEM_SERVICE_RUNNING_APP_ID, titleId);
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceLncUtilGetAppId(const char* titleId) {
 return RunningAppId("sceLncUtilGetAppId", titleId);
}

int APS5_VABI sceLncUtilIsAppLaunched(const char* titleId, bool* launched) {
 if (!WellFormedTitleId(titleId) || launched == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
 static_cast<void>(RunningAppId("sceLncUtilIsAppLaunched", titleId));
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
 return ListRunningBigApp("sceSystemServiceGetAppIdListOfBigApp", appIds, capacity, count);
}

int APS5_VABI sceLncUtilGetAppLaunchedUser(std::int32_t appId, std::int32_t* userId) {
 return AnswerForRunningApp("sceLncUtilGetAppLaunchedUser", appId, userId, USER_SERVICE_INITIAL_USER_ID, LNC_UTIL_ERROR_INVALID_ARGUMENT);
}

int APS5_VABI sceSystemServiceGetAppLaunchedUser(std::int32_t appId, std::int32_t* userId) {
 return AnswerForRunningApp("sceSystemServiceGetAppLaunchedUser", appId, userId, USER_SERVICE_INITIAL_USER_ID, LNC_UTIL_ERROR_INVALID_ARGUMENT);
}

int APS5_VABI sceShellCoreUtilGetUserFocus(std::int32_t appId, std::int32_t* userId) {
 return AnswerForRunningApp("sceShellCoreUtilGetUserFocus", appId, userId, USER_SERVICE_INITIAL_USER_ID, SHELL_CORE_UTIL_ERROR_INVALID_ARGUMENT);
}

int APS5_VABI sceLncUtilIsAppSuspended(std::int32_t appId, bool* suspended) {
 return AnswerForRunningApp("sceLncUtilIsAppSuspended", appId, suspended, false, LNC_UTIL_ERROR_INVALID_ARGUMENT);
}

int APS5_VABI sceSystemServiceIsAppSuspended(std::int32_t appId, bool* suspended) {
 return AnswerForRunningApp("sceSystemServiceIsAppSuspended", appId, suspended, false, LNC_UTIL_ERROR_INVALID_ARGUMENT);
}

int APS5_VABI sceLncUtilLoadExec(const char* path, const char* const* argv) {
 if (path == nullptr) return LNC_UTIL_ERROR_INVALID_ARGUMENT;
 if (path[0] == '\0') NotImplemented_nid_no_patch("sceLncUtilLoadExec: empty path");
 return sceSystemServiceLoadExec(path, argv);
}

int APS5_VABI sceSystemServiceKillApp(int appId, int how, int reason, int coreDump) {
 if (appId != SYSTEM_SERVICE_RUNNING_APP_ID) {
  NotImplemented_nid_no_patch("sceSystemServiceKillApp: application other than the running title");
 }
 if (how != -1 || reason != 0 || coreDump != 0) {
  NotImplemented_nid_no_patch("sceSystemServiceKillApp: arguments other than -1, 0 and 0");
 }
 LibcExit_nid_no_patch(0);
}

int APS5_VABI sceLncUtilKillApp(uint32_t appId) {
 if (appId != static_cast<uint32_t>(SYSTEM_SERVICE_RUNNING_APP_ID)) {
  NotImplemented_nid_no_patch("sceLncUtilKillApp: application other than the running title");
 }
 LibcExit_nid_no_patch(0);
}

int APS5_VABI sceLncUtilKillAppWithReason(uint32_t appId, int reason) {
 if (appId != static_cast<uint32_t>(SYSTEM_SERVICE_RUNNING_APP_ID)) {
  NotImplemented_nid_no_patch("sceLncUtilKillAppWithReason: application other than the running title");
 }
 if (reason != 0) {
  NotImplemented_nid_no_patch("sceLncUtilKillAppWithReason: a reason other than 0");
 }
 LibcExit_nid_no_patch(0);
}

int APS5_VABI sceSystemServiceLaunchSystemDeeplink(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceShellCoreUtilLaunchByUriParamInit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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

int APS5_VABI sceShellCoreUtilDebugGetNumberOfCffItems(void) {
    NotImplemented_nid_no_patch(__func__);
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

}
