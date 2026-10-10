#include "SceTypes.hpp"
#include "prx/libSceSystemService/SystemService.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" int APS5_VABI sceSystemServiceGetHdrToneMapLuminance(SystemServiceHdrToneMapLuminance* luminance);
extern "C" int APS5_VABI sceSystemServiceParamGetString(int paramId, char* buf, std::size_t bufSize);
extern "C" int APS5_VABI sceSystemServicePowerTick(void);
extern "C" int APS5_VABI sceSystemServiceReportAbnormalTermination(const void* info);
extern "C" int APS5_VABI sceSystemServiceDisableMusicPlayer(void);
extern "C" int APS5_VABI sceSystemServiceReenableMusicPlayer(void);
extern "C" int APS5_VABI sceSystemServiceDisableMediaPlay(void);
extern "C" int APS5_VABI sceSystemServiceReenableMediaPlay(void);
extern "C" int APS5_VABI sceSystemServiceLaunchWebBrowser(const char* uri, void* param);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int browserUnavailable = static_cast<int>(0x8002002Du);

const Case powerTick{"PowerTick_Repeated_Succeeds", [] {
    RequireEqual(sceSystemServicePowerTick(), SYSTEM_SERVICE_OK, "first tick");
    RequireEqual(sceSystemServicePowerTick(), SYSTEM_SERVICE_OK, "second tick");
}};

const Case abnormalTermination{"ReportAbnormalTermination_NullOrInfo_Succeeds", [] {
    RequireEqual(sceSystemServiceReportAbnormalTermination(nullptr), SYSTEM_SERVICE_OK, "null info");
    int info = 0;
    RequireEqual(sceSystemServiceReportAbnormalTermination(&info), SYSTEM_SERVICE_OK, "info");
}};

const Case musicPlayer{"MusicPlayer_DisableAndReenableTwice_Succeeds", [] {
    RequireEqual(sceSystemServiceDisableMusicPlayer(), SYSTEM_SERVICE_OK, "first disable");
    RequireEqual(sceSystemServiceDisableMusicPlayer(), SYSTEM_SERVICE_OK, "second disable");
    RequireEqual(sceSystemServiceReenableMusicPlayer(), SYSTEM_SERVICE_OK, "first reenable");
    RequireEqual(sceSystemServiceReenableMusicPlayer(), SYSTEM_SERVICE_OK, "second reenable");
}};

const Case mediaPlay{"MediaPlay_DisableAndReenableTwice_Succeeds", [] {
    RequireEqual(sceSystemServiceDisableMediaPlay(), SYSTEM_SERVICE_OK, "first disable");
    RequireEqual(sceSystemServiceDisableMediaPlay(), SYSTEM_SERVICE_OK, "second disable");
    RequireEqual(sceSystemServiceReenableMediaPlay(), SYSTEM_SERVICE_OK, "first reenable");
    RequireEqual(sceSystemServiceReenableMediaPlay(), SYSTEM_SERVICE_OK, "second reenable");
}};

const Case webBrowser{"LaunchWebBrowser_AnyUri_ReportsUnavailable", [] {
    RequireEqual(sceSystemServiceLaunchWebBrowser("http://127.0.0.1:8780/video?v=0", nullptr), browserUnavailable, "local uri");
    RequireEqual(sceSystemServiceLaunchWebBrowser("", nullptr), browserUnavailable, "empty uri");
    RequireEqual(sceSystemServiceLaunchWebBrowser(nullptr, nullptr), browserUnavailable, "null uri");
}};

const Case webBrowserParam{"LaunchWebBrowser_WithParam_LeavesParamUntouched", [] {
    unsigned char browserParam[64];
    std::memset(browserParam, 0x5a, sizeof(browserParam));
    RequireEqual(sceSystemServiceLaunchWebBrowser("https://example.com/", browserParam), browserUnavailable, "https uri");
    for (std::size_t i = 0; i < sizeof(browserParam); ++i) RequireEqual(browserParam[i], 0x5a, "param byte " + std::to_string(i));
}};

const Case luminanceNull{"GetHdrToneMapLuminance_NullOutput_FailsParameter", [] {
    RequireEqual(sceSystemServiceGetHdrToneMapLuminance(nullptr), SYSTEM_SERVICE_ERROR_PARAMETER, "null output");
}};

const Case luminance{"GetHdrToneMapLuminance_Default_ReportsSdrRange", [] {
    SystemServiceHdrToneMapLuminance value{-1.0f, -1.0f, -1.0f};
    RequireEqual(sceSystemServiceGetHdrToneMapLuminance(&value), SYSTEM_SERVICE_OK, "get");
    RequireEqual(value.max_full_frame_tone_map_luminance, 100.0f, "max full frame");
    RequireEqual(value.max_tone_map_luminance, 100.0f, "max");
    RequireEqual(value.min_tone_map_luminance, 0.0f, "min");
}};

const Case paramStringInvalid{"ParamGetString_NullOrEmptyBuffer_FailsParameter", [] {
    char name[SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH];
    RequireEqual(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, nullptr, sizeof(name)),
                 SYSTEM_SERVICE_ERROR_PARAMETER, "null buffer");
    RequireEqual(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, name, 0), SYSTEM_SERVICE_ERROR_PARAMETER,
                 "empty buffer");
}};

const Case paramStringUnsupported{"ParamGetString_ShortBufferOrUnsupportedParam_ThrowsWithoutWriting", [] {
    char name[SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH];
    std::memset(name, 'x', sizeof(name));
    Testing::RequireThrows<std::runtime_error>(
        [&] { sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, name, sizeof(name) - 1); }, "short buffer");
    Testing::RequireThrows<std::runtime_error>(
        [&] { sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_LANG, name, sizeof(name)); }, "language parameter");
    RequireEqual(name[0], 'x', "buffer untouched");
}};

const Case systemName{"ParamGetString_SystemName_ReturnsPs5", [] {
    char name[SYSTEM_SERVICE_MAX_SYSTEM_NAME_LENGTH];
    std::memset(name, 'x', sizeof(name));
    RequireEqual(sceSystemServiceParamGetString(SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, name, sizeof(name)), SYSTEM_SERVICE_OK, "get");
    RequireEqual(std::string(name), std::string("PS5"), "system name");
}};

} // namespace
