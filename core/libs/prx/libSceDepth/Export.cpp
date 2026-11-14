#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceDepth/include/InventoryExports.hpp"
#include "prx/libSceDepth/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidHandle = static_cast<int>(0x80BB0005u);
constexpr int ResultCameraFactoryCalibrationFailure = static_cast<int>(0x80BBFFFFu);

bool IsOpenHandle(int32_t handle) {
    (void)handle;
    return false;
}

} // namespace

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI sceDepthCalculateRectificationInformation(void) {
    return 0;
}

int APS5_VABI sceDepthDisableHeteroMode(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthEnableExtendedMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthEnableHeteroMode(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthEnableHeteroModeAndGenerateInformation(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthGetCalibrationData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthGetImage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthGetRectificationInformation(void) {
    return 0;
}

int APS5_VABI sceDepthGetSrParameter(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthHandCandidateTrackerGetResult(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthHasCalibrationData(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthHeadCandidateDetectorGetResults(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthHeadCandidateTrackerGetResult(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthHeadCandidateTrackerSetValidationInformation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthInitialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthLoadCalibrationData(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthQueryMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSaveInternalBuffers(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSetCalibrationData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSetCommands(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSetPostFilterParameter(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSetPreFilterParameter(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSetRectificationInformation(void) {
    return 0;
}

int APS5_VABI sceDepthSetRoi(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSetRoiset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSetSrParameter(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSetSrTnrParameter(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthSubmit(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthTerminate(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthUpdate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthValidateCurrentCalibrationData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDepthWaitAndExecutePostProcess(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
extern "C" {

int APS5_VABI _sceDepthCalcHeteroAdjustParam() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceDepthHeadCandidateTrackerGetInformation() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceDepthHeadCandidateTrackerSetInformation() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceDepthSetCameraFactoryCalibrationData(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ResultCameraFactoryCalibrationFailure;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceDepthSetHeteroAdjustParam(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceDepthSetHeteroMode(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceDepthUnknown_unL9sWu4IIw_nid_no_patch(int32_t handle) {
    if (!IsOpenHandle(handle)) {
        return ErrorInvalidHandle;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("unL9sWu4IIw", libSceDepthUnknown_unL9sWu4IIw_nid_no_patch);

}
