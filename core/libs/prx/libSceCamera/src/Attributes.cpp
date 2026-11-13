#include <cerrno>
#include <cstdint>
#include <cstring>
#include "prx/libc/include/General.hpp"
#include "prx/libSceCamera/libSceCamera.hpp"
#include "prx/libSceCamera/include/Camera.hpp"

namespace {

constexpr std::uint32_t CHANNEL_0 = 1;
constexpr std::uint32_t CHANNEL_BOTH = 3;

constexpr std::uint32_t DEFAULT_CONTRAST = 32;
constexpr std::uint32_t DEFAULT_SATURATION = 64;
constexpr std::uint32_t DEFAULT_SHARPNESS = 1;
constexpr std::int32_t DEFAULT_HUE = 1;
constexpr std::uint32_t DEFAULT_GAMMA = 4;
constexpr CameraWhiteBalance DEFAULT_WHITE_BALANCE{0, 768, 768, 512};

bool Readable(std::uint32_t channel) { return channel >= CHANNEL_0 && channel < CHANNEL_BOTH; }

bool Writable(std::uint32_t channel) { return channel >= CHANNEL_0 && channel <= CHANNEL_BOTH; }

int CheckGet(std::int32_t handle, std::uint32_t channel, const void* out, const void* option) {
    if (handle < 1 || !Readable(channel) || !out || option) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen(handle)) return SCE_CAMERA_ERROR_NOT_OPEN;
    return 0;
}

int RefuseSet(std::int32_t handle, std::uint32_t channel, bool valid) {
    if (handle < 1 || !Writable(channel) || !valid) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen(handle)) return SCE_CAMERA_ERROR_NOT_OPEN;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

bool ValidTarget(const CameraAutoExposureGainTarget* option) {
    if (!option) return true;
    if (option->sizeThis != sizeof(CameraAutoExposureGainTarget)) return false;
    switch (option->target) {
    case 0x0: case 0x2: case 0x4: case 0x6: case 0x8:
    case 0x10: case 0x12: case 0x14: case 0x16:
    case 0x20:
        return true;
    default:
        return false;
    }
}

template <typename TValue>
int Report(std::int32_t handle, std::uint32_t channel, TValue* out, const void* option, TValue value) {
    if (const int error = CheckGet(handle, channel, out, option)) return error;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(out, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return SCE_CAMERA_ERROR_PARAM;
    return 0;
}

}

extern "C" {

int APS5_VABI sceCameraGetAttribute(std::int32_t handle, CameraAttribute* attribute) {
    if (handle < 1 || !attribute) return SCE_CAMERA_ERROR_PARAM;
    CameraAttribute localAttr{};
    const int savedErrno = errno;
    const bool copiedIn = GuestCopyIn_nid_no_patch(&localAttr, attribute, sizeof(localAttr));
    errno = savedErrno;
    if (!copiedIn || localAttr.sizeThis != sizeof(CameraAttribute) || !Readable(localAttr.channel)) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen(handle)) return SCE_CAMERA_ERROR_NOT_OPEN;
    const std::uint32_t channel = localAttr.channel;
    localAttr = {};
    localAttr.sizeThis = sizeof(CameraAttribute);
    localAttr.channel = channel;
    localAttr.exposureGain.exposure = 83;
    localAttr.exposureGain.gain = 100;
    localAttr.whiteBalance = DEFAULT_WHITE_BALANCE;
    localAttr.gamma.value = DEFAULT_GAMMA;
    localAttr.saturation = DEFAULT_SATURATION;
    localAttr.contrast = DEFAULT_CONTRAST;
    localAttr.sharpness = DEFAULT_SHARPNESS;
    localAttr.hue = DEFAULT_HUE;
    const bool copiedOut = GuestCopyOut_nid_no_patch(attribute, &localAttr, sizeof(localAttr));
    errno = savedErrno;
    if (!copiedOut) return SCE_CAMERA_ERROR_PARAM;
    return 0;
}

int APS5_VABI sceCameraGetAutoExposureGain(std::int32_t handle, std::uint32_t channel, std::uint32_t* enable, CameraAutoExposureGainTarget* option) {
    if (handle < 1 || !Readable(channel) || !enable) return SCE_CAMERA_ERROR_PARAM;
    if (option && option->sizeThis != sizeof(CameraAutoExposureGainTarget)) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen(handle)) return SCE_CAMERA_ERROR_NOT_OPEN;
    *enable = 0;
    if (option) *option = {};
    return 0;
}

int APS5_VABI sceCameraGetAutoWhiteBalance(std::int32_t handle, std::uint32_t channel, std::uint32_t* enable, void* option) {
    return Report(handle, channel, enable, option, 0u);
}

int APS5_VABI sceCameraGetDefectivePixelCancellation(std::int32_t handle, std::uint32_t channel, std::uint32_t* enable, void* option) {
    return Report(handle, channel, enable, option, 0u);
}

