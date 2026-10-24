#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libSceCamera/include/Camera.hpp"

namespace {

struct CameraDeviceInfo {
    std::uint32_t sizeThis;
    std::uint32_t infoRevision;
    std::uint32_t deviceRevision;
    std::uint32_t padding;
};

struct CameraGetCalibrationDataParameter {
    std::uint32_t size;
    std::uint32_t formatType;
    std::uint32_t functionType;
};

constexpr std::uint32_t CALIBRATION_IMAGE_INVERSE_RECTIFICATION = 1;

int RefuseQuery(const void* out) {
    if (!out) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_INIT;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

}

extern "C" {

int APS5_VABI sceCameraGetDeviceInfo(std::int32_t reserved, const CameraDeviceInfo* deviceInfo) {
    if (reserved != 0 || !deviceInfo || deviceInfo->sizeThis != sizeof(CameraDeviceInfo) || deviceInfo->infoRevision != 1) return SCE_CAMERA_ERROR_PARAM;
    return RefuseQuery(deviceInfo);
}

int APS5_VABI sceCameraGetProductInfo(void* productInfo) {
    return RefuseQuery(productInfo);
}

int APS5_VABI sceCameraGetRegistryInfo(void* registryInfo) {
    return RefuseQuery(registryInfo);
}

int APS5_VABI sceCameraGetMmapConnectedCount(std::uint32_t* count) {
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_OPEN;
    if (!count) return SCE_CAMERA_ERROR_PARAM;
    *count = 0;
    return 0;
}

int APS5_VABI sceCameraGetCalibDataFromDevice(void) {
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceCameraGetCalibrationData(const CameraGetCalibrationDataParameter* param, void* calibrationData) {
    if (!param || !calibrationData || param->size != sizeof(CameraGetCalibrationDataParameter) || param->formatType != 0 ||
        param->functionType > CALIBRATION_IMAGE_INVERSE_RECTIFICATION)
        return SCE_CAMERA_ERROR_PARAM;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceCameraAudioOpen(void) {
    return SCE_CAMERA_ERROR_FATAL;
}

}
