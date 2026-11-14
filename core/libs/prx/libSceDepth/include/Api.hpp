#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceDepthCalculateRectificationInformation(void);
int APS5_VABI sceDepthDisableHeteroMode(int32_t handle);
int APS5_VABI sceDepthEnableExtendedMode(void);
int APS5_VABI sceDepthEnableHeteroMode(int32_t handle);
int APS5_VABI sceDepthEnableHeteroModeAndGenerateInformation(int32_t handle);
int APS5_VABI sceDepthGetCalibrationData(void);
int APS5_VABI sceDepthGetImage(void);
int APS5_VABI sceDepthGetRectificationInformation(void);
int APS5_VABI sceDepthGetSrParameter(int32_t handle);
int APS5_VABI sceDepthHandCandidateTrackerGetResult(void);
int APS5_VABI sceDepthHasCalibrationData(int32_t handle);
int APS5_VABI sceDepthHeadCandidateDetectorGetResults(int32_t handle);
int APS5_VABI sceDepthHeadCandidateTrackerGetResult(void);
int APS5_VABI sceDepthHeadCandidateTrackerSetValidationInformation(void);
int APS5_VABI sceDepthInitialize(void);
int APS5_VABI sceDepthLoadCalibrationData(int32_t handle);
int APS5_VABI sceDepthQueryMemory(void);
int APS5_VABI sceDepthSaveInternalBuffers(int32_t handle);
int APS5_VABI sceDepthSetCalibrationData(void);
int APS5_VABI sceDepthSetCommands(void);
int APS5_VABI sceDepthSetPostFilterParameter(int32_t handle);
int APS5_VABI sceDepthSetPreFilterParameter(int32_t handle);
int APS5_VABI sceDepthSetRectificationInformation(void);
int APS5_VABI sceDepthSetRoi(void);
int APS5_VABI sceDepthSetRoiset(void);
int APS5_VABI sceDepthSetSrParameter(int32_t handle);
int APS5_VABI sceDepthSetSrTnrParameter(int32_t handle);
int APS5_VABI sceDepthSubmit(int32_t handle);
int APS5_VABI sceDepthTerminate(int32_t handle);
int APS5_VABI sceDepthUpdate(void);
int APS5_VABI sceDepthValidateCurrentCalibrationData(void);
int APS5_VABI sceDepthWaitAndExecutePostProcess(int32_t handle);
int APS5_VABI _sceDepthCalcHeteroAdjustParam();
int APS5_VABI _sceDepthHeadCandidateTrackerGetInformation();
int APS5_VABI _sceDepthHeadCandidateTrackerSetInformation();
int APS5_VABI _sceDepthSetCameraFactoryCalibrationData(int32_t handle);
int APS5_VABI _sceDepthSetHeteroAdjustParam(int32_t handle);
int APS5_VABI _sceDepthSetHeteroMode(int32_t handle);

}
