#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_HMD2_ERROR_UNSUPPORTED_FEATURE = static_cast<std::int32_t>(0x81110016);

}

extern "C" {

std::int32_t APS5_VABI sceHmd2Initialize(const void* param) {
    (void)param;
    return SCE_HMD2_ERROR_UNSUPPORTED_FEATURE;
}

int APS5_VABI sceHmd2Close() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2GazeGetResultForFoveatedRendering() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2GetDeviceInformation() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2GetFieldOfViewWithoutHandle() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2Open() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionBeginFrame() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionDisableVrMode() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionEnableVrMode() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionGetPredictedDisplayTime() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionInitialize() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionQueryBufferSizeAlign() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionQueryDisplayBufferSizeAlign() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionSetAllowPositionalReprojection() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionSetParamWithBuffer() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2ReprojectionSetRenderConfig() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceHmd2SetVibration() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

}

