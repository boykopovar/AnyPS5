#include "SceTypes.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <initializer_list>
#include <stdexcept>

extern "C" int APS5_VABI sceSystemServiceGetHdrToneMapLuminance(SystemServiceHdrToneMapLuminance* luminance);
extern "C" int APS5_VABI sceSystemServiceParamGetString(int paramId, char* buf, std::size_t bufSize);
extern "C" int APS5_VABI sceSystemServiceLaunchApp(const char* titleId, const char* const* arguments, const void* param);
extern "C" int APS5_VABI sceSystemServiceAddLocalProcessForPsmKit(const char* path, const char* const* arguments, const void* param);
extern "C" int APS5_VABI sceSystemServiceKillLocalProcessForPsmKit(int handle);

namespace {

constexpr int browserUnavailable = static_cast<int>(0x8002002Du);

void Require(bool value) { if (!value) std::abort(); }

template<typename TCall>
bool Throws(TCall call) {
    try {
        static_cast<void>(call());
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool LaunchAppThrows(const char* titleId, const char* const* arguments, const void* param) {
    try {
        static_cast<void>(sceSystemServiceLaunchApp(titleId, arguments, param));
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

extern "C" int APS5_VABI sceSystemServicePowerTick(void);
extern "C" int APS5_VABI sceSystemServiceIsBgmPlaying(void);
extern "C" int APS5_VABI sceShellCoreUtilIsBgmPlaying(void);
extern "C" int APS5_VABI sceShellCoreUtilActivateInit(std::int32_t, std::intptr_t*);
extern "C" int APS5_VABI sceShellCoreUtilActivateIsActivated(std::intptr_t, bool*);
extern "C" int APS5_VABI sceShellCoreUtilActivateGetStatus(std::intptr_t, std::int32_t*, std::int32_t*);
extern "C" int APS5_VABI sceShellCoreUtilActivateStart(std::intptr_t);
extern "C" int APS5_VABI sceShellCoreUtilActivateStartAsync(std::intptr_t);
extern "C" int APS5_VABI sceShellCoreUtilActivateAbort(std::intptr_t);
extern "C" int APS5_VABI sceShellCoreUtilActivateTerm(std::intptr_t);
extern "C" int APS5_VABI sceShellCoreUtilActivateRecordActivation();
extern "C" int APS5_VABI sceSystemServiceReportAbnormalTermination(const void* info);
extern "C" int APS5_VABI sceSystemServiceDisableMusicPlayer(void);
extern "C" int APS5_VABI sceSystemServiceReenableMusicPlayer(void);
extern "C" int APS5_VABI sceSystemServiceDisableMediaPlay(void);
extern "C" int APS5_VABI sceSystemServiceReenableMediaPlay(void);
extern "C" int APS5_VABI sceSystemServiceLaunchWebBrowser(const char* uri);
extern "C" int APS5_VABI sceSystemServiceShowControllerSettings(void);
extern "C" int APS5_VABI sceSystemServiceShowDisplaySafeAreaSettings(void);
extern "C" int APS5_VABI sceSystemServiceOpenChallengeActivity(void);
extern "C" int APS5_VABI sceSystemServiceGetStatus(SystemServiceStatus* status);
extern "C" int APS5_VABI sceSystemServiceGetDisplaySafeAreaInfo(SystemServiceDisplaySafeAreaInfo* info);
extern "C" int APS5_VABI sceSystemServiceGetNoticeScreenSkipFlag(bool* value);
extern "C" int APS5_VABI sceSystemServiceSetNoticeScreenSkipFlag(void);
extern "C" int APS5_VABI sceSystemServiceDisableNoticeScreenSkipFlagAutoSet(void);
extern "C" int APS5_VABI sceSystemServiceHideSplashScreen(void);
extern "C" int APS5_VABI sceSystemServiceReceiveEvent(SystemServiceEvent* event);
extern "C" void APS5_VABI sceSystemServiceInitializePlayerDialogParam(SystemServicePlayerDialogParam* param);

namespace {

void TestActivation() {
    Require(sceShellCoreUtilActivateInit(0, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceShellCoreUtilActivateIsActivated(1, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceShellCoreUtilActivateGetStatus(1, nullptr, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    std::int32_t status = -1;
    std::int32_t result = -1;
    Require(sceShellCoreUtilActivateGetStatus(1, nullptr, &result) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceShellCoreUtilActivateGetStatus(1, &status, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);

    std::intptr_t handle = 0;
    Require(Throws([&] { return sceShellCoreUtilActivateInit(0, &handle); }));
    Require(handle == 0);
    bool activated = true;
    Require(Throws([&] { return sceShellCoreUtilActivateIsActivated(1, &activated); }));
    Require(activated == true);
    status = -1;
    result = -1;
    Require(Throws([&] { return sceShellCoreUtilActivateGetStatus(1, &status, &result); }));
    Require(status == -1 && result == -1);

    Require(sceShellCoreUtilActivateStart(handle) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilActivateStartAsync(handle) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilActivateAbort(handle) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilActivateTerm(handle) == SYSTEM_SERVICE_OK);
    Require(sceShellCoreUtilActivateRecordActivation() == SYSTEM_SERVICE_OK);
}

void TestPlayerDialogParam() {
    alignas(8) unsigned char buffer[0x100];
    std::memset(buffer, 0xab, sizeof(buffer));
    sceSystemServiceInitializePlayerDialogParam(reinterpret_cast<SystemServicePlayerDialogParam*>(buffer));
    unsigned char expected[0x40] = {0x40};
    std::memset(expected + 0xc, 0xff, 4);
    Require(std::memcmp(buffer, expected, sizeof(expected)) == 0);
    for (std::size_t i = sizeof(expected); i < sizeof(buffer); ++i) Require(buffer[i] == 0xab);
    bool threw = false;
    try {
        sceSystemServiceInitializePlayerDialogParam(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    Require(sceSystemServicePowerTick() == SYSTEM_SERVICE_OK);
    Require(sceSystemServicePowerTick() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceReportAbnormalTermination(nullptr) == SYSTEM_SERVICE_OK);
    int info = 0;
    Require(sceSystemServiceReportAbnormalTermination(&info) == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceDisableMusicPlayer() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceDisableMusicPlayer() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceReenableMusicPlayer() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceIsBgmPlaying() == 0 && sceShellCoreUtilIsBgmPlaying() == 0);
    Require(sceSystemServiceReenableMusicPlayer() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceDisableMediaPlay() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceDisableMediaPlay() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceReenableMediaPlay() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceReenableMediaPlay() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceShowControllerSettings() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceShowDisplaySafeAreaSettings() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceOpenChallengeActivity() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceLaunchWebBrowser("http:") == browserUnavailable);
    Require(sceSystemServiceLaunchWebBrowser("https:") == browserUnavailable);
    Require(sceSystemServiceLaunchWebBrowser("psno:") == browserUnavailable);
    Require(sceSystemServiceLaunchWebBrowser("") == static_cast<int>(0x80a10008));
    Require(sceSystemServiceLaunchWebBrowser("HTTP:") == static_cast<int>(0x80a10008));
    Require(sceSystemServiceLaunchWebBrowser(nullptr) == static_cast<int>(0x80a10003));
    std::string browserUri = "http:" + std::string(6010, 'a');
    Require(sceSystemServiceLaunchWebBrowser(browserUri.c_str()) == browserUnavailable);
    browserUri += 'a';
    Require(sceSystemServiceLaunchWebBrowser(browserUri.c_str()) == static_cast<int>(0x80a10007));
    Require(LaunchAppThrows("PPSA99109", nullptr, nullptr));
    Require(LaunchAppThrows(nullptr, nullptr, nullptr));
    const char* launchArguments[] = {"--backup", nullptr};
    unsigned char launchParam[0x20];
    std::memset(launchParam, 0x5a, sizeof(launchParam));
    Require(LaunchAppThrows("PPSA99110", launchArguments, launchParam));
    for (unsigned char byte : launchParam) Require(byte == 0x5a);
    Require(std::strcmp(launchArguments[0], "--backup") == 0 && launchArguments[1] == nullptr);
    const char* childArguments[] = {"/app0/child.bin", nullptr};
    unsigned char processParam[0x90];
    std::memset(processParam, 0x5a, sizeof(processParam));
    Require(Throws([&] { return sceSystemServiceAddLocalProcessForPsmKit("/app0/child.bin", childArguments, processParam); }));
    for (unsigned char byte : processParam) Require(byte == 0x5a);
    Require(Throws([] { return sceSystemServiceAddLocalProcessForPsmKit(nullptr, nullptr, nullptr); }));
    Require(Throws([] { return sceSystemServiceKillLocalProcessForPsmKit(1); }));
    Require(sceSystemServiceGetHdrToneMapLuminance(nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    SystemServiceHdrToneMapLuminance luminance{-1.0f, -1.0f, -1.0f};
    Require(sceSystemServiceGetHdrToneMapLuminance(&luminance) == SYSTEM_SERVICE_OK);
    Require(luminance.max_full_frame_tone_map_luminance == 100.0f);
    Require(luminance.max_tone_map_luminance == 100.0f);
    Require(luminance.min_tone_map_luminance == 0.0f);

    char name[SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH * 2];
    const auto untouched = [&name] {
        for (const char byte : name) {
            if (byte != 'x') return false;
        }
        return true;
    };
    std::memset(name, 'x', sizeof(name));
    Require(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, nullptr, SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH) == SYSTEM_SERVICE_ERROR_PARAMETER);
    for (std::size_t size = 0; size < SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH; ++size) {
        Require(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, name, size) == SYSTEM_SERVICE_ERROR_PARAMETER);
    }
    for (const int paramId : {-1, 0, 1, 5, 7, 40, 100, 1000, 0x7fffffff}) {
        Require(sceSystemServiceParamGetString(paramId, name, sizeof(name)) == SYSTEM_SERVICE_ERROR_PARAMETER);
    }
    Require(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_LANG, name, sizeof(name)) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(untouched());
    Require(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, name, SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH + 3) == SYSTEM_SERVICE_OK);
    Require(std::strcmp(name, "PS5") == 0);
    for (std::size_t index = 4; index < SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH + 3; ++index) Require(name[index] == '\0');
    for (std::size_t index = SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH + 3; index < sizeof(name); ++index) Require(name[index] == 'x');

    Require(sceSystemServiceGetStatus(nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    SystemServiceStatus status;
    std::memset(&status, 0x5a, sizeof(status));
    Require(sceSystemServiceGetStatus(&status) == SYSTEM_SERVICE_OK);
    Require(status.event_num == 0 && !status.is_system_ui_overlaid && !status.is_in_background_execution && !status.is_vr_play_area_overlaid);
    for (unsigned char byte : status.reserved) Require(byte == 0);
    Require(sceSystemServiceGetDisplaySafeAreaInfo(nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    SystemServiceDisplaySafeAreaInfo safeArea;
    std::memset(&safeArea, 0x5a, sizeof(safeArea));
    Require(sceSystemServiceGetDisplaySafeAreaInfo(&safeArea) == SYSTEM_SERVICE_OK);
    Require(safeArea.ratio == 1.0f);
    for (unsigned char byte : safeArea.reserved) Require(byte == 0);
    Require(sceSystemServiceGetNoticeScreenSkipFlag(nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    bool skip = true;
    Require(sceSystemServiceGetNoticeScreenSkipFlag(&skip) == SYSTEM_SERVICE_OK && !skip);
    Require(sceSystemServiceSetNoticeScreenSkipFlag() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceDisableNoticeScreenSkipFlagAutoSet() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceHideSplashScreen() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceReceiveEvent(nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    SystemServiceEvent event;
    event.event_type = 7;
    Require(sceSystemServiceReceiveEvent(&event) == SYSTEM_SERVICE_ERROR_NO_EVENT);
    TestPlayerDialogParam();
    TestActivation();
}
