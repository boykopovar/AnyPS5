#include "prx/libSceCamera/libSceCamera.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using GetU32 = int(APS5_VABI*)(std::int32_t, std::uint32_t, std::uint32_t*, void*);
using SetU32 = int(APS5_VABI*)(std::int32_t, std::uint32_t, std::uint32_t, const void*);

namespace {

constexpr int ErrorParam = static_cast<int>(0x802E0000u);
constexpr int ErrorNotInit = static_cast<int>(0x802E0002u);
constexpr int ErrorNotOpen = static_cast<int>(0x802E0004u);
constexpr int ErrorNotConnected = static_cast<int>(0x802E0010u);
constexpr int ErrorFatal = static_cast<int>(0x802E00FFu);
constexpr std::uint32_t Channel0 = 1;
constexpr std::uint32_t Channel1 = 2;
constexpr std::uint32_t ChannelBoth = 3;

using AudioGetData = int (APS5_VABI*)(void*);

AudioGetData ResolveAudio(const char* name) {
#ifdef _WIN32
    void* symbol = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("libSceCamera.prx"), name));
#else
    void* symbol = dlsym(RTLD_DEFAULT, name);
#endif
    if (!symbol) {
        std::fprintf(stderr, "%s is not exported by libSceCamera\n", name);
        std::abort();
    }
    return reinterpret_cast<AudioGetData>(symbol);
}

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Camera settings: %s\n", message);
        std::abort();
    }
}

void CheckAudioAfterOpen() {
    std::array<std::uint8_t, 64> data{};
    data.fill(0xA5);
    for (const char* name : {"sceCameraAudioGetData", "sceCameraAudioGetData2"}) {
        const AudioGetData getData = ResolveAudio(name);
        Require(getData(nullptr) == ErrorParam, "camera audio data must reject a null buffer");
        Require(getData(data.data()) == 0, "camera audio data must answer 0 after the failed open");
    }
    for (const std::uint8_t byte : data) Require(byte == 0xA5, "camera audio data must not be written");
    Require(sceCameraAudioReset() == 0, "camera audio reset must answer 0");
    Require(sceCameraAudioClose() == 0, "camera audio close must answer 0");
}

CameraConfig SizedConfig() {
    CameraConfig config;
    std::memset(&config, 0x5a, sizeof(config));
    config.sizeThis = sizeof(CameraConfig);
    return config;
}

void CheckBeforeOpen() {
    std::uint32_t value = 0;
    CameraConfig config = SizedConfig();
    const CameraDeviceInfo info{sizeof(CameraDeviceInfo), 1, 0, 0};
    Require(sceCameraGetContrast(1, Channel0, &value, nullptr) == ErrorNotOpen, "a getter before open must report NOT_OPEN");
    Require(sceCameraSetContrast(1, Channel0, 1, nullptr) == ErrorNotOpen, "a setter before open must report NOT_OPEN");
    Require(sceCameraGetConfig(1, &config) == ErrorNotOpen, "reading the configuration before open must report NOT_OPEN");
    Require(sceCameraGetDeviceConfigWithoutHandle(&config) == ErrorNotOpen, "reading the device configuration before open must report NOT_OPEN");
    Require(sceCameraGetMmapConnectedCount(&value) == ErrorNotOpen, "the mapped device count before open must report NOT_OPEN");
    Require(sceCameraGetDeviceInfo(0, &info) == ErrorNotInit, "device info before open must report NOT_INIT");
    Require(sceCameraGetProductInfo(&value) == ErrorNotInit, "product info before open must report NOT_INIT");
    Require(sceCameraGetRegistryInfo(&value) == ErrorNotInit, "registry info before open must report NOT_INIT");
}

void CheckGetter(GetU32 get, std::uint32_t expected, const char* message) {
    std::uint32_t value = 0x5a5a5a5a;
    Require(get(1, Channel1, &value, nullptr) == 0 && value == expected, message);
    value = 0x5a5a5a5a;
    std::uint32_t option = 0;
    Require(get(0, Channel0, &value, nullptr) == ErrorParam, "a getter must reject handle 0");
    Require(get(99, Channel0, &value, nullptr) == ErrorNotOpen && value == 0x5a5a5a5a, "a fabricated handle must not read settings");
    Require(get(1, 0, &value, nullptr) == ErrorParam, "a getter must reject channel 0");
    Require(get(1, ChannelBoth, &value, nullptr) == ErrorParam, "a getter must reject both channels");
    Require(get(1, 4, &value, nullptr) == ErrorParam, "a getter must reject channel 4");
    Require(get(1, 0xFFFFFFFFu, &value, nullptr) == ErrorParam, "a getter must reject invalid channel");
    Require(get(1, Channel0, nullptr, nullptr) == ErrorParam, "a getter must reject a null result");
    Require(get(1, Channel0, &value, &option) == ErrorParam, "a getter must reject an option");
    Require(value == 0x5a5a5a5a, "a refused getter must not write its result");
}

