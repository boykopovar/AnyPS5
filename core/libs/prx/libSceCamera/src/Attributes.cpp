#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libSceCamera/include/Camera.hpp"

namespace {

constexpr std::uint32_t CHANNEL_0 = 1;
constexpr std::uint32_t CHANNEL_BOTH = 3;
constexpr std::uint32_t AECAGC_TARGET_2_0 = 0x20;

struct CameraFramePosition {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t xSize;
    std::uint32_t ySize;
};

struct CameraExposureGain {
    std::uint32_t exposureControl;
    std::uint32_t exposure;
    std::uint32_t gain;
    std::uint32_t mode;
};

struct CameraWhiteBalance {
    std::uint32_t whiteBalanceControl;
    std::uint32_t gainRed;
    std::uint32_t gainBlue;
    std::uint32_t gainGreen;
};

struct CameraGamma {
    std::uint32_t gammaControl;
    std::uint32_t value;
    std::uint8_t reserved[16];
};

struct CameraAttribute {
    std::uint32_t sizeThis;
    std::uint32_t channel;
    CameraFramePosition framePosition;
    CameraExposureGain exposureGain;
    CameraWhiteBalance whiteBalance;
    CameraGamma gamma;
    std::uint32_t saturation;
    std::uint32_t contrast;
    std::uint32_t sharpness;
    std::int32_t hue;
    std::uint32_t reserved[4];
};
static_assert(sizeof(CameraAttribute) == 112);

struct CameraAutoExposureGainTarget {
    std::uint32_t sizeThis;
    std::uint32_t target;
};

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
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_OPEN;
    return 0;
}

int RefuseSet(std::int32_t handle, std::uint32_t channel, bool valid) {
    if (handle < 1 || !Writable(channel) || !valid) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_OPEN;
    return SCE_CAMERA_ERROR_NOT_CONNECTED;
}

bool ValidTarget(const CameraAutoExposureGainTarget* option) {
    if (!option) return true;
    return option->sizeThis == sizeof(CameraAutoExposureGainTarget) && option->target % 2 == 0 && option->target <= AECAGC_TARGET_2_0;
}

template <typename TValue>
int Report(std::int32_t handle, std::uint32_t channel, TValue* out, const void* option, TValue value) {
    if (const int error = CheckGet(handle, channel, out, option)) return error;
    *out = value;
    return 0;
}

}

extern "C" {

int APS5_VABI sceCameraGetAttribute(std::int32_t handle, CameraAttribute* attribute) {
    if (handle < 1 || !attribute || attribute->sizeThis != sizeof(CameraAttribute) || !Readable(attribute->channel)) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_OPEN;
    const std::uint32_t channel = attribute->channel;
    *attribute = {};
    attribute->sizeThis = sizeof(CameraAttribute);
    attribute->channel = channel;
    attribute->exposureGain.exposure = 83;
    attribute->exposureGain.gain = 100;
    attribute->whiteBalance = DEFAULT_WHITE_BALANCE;
    attribute->gamma.value = DEFAULT_GAMMA;
    attribute->saturation = DEFAULT_SATURATION;
    attribute->contrast = DEFAULT_CONTRAST;
    attribute->sharpness = DEFAULT_SHARPNESS;
    attribute->hue = DEFAULT_HUE;
    return 0;
}

int APS5_VABI sceCameraGetAutoExposureGain(std::int32_t handle, std::uint32_t channel, std::uint32_t* enable, CameraAutoExposureGainTarget* option) {
    if (handle < 1 || !Readable(channel) || !enable) return SCE_CAMERA_ERROR_PARAM;
    if (option && option->sizeThis != sizeof(CameraAutoExposureGainTarget)) return SCE_CAMERA_ERROR_PARAM;
    if (!Camera::IsOpen()) return SCE_CAMERA_ERROR_NOT_OPEN;
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
    return Report(handle, channel, enable, option, 0u);
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
    return Report(handle, channel, whiteBalance, option, DEFAULT_WHITE_BALANCE);
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
