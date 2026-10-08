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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeGetResultForFoveatedRendering() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GetDeviceInformation() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GetFieldOfViewWithoutHandle() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2Open() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionBeginFrame() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionDisableVrMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionEnableVrMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionGetPredictedDisplayTime() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionQueryBufferSizeAlign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionQueryDisplayBufferSizeAlign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetAllowPositionalReprojection() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetParamWithBuffer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetRenderConfig() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2SetVibration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