void CheckGetters() {
    CheckGetter(sceCameraGetAutoWhiteBalance, 0, "automatic white balance must be off");
    CheckGetter(sceCameraGetDefectivePixelCancellation, 0, "defective pixel cancellation must be off");
    CheckGetter(sceCameraGetLensCorrection, 0, "lens correction must be off");
    CheckGetter(sceCameraGetContrast, 32, "the contrast must be 32");
    CheckGetter(sceCameraGetSaturation, 64, "the saturation must be 64");
    CheckGetter(sceCameraGetSharpness, 1, "the sharpness must be 1");
    std::int32_t hue = 0;
    Require(sceCameraGetHue(1, Channel0, &hue, nullptr) == 0 && hue == 1, "the hue must be 1");
    CameraExposureGain exposure{9, 9, 9, 9};
    Require(sceCameraGetExposureGain(1, Channel0, &exposure, nullptr) == 0, "the exposure must be readable");
    Require(exposure.exposureControl == 0 && exposure.exposure == 20 && exposure.gain == 100 && exposure.mode == 0, "the exposure defaults are wrong");
    CameraWhiteBalance balance{9, 9, 9, 9};
    Require(sceCameraGetWhiteBalance(1, Channel0, &balance, nullptr) == 0, "the white balance must be readable");
    Require(balance.whiteBalanceControl == 0 && balance.gainRed == 768 && balance.gainBlue == 768 && balance.gainGreen == 512, "the white balance defaults are wrong");
    CameraGamma gamma;
    std::memset(&gamma, 0x5a, sizeof(gamma));
    Require(sceCameraGetGamma(1, Channel0, &gamma, nullptr) == 0 && gamma.gammaControl == 0 && gamma.value == 4 && gamma.reserved[15] == 0, "the gamma defaults are wrong");
    std::uint32_t enable = 9;
    CameraAutoExposureGainTarget target{sizeof(CameraAutoExposureGainTarget), 0x10};
    Require(sceCameraGetAutoExposureGain(1, Channel0, &enable, &target) == 0 && enable == 0, "automatic exposure must be off");
    Require(target.sizeThis == 0 && target.target == 0, "the automatic exposure target must be cleared");
    target.sizeThis = 4;
    Require(sceCameraGetAutoExposureGain(1, Channel0, &enable, &target) == ErrorParam, "a target of the wrong size must be rejected");
    Require(sceCameraGetAutoExposureGain(1, Channel0, &enable, nullptr) == 0, "automatic exposure must be readable without a target");
}

void CheckAttribute() {
    CameraAttribute attribute;
    std::memset(&attribute, 0x5a, sizeof(attribute));
    attribute.sizeThis = sizeof(CameraAttribute);
    attribute.channel = Channel1;
    Require(sceCameraGetAttribute(1, &attribute) == 0, "the attributes must be readable");
    Require(attribute.sizeThis == sizeof(CameraAttribute) && attribute.channel == Channel1, "the attribute header must be kept");
    Require(attribute.framePosition.x == 0 && attribute.exposureGain.exposure == 83 && attribute.exposureGain.gain == 100, "the attribute exposure is wrong");
    Require(attribute.whiteBalance.gainRed == 768 && attribute.whiteBalance.gainGreen == 512 && attribute.gamma.value == 4, "the attribute colour defaults are wrong");
    Require(attribute.saturation == 64 && attribute.contrast == 32 && attribute.sharpness == 1 && attribute.hue == 1 && attribute.reserved[3] == 0, "the attribute levels are wrong");
    attribute.channel = ChannelBoth;
    Require(sceCameraGetAttribute(1, &attribute) == ErrorParam, "reading both channels at once must be rejected");
    Require(sceCameraSetAttribute(1, &attribute) == ErrorNotConnected, "writing both channels must report NOT_CONNECTED");
    attribute.channel = 4;
    Require(sceCameraSetAttribute(1, &attribute) == ErrorParam, "an unknown channel must be rejected");
    attribute.channel = Channel0;
    attribute.sizeThis = sizeof(CameraAttribute) - 4;
    Require(sceCameraGetAttribute(1, &attribute) == ErrorParam && sceCameraSetAttribute(1, &attribute) == ErrorParam, "an attribute of the wrong size must be rejected");
    Require(sceCameraGetAttribute(1, nullptr) == ErrorParam && sceCameraSetAttribute(1, nullptr) == ErrorParam, "a null attribute must be rejected");
}

