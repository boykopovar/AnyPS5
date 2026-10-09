#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include "prx/libc/include/Shutdown.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceSystemService/SystemService.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
extern char** environ;
#endif

namespace {

bool hasWebScheme(std::string_view uri) {
    for (std::string_view scheme : {std::string_view("http://"), std::string_view("https://")}) {
        if (uri.size() < scheme.size()) continue;
        bool matches = true;
        for (std::size_t i = 0; i < scheme.size() && matches; ++i) {
            matches = std::tolower(static_cast<unsigned char>(uri[i])) == scheme[i];
        }
        if (matches) return true;
    }
    return false;
}

void openInHostBrowser(const char* uri) {
#ifdef _WIN32
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, uri, -1, nullptr, 0);
    if (length <= 0) throw std::runtime_error("sceSystemServiceLaunchWebBrowser: the URI is not valid UTF-8");
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, uri, -1, wide.data(), length);
    const auto result = reinterpret_cast<std::intptr_t>(ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        throw std::runtime_error("sceSystemServiceLaunchWebBrowser: ShellExecuteW failed with " + std::to_string(result));
    }
#else
#ifdef __APPLE__
    const char* opener = "open";
#else
    const char* opener = "xdg-open";
#endif
    char* const arguments[] = {const_cast<char*>(opener), const_cast<char*>(uri), nullptr};
    pid_t pid = 0;
    const int error = posix_spawnp(&pid, opener, nullptr, nullptr, arguments, environ);
    if (error != 0) {
        throw std::runtime_error(std::string("sceSystemServiceLaunchWebBrowser: cannot start ") + opener + ": " + std::strerror(error));
    }
    std::thread([pid] {
        int status = 0;
        waitpid(pid, &status, 0);
    }).detach();
#endif
}

}

extern "C" {

int APS5_VABI sceSystemServiceLaunchWebBrowser(const char* uri, const void* param) {
    if (uri == nullptr || *uri == '\0') return SYSTEM_SERVICE_ERROR_PARAMETER;
    if (param != nullptr) NotImplemented_nid_no_patch("sceSystemServiceLaunchWebBrowser: launch parameter");
    if (!hasWebScheme(uri)) NotImplemented_nid_no_patch("sceSystemServiceLaunchWebBrowser: URI scheme other than http or https");
    openInHostBrowser(uri);
    return SYSTEM_SERVICE_OK;
}


int APS5_VABI sceSystemServiceLoadExec(const char* path, const char* const* arguments) {
    if (!path || !*path) return SYSTEM_SERVICE_ERROR_PARAMETER;
    if (std::strcmp(path, "exit") != 0) {
        NotImplemented_nid_no_patch("sceSystemServiceLoadExec: executable replacement");
    }
    (void)arguments;
    LibcRunShutdown_nid_postfix();
    std::exit(0);
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
 constexpr float SdrReferenceWhiteNits = 100.0f;
 luminance->max_full_frame_tone_map_luminance = SdrReferenceWhiteNits;
 luminance->max_tone_map_luminance = SdrReferenceWhiteNits;
 luminance->min_tone_map_luminance = 0.0f;
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
  case SYSTEM_SERVICE_PARAM_ID_LANG: *value = SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US; break;
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
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemServiceReenableMediaPlay() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemServiceLaunchPlayerDialog(const void* param) {
 if (param == nullptr) return SYSTEM_SERVICE_ERROR_PARAMETER;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceDisableMusicPlayer(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceOpenChallengeActivity(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemServiceOpenTournamentOccurrence(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceSystemServiceReenableMusicPlayer(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceShowControllerSettings(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
