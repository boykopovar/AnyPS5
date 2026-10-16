#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <charconv>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif
#include "prx/libc/include/Shutdown.hpp"
#include "SceTypes.hpp"
#include "LoadExec.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"

namespace {

int ConsoleLanguage() {
    const char* configured = std::getenv("ANYPS5_LANGUAGE");
    if (configured == nullptr || configured[0] == '\0') return SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US;
    const std::string_view text(configured);
    int language = -1;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), language);
    if (error != std::errc{} || end != text.data() + text.size() || language < SYSTEM_SERVICE_PARAM_LANG_JAPANESE || language > SYSTEM_SERVICE_PARAM_LANG_UKRAINIAN) {
        throw std::runtime_error("sceSystemServiceParamGetInt: ANYPS5_LANGUAGE must be a console language number from " +
            std::to_string(SYSTEM_SERVICE_PARAM_LANG_JAPANESE) + " to " + std::to_string(SYSTEM_SERVICE_PARAM_LANG_UKRAINIAN) +
            ", not \"" + std::string(text) + "\"");
    }
    return language;
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
#elif defined(__linux__)
struct HostLaunch {
    std::string executable;
    std::vector<std::string> arguments;
};

HostLaunch PrepareLaunch(const char* path, const char* const* arguments) {
    std::error_code error;
    const auto own = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) throw std::runtime_error("sceSystemServiceLoadExec: cannot read /proc/self/exe: " + error.message());
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

}

extern "C" {

int APS5_VABI sceSystemServiceLoadExec(const char* path, const char* const* arguments) {
    if (!path || !*path) return SYSTEM_SERVICE_ERROR_PARAMETER;
    if (std::strcmp(path, "exit") == 0) LibcExitWithoutHandlers_nid_no_patch(0);
    auto launch = PrepareLaunch(path, arguments);
#if defined(_WIN32) || defined(__linux__)
    LoadConvertedExecutable_nid_no_patch(std::filesystem::path(launch.executable), arguments);
#endif
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
  case SYSTEM_SERVICE_PARAM_ID_400: *value = ConsoleLanguage(); break;
  case SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT: *value = SYSTEM_SERVICE_PARAM_DATE_FORMAT_DDMMYYYY; break;
  case SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT: *value = SYSTEM_SERVICE_PARAM_TIME_FORMAT_24HOUR; break;
  case SYSTEM_SERVICE_PARAM_ID_TIME_ZONE: *value = 0; break;
  case SYSTEM_SERVICE_PARAM_ID_SUMMERTIME: *value = 0; break;
  case SYSTEM_SERVICE_PARAM_ID_GAME_PARENTAL_LEVEL: *value = SYSTEM_SERVICE_PARAM_GAME_PARENTAL_OFF; break;
  case SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN: *value = SYSTEM_SERVICE_PARAM_ENTER_BUTTON_CROSS; break;
  default: *value = 0; break;
 }
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceParamGetString(int paramId, char* buf, size_t bufSize) {
 if (buf == nullptr || bufSize == 0) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 if (paramId != SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME) {
  NotImplemented_nid_no_patch("sceSystemServiceParamGetString: parameter other than the system name");
 }
 if (bufSize < SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH) {
  NotImplemented_nid_no_patch("sceSystemServiceParamGetString: buffer shorter than 65 bytes");
 }
 constexpr char SystemName[] = "PS5";
 std::memcpy(buf, SystemName, sizeof(SystemName));
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

int APS5_VABI sceSystemServiceInitializePlayerDialogParam(void* param) {
 if (param == nullptr) return SYSTEM_SERVICE_ERROR_PARAMETER;
 return SYSTEM_SERVICE_OK;
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

int APS5_VABI sceSystemServiceLaunchWebBrowser(const char* uri, void* param) {
 (void)uri;
 (void)param;
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

int APS5_VABI sceSystemServiceOpenTournamentOccurrence(void) {
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

int APS5_VABI sceSystemServiceKillApp(int appId, int how, int reason, int coreDump) {
 if (appId != SYSTEM_SERVICE_RUNNING_APP_ID) {
  NotImplemented_nid_no_patch("sceSystemServiceKillApp: application other than the running title");
 }
 if (how != -1 || reason != 0 || coreDump != 0) {
  NotImplemented_nid_no_patch("sceSystemServiceKillApp: arguments other than -1, 0 and 0");
 }
 LibcExit_nid_no_patch(0);
}

}