void CheckSetter(SetU32 set, std::uint32_t largestValue) {
    std::uint32_t option = 0;
    Require(set(1, ChannelBoth, largestValue, nullptr) == ErrorNotConnected, "a setter must report NOT_CONNECTED");
    Require(set(0, Channel0, 0, nullptr) == ErrorParam, "a setter must reject handle 0");
    Require(set(1, 0, 0, nullptr) == ErrorParam, "a setter must reject channel 0");
    Require(set(1, 4, 0, nullptr) == ErrorParam, "a setter must reject channel 4");
    Require(set(1, 0xFFFFFFFFu, 0, nullptr) == ErrorParam, "a setter must reject invalid channel");
    Require(set(1, Channel0, 0, &option) == ErrorParam, "a setter must reject an option");
    if (largestValue != UINT32_MAX) Require(set(1, Channel0, largestValue + 1, nullptr) == ErrorParam, "a setter must reject a value past its range");
}

void CheckSetters() {
    CheckSetter(sceCameraSetAutoWhiteBalance, UINT32_MAX);
    CheckSetter(sceCameraSetContrast, UINT32_MAX);
    CheckSetter(sceCameraSetSaturation, UINT32_MAX);
    CheckSetter(sceCameraSetDefectivePixelCancellation, 1);
    CheckSetter(sceCameraSetDefectivePixelCancellationInternal, 2);
    CheckSetter(sceCameraSetLensCorrection, 1);
    CheckSetter(sceCameraSetLensCorrectionInternal, 2);
    CheckSetter(sceCameraSetSharpness, 10);
    Require(sceCameraSetHue(1, Channel0, -5, nullptr) == ErrorNotConnected, "a hue change must report NOT_CONNECTED");
    const CameraExposureGain exposure{};
    const CameraWhiteBalance balance{};
    const CameraGamma gamma{};
    Require(sceCameraSetExposureGain(1, Channel0, &exposure, nullptr) == ErrorNotConnected, "an exposure change must report NOT_CONNECTED");
    Require(sceCameraSetWhiteBalance(1, Channel0, &balance, nullptr) == ErrorNotConnected, "a white balance change must report NOT_CONNECTED");
    Require(sceCameraSetGamma(1, Channel0, &gamma, nullptr) == ErrorNotConnected, "a gamma change must report NOT_CONNECTED");
    Require(sceCameraSetExposureGain(1, Channel0, nullptr, nullptr) == ErrorParam, "a null exposure must be rejected");
    Require(sceCameraSetWhiteBalance(1, Channel0, nullptr, nullptr) == ErrorParam, "a null white balance must be rejected");
    Require(sceCameraSetGamma(1, Channel0, nullptr, nullptr) == ErrorParam, "a null gamma must be rejected");
    CameraAutoExposureGainTarget target{sizeof(CameraAutoExposureGainTarget), 0x20};
    Require(sceCameraSetAutoExposureGain(1, Channel0, 1, &target) == ErrorNotConnected, "the largest exposure target must be accepted");
    Require(sceCameraSetAutoExposureGain(1, Channel0, 1, nullptr) == ErrorNotConnected, "automatic exposure without a target must be accepted");
    for (const std::uint32_t accepted : {0x0u, 0x2u, 0x4u, 0x6u, 0x8u, 0x10u, 0x12u, 0x14u, 0x16u, 0x20u}) {
        target.target = accepted;
        Require(sceCameraSetAutoExposureGain(1, Channel0, 1, &target) == ErrorNotConnected, "a listed exposure target must be accepted");
    }
    for (const std::uint32_t refused : {0x1u, 0xAu, 0xCu, 0xEu, 0x11u, 0x18u, 0x1Au, 0x1Cu, 0x1Eu, 0x22u}) {
        target.target = refused;
        Require(sceCameraSetAutoExposureGain(1, Channel0, 1, &target) == ErrorParam, "an unlisted exposure target must be rejected");
    }
    target = {4, 0x10};
    Require(sceCameraSetAutoExposureGain(1, Channel0, 1, &target) == ErrorParam, "an exposure target of the wrong size must be rejected");
}

