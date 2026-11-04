#include <cstdint>
#include "prx/libSceCamera/libSceCamera.hpp"
#include "prx/libSceCamera/include/Camera.hpp"

namespace {

constexpr std::uint32_t CONFIG_TYPE1 = 0x01;
constexpr std::uint32_t FORMAT_YUV422 = 0x0;
constexpr std::uint32_t SCALE_FORMAT_Y8 = 0x4;
constexpr std::uint32_t RESOLUTION_1280X800 = 0x0;
constexpr std::uint32_t FRAMERATE_60 = 60;

bool ValidConfig(const CameraConfig* config) {
    return config && config->sizeThis == sizeof(CameraConfig);
}

int CheckConfig(std::int32_t handle, const CameraConfig* config) {
    if (handle < 1 || !ValidConfig(config)) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen(handle)) return SCE_CAMERA_ERROR_NOT_OPEN;
    return 0;
}

}

extern "C" {

int APS5_VABI sceCameraGetConfig(std::int32_t handle, CameraConfig* config) {
    if (const int error = CheckConfig(handle, config)) return error;
    config->configType = CONFIG_TYPE1;
    const CameraConfigExtension extension{{FORMAT_YUV422, SCALE_FORMAT_Y8, SCALE_FORMAT_Y8, SCALE_FORMAT_Y8}, RESOLUTION_1280X800, FRAMERATE_60, 0, 0, 0, nullptr};
    config->extension[0] = extension;
    config->extension[1] = extension;
    return 0;
}

int APS5_VABI sceCameraSetConfig(std::int32_t handle, const CameraConfig* config) {
    if (const int error = CheckConfig(handle, config)) return error;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceCameraSetConfigInternal(std::int32_t handle, const CameraConfig* config) {
    return sceCameraSetConfig(handle, config);
}

int APS5_VABI sceCameraGetDeviceConfig(std::int32_t handle, CameraConfig* config) {
    if (const int error = CheckConfig(handle, config)) return error;
    *config = {};
    config->sizeThis = sizeof(CameraConfig);
    return 0;
}

int APS5_VABI sceCameraGetDeviceConfigWithoutHandle(CameraConfig* config) {
    if (!ValidConfig(config)) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_OPEN;
    *config = {};
    return 0;
}

}
