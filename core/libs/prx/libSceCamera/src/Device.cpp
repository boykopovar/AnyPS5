#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "prx/libSceCamera/libSceCamera.hpp"
#include "prx/libSceCamera/include/Camera.hpp"

extern "C" {
bool GuestCopyIn_nid_no_patch(void* destination, const void* source, std::size_t bytes);
bool GuestCopyOut_nid_no_patch(void* destination, const void* source, std::size_t bytes);
}

namespace {

constexpr std::uint32_t CALIBRATION_IMAGE_INVERSE_RECTIFICATION = 1;

int RefuseQuery(const void* out) {
    if (!out) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_INIT;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

}

extern "C" {

int APS5_VABI sceCameraGetDeviceInfo(std::int32_t reserved, const CameraDeviceInfo* deviceInfo) {
    if (reserved != 0 || !deviceInfo) return SCE_CAMERA_ERROR_PARAM;
    CameraDeviceInfo localInfo{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localInfo, deviceInfo, sizeof(localInfo));
    errno = savedErrno;
    if (!copied || localInfo.sizeThis != sizeof(CameraDeviceInfo) || localInfo.infoRevision != 1) return SCE_CAMERA_ERROR_PARAM;
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
    const std::uint32_t val = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(count, &val, sizeof(val));
    errno = savedErrno;
    if (!copied) return SCE_CAMERA_ERROR_PARAM;
    return 0;
}

int APS5_VABI sceCameraGetCalibDataFromDevice(void) {
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceCameraGetCalibrationData(const CameraGetCalibrationDataParameter* param, void* calibrationData) {
    if (!param || !calibrationData) return SCE_CAMERA_ERROR_PARAM;
    CameraGetCalibrationDataParameter localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
    errno = savedErrno;
    if (!copied || localParam.size != sizeof(CameraGetCalibrationDataParameter) || localParam.formatType != 0 ||
        localParam.functionType > CALIBRATION_IMAGE_INVERSE_RECTIFICATION)
        return SCE_CAMERA_ERROR_PARAM;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceCameraAudioOpen(void) {
    return SCE_CAMERA_ERROR_FATAL;
}

}