int APS5_VABI sceCameraGetLensCorrection(std::int32_t handle, std::uint32_t channel, std::uint32_t* enable, void* option) {
    if (const int error = CheckGet(handle, channel, enable, option)) return error;
    const std::uint32_t value = 0u;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(enable, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return SCE_CAMERA_ERROR_PARAM;
    return 0;
}

int APS5_VABI sceCameraGetContrast(std::int32_t handle, std::uint32_t channel, std::uint32_t* contrast, void* option) {
    return Report(handle, channel, contrast, option, DEFAULT_CONTRAST);
}

int APS5_VABI sceCameraGetSaturation(std::int32_t handle, std::uint32_t channel, std::uint32_t* saturation, void* option) {
    return Report(handle, channel, saturation, option, DEFAULT_SATURATION);
}

int APS5_VABI sceCameraGetSharpness(std::int32_t handle, std::uint32_t channel, std::uint32_t* sharpness, void* option) {
    return Report(handle, channel, sharpness, option, DEFAULT_SHARPNESS);
}

int APS5_VABI sceCameraGetHue(std::int32_t handle, std::uint32_t channel, std::int32_t* hue, void* option) {
    return Report(handle, channel, hue, option, DEFAULT_HUE);
}

int APS5_VABI sceCameraGetExposureGain(std::int32_t handle, std::uint32_t channel, CameraExposureGain* exposureGain, void* option) {
    return Report(handle, channel, exposureGain, option, CameraExposureGain{0, 20, 100, 0});
}

int APS5_VABI sceCameraGetWhiteBalance(std::int32_t handle, std::uint32_t channel, CameraWhiteBalance* whiteBalance, void* option) {
    if (const int error = CheckGet(handle, channel, whiteBalance, option)) return error;
    const CameraWhiteBalance value = DEFAULT_WHITE_BALANCE;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(whiteBalance, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return SCE_CAMERA_ERROR_PARAM;
    return 0;
}

int APS5_VABI sceCameraGetGamma(std::int32_t handle, std::uint32_t channel, CameraGamma* gamma, void* option) {
    CameraGamma value{};
    value.value = DEFAULT_GAMMA;
    return Report(handle, channel, gamma, option, value);
}

int APS5_VABI sceCameraSetAttribute(std::int32_t handle, const CameraAttribute* attribute) {
    if (!attribute || attribute->sizeThis != sizeof(CameraAttribute)) return SCE_CAMERA_ERROR_PARAM;
    return RefuseSet(handle, attribute->channel, true);
}

int APS5_VABI sceCameraSetAutoExposureGain(std::int32_t handle, std::uint32_t channel, std::uint32_t, const CameraAutoExposureGainTarget* option) {
    return RefuseSet(handle, channel, ValidTarget(option));
}

int APS5_VABI sceCameraSetAutoWhiteBalance(std::int32_t handle, std::uint32_t channel, std::uint32_t, const void* option) {
    return RefuseSet(handle, channel, !option);
}

int APS5_VABI sceCameraSetDefectivePixelCancellation(std::int32_t handle, std::uint32_t channel, std::uint32_t enable, const void* option) {
    return RefuseSet(handle, channel, enable <= 1 && !option);
}

int APS5_VABI sceCameraSetDefectivePixelCancellationInternal(std::int32_t handle, std::uint32_t channel, std::uint32_t enable, const void* option) {
    return RefuseSet(handle, channel, enable <= 2 && !option);
}

int APS5_VABI sceCameraSetLensCorrection(std::int32_t handle, std::uint32_t channel, std::uint32_t enable, const void* option) {
    return RefuseSet(handle, channel, enable <= 1 && !option);
}

int APS5_VABI sceCameraSetLensCorrectionInternal(std::int32_t handle, std::uint32_t channel, std::uint32_t enable, const void* option) {
    return RefuseSet(handle, channel, enable <= 2 && !option);
}

int APS5_VABI sceCameraSetContrast(std::int32_t handle, std::uint32_t channel, std::uint32_t, const void* option) {
    return RefuseSet(handle, channel, !option);
}

int APS5_VABI sceCameraSetSaturation(std::int32_t handle, std::uint32_t channel, std::uint32_t, const void* option) {
    return RefuseSet(handle, channel, !option);
}

int APS5_VABI sceCameraSetSharpness(std::int32_t handle, std::uint32_t channel, std::uint32_t sharpness, const void* option) {
    return RefuseSet(handle, channel, sharpness <= 10 && !option);
}

int APS5_VABI sceCameraSetHue(std::int32_t handle, std::uint32_t channel, std::int32_t, const void* option) {
    return RefuseSet(handle, channel, !option);
}

int APS5_VABI sceCameraSetExposureGain(std::int32_t handle, std::uint32_t channel, const CameraExposureGain* exposureGain, const void* option) {
    return RefuseSet(handle, channel, exposureGain && !option);
}

int APS5_VABI sceCameraSetWhiteBalance(std::int32_t handle, std::uint32_t channel, const CameraWhiteBalance* whiteBalance, const void* option) {
    return RefuseSet(handle, channel, whiteBalance && !option);
}

int APS5_VABI sceCameraSetGamma(std::int32_t handle, std::uint32_t channel, const CameraGamma* gamma, const void* option) {
    return RefuseSet(handle, channel, gamma && !option);
}

}