void CheckConfig() {
    CameraConfig config = SizedConfig();
    Require(sceCameraGetConfig(1, &config) == 0 && config.configType == 1, "the configuration must be type 1");
    for (const CameraConfigExtension& extension : config.extension) {
        Require(extension.formatLevel[0] == 0 && extension.formatLevel[1] == 4 && extension.formatLevel[3] == 4, "the default formats are wrong");
        Require(extension.resolution == 0 && extension.framerate == 60 && extension.width == 0 && extension.baseOption == nullptr, "the default mode is wrong");
    }
    Require(sceCameraSetConfig(1, &config) == ErrorNotConnected, "a new configuration must report NOT_CONNECTED");
    Require(sceCameraSetConfigInternal(1, &config) == ErrorNotConnected, "an internal configuration must report NOT_CONNECTED");
    config = SizedConfig();
    Require(sceCameraGetDeviceConfig(1, &config) == 0 && config.sizeThis == sizeof(CameraConfig) && config.configType == 0, "the device configuration must be empty");
    Require(config.extension[1].baseOption == nullptr, "the device configuration must be cleared");
    config = SizedConfig();
    Require(sceCameraGetDeviceConfigWithoutHandle(&config) == 0 && config.sizeThis == 0 && config.configType == 0, "the handleless device configuration must be cleared");
    config = SizedConfig();
    config.sizeThis = 96;
    Require(sceCameraGetConfig(1, &config) == ErrorParam && sceCameraSetConfig(1, &config) == ErrorParam, "a configuration of the wrong size must be rejected");
    Require(sceCameraGetDeviceConfig(1, &config) == ErrorParam && sceCameraGetDeviceConfigWithoutHandle(&config) == ErrorParam, "a device configuration of the wrong size must be rejected");
    Require(sceCameraGetConfig(0, nullptr) == ErrorParam && sceCameraSetConfigInternal(1, nullptr) == ErrorParam, "a missing configuration must be rejected");
}

void CheckDevice() {
    CameraDeviceInfo info{sizeof(CameraDeviceInfo), 1, 0, 0};
    std::array<std::uint8_t, 64> out{};
    Require(sceCameraGetDeviceInfo(0, &info) == ErrorNotConnected, "device info must report NOT_CONNECTED");
    Require(sceCameraGetDeviceInfo(1, &info) == ErrorParam, "a reserved value other than 0 must be rejected");
    info.infoRevision = 2;
    Require(sceCameraGetDeviceInfo(0, &info) == ErrorParam, "an unknown info revision must be rejected");
    Require(sceCameraGetDeviceInfo(0, nullptr) == ErrorParam, "null device info must be rejected");
    Require(sceCameraGetProductInfo(out.data()) == ErrorNotConnected && sceCameraGetRegistryInfo(out.data()) == ErrorNotConnected, "product and registry info must report NOT_CONNECTED");
    Require(sceCameraGetProductInfo(nullptr) == ErrorParam && sceCameraGetRegistryInfo(nullptr) == ErrorParam, "null product or registry info must be rejected");
    std::uint32_t count = 9;
    Require(sceCameraGetMmapConnectedCount(&count) == 0 && count == 0, "no camera may be mapped");
    Require(sceCameraGetMmapConnectedCount(nullptr) == ErrorParam, "a null count must be rejected");
    Require(sceCameraGetCalibDataFromDevice() == ErrorNotConnected, "device calibration must report NOT_CONNECTED");
    CameraGetCalibrationDataParameter param{sizeof(CameraGetCalibrationDataParameter), 0, 1};
    Require(sceCameraGetCalibrationData(&param, out.data()) == ErrorNotConnected, "calibration data must report NOT_CONNECTED");
    Require(sceCameraGetCalibrationData(&param, nullptr) == ErrorParam && sceCameraGetCalibrationData(nullptr, out.data()) == ErrorParam, "missing calibration arguments must be rejected");
    for (const CameraGetCalibrationDataParameter refused : {CameraGetCalibrationDataParameter{8, 0, 0}, CameraGetCalibrationDataParameter{12, 1, 0}, CameraGetCalibrationDataParameter{12, 0, 2}}) {
        Require(sceCameraGetCalibrationData(&refused, out.data()) == ErrorParam, "a bad calibration request must be rejected");
    }
    Require(sceCameraAudioOpen() == ErrorFatal, "camera audio must fail with FATAL");
    CheckAudioAfterOpen();
}

}

int main() {
    CheckBeforeOpen();
    Require(sceCameraOpen(0xFF, 0, 0, nullptr) == 1, "the camera must open");
    CheckGetters();
    CheckAttribute();
    CheckSetters();
    CheckConfig();
    CheckDevice();
    Require(sceCameraClose(1) == 0, "the camera must close");
    std::uint32_t value = 0;
    Require(sceCameraGetSharpness(1, Channel0, &value, nullptr) == ErrorNotOpen, "a getter after close must report NOT_OPEN");
    return 0;
}
