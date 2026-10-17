#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_HMD2_ERROR_UNSUPPORTED_FEATURE = static_cast<std::int32_t>(0x81110016);

}

extern "C" {

int APS5_VABI sceHmd2ReprojectionGetStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionQuerySeeThroughBufferSizeAlign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceHmd2Initialize(const void* param) {
    (void)param;
    return SCE_HMD2_ERROR_UNSUPPORTED_FEATURE;
}

int APS5_VABI sceHmd2GetFieldOfView() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GetDeviceInformationByHandle() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2Terminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2Close() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeGetResult() {
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

int APS5_VABI sceHmd2ReprojectionDisableMirroring() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionDisableVrMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionEnableMirroring() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionEnableVrMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionGetMirroringDisplayBufferSizeAlign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionGetMirroringWorkMemorySizeAlign() {
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

int APS5_VABI sceHmd2ReprojectionSetMirroringOption() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetParam() {
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

int APS5_VABI sceHmd2ReprojectionSetTiming() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2SetVibration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeGetCalibrationData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeGetCalibrationDataMaxSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionClearUserEventEnd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionClearUserEventStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionGetCurrentOutput() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionGetStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetUserEventEnd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetUserEventStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeActivateGazeTracking() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeGetDeviceStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeGetUserCalibrationStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeInsertRawDataWithHandle() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeSetEnabledEye() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeSetUserCalibrationData() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeSetUserCalibrationPoint() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeStartUserCalibration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeStopUserCalibration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSeeThroughDraw() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSeeThroughIC() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionVideoOutEq() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionWaitShader() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2SeeThroughCallbackGetPoseDataFunc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeObjectSelectionGetSelectedObject() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeObjectSelectionInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeObjectSelectionQueryMemory() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2GazeObjectSelectionTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionClear() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSet() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetEventGpuEnd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2ReprojectionSetEventGpuStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2RpeorjectionSetParam() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2SetFieldOfView() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHmd2SetIpdOffset() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
