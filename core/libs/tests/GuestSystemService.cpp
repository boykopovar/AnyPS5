#include "SceTypes.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" int APS5_VABI sceSystemServiceGetHdrToneMapLuminance(SystemServiceHdrToneMapLuminance* luminance);
extern "C" int APS5_VABI sceSystemServiceParamGetString(int paramId, char* buf, std::size_t bufSize);

namespace {

void Require(bool value) { if (!value) std::abort(); }

bool ParamGetStringThrows(int paramId, char* buf, std::size_t bufSize) {
    try {
        static_cast<void>(sceSystemServiceParamGetString(paramId, buf, bufSize));
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

extern "C" int APS5_VABI sceSystemServicePowerTick(void);
extern "C" int APS5_VABI sceSystemServiceReportAbnormalTermination(const void* info);
extern "C" int APS5_VABI sceSystemServiceDisableMusicPlayer(void);
extern "C" int APS5_VABI sceSystemServiceReenableMusicPlayer(void);
extern "C" int APS5_VABI sceSystemServiceLaunchWebBrowser(const char* uri, const void* param);

namespace {

bool LaunchWebBrowserThrows(const char* uri, const void* param, const char* what) {
    try {
        static_cast<void>(sceSystemServiceLaunchWebBrowser(uri, param));
    } catch (const std::runtime_error& error) {
        return std::strcmp(error.what(), what) == 0;
    }
    return false;
}

}

int main() {
    Require(sceSystemServiceLaunchWebBrowser(nullptr, nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceSystemServiceLaunchWebBrowser("", nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    int launchParam = 0;
    Require(LaunchWebBrowserThrows("https://example.com/", &launchParam,
                                   "sceSystemServiceLaunchWebBrowser: launch parameter not implemented"));
    constexpr const char* schemeNotImplemented =
        "sceSystemServiceLaunchWebBrowser: URI scheme other than http or https not implemented";
    Require(LaunchWebBrowserThrows("ftp://example.com/", nullptr, schemeNotImplemented));
    Require(LaunchWebBrowserThrows("file:///etc/passwd", nullptr, schemeNotImplemented));
    Require(LaunchWebBrowserThrows("example.com", nullptr, schemeNotImplemented));
    Require(LaunchWebBrowserThrows("http:/example.com", nullptr, schemeNotImplemented));

    Require(sceSystemServicePowerTick() == SYSTEM_SERVICE_OK);
    Require(sceSystemServicePowerTick() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceReportAbnormalTermination(nullptr) == SYSTEM_SERVICE_OK);
    int info = 0;
    Require(sceSystemServiceReportAbnormalTermination(&info) == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceDisableMusicPlayer() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceDisableMusicPlayer() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceReenableMusicPlayer() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceReenableMusicPlayer() == SYSTEM_SERVICE_OK);
    Require(sceSystemServiceGetHdrToneMapLuminance(nullptr) == SYSTEM_SERVICE_ERROR_PARAMETER);
    SystemServiceHdrToneMapLuminance luminance{-1.0f, -1.0f, -1.0f};
    Require(sceSystemServiceGetHdrToneMapLuminance(&luminance) == SYSTEM_SERVICE_OK);
    Require(luminance.max_full_frame_tone_map_luminance == 100.0f);
    Require(luminance.max_tone_map_luminance == 100.0f);
    Require(luminance.min_tone_map_luminance == 0.0f);

    char name[SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH];
    std::memset(name, 'x', sizeof(name));
    Require(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, nullptr, sizeof(name)) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, name, 0) == SYSTEM_SERVICE_ERROR_PARAMETER);
    Require(ParamGetStringThrows(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, name, sizeof(name) - 1));
    Require(ParamGetStringThrows(SYSTEM_SERVICE_PARAM_ID_LANG, name, sizeof(name)));
    Require(name[0] == 'x');
    Require(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, name, sizeof(name)) == SYSTEM_SERVICE_OK);
    Require(std::strcmp(name, "PS5") == 0);
}
