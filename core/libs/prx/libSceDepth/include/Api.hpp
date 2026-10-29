#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceDepthCalculateRectificationInformation(void);
int APS5_VABI sceDepthDisableHeteroMode(void);
int APS5_VABI sceDepthEnableExtendedMode(void);
int APS5_VABI sceDepthEnableHeteroMode(void);
int APS5_VABI sceDepthEnableHeteroModeAndGenerateInformation(void);
int APS5_VABI sceDepthGetCalibrationData(void);
int APS5_VABI sceDepthGetImage(void);
int APS5_VABI sceDepthGetRectificationInformation(void);
int APS5_VABI sceDepthGetSrParameter(void);
int APS5_VABI sceDepthHandCandidateTrackerGetResult(void);
int APS5_VABI sceDepthHasCalibrationData(void);
int APS5_VABI sceDepthHeadCandidateDetectorGetResults(void);
int APS5_VABI sceDepthHeadCandidateTrackerGetResult(void);
int APS5_VABI sceDepthHeadCandidateTrackerSetValidationInformation(void);
int APS5_VABI sceDepthInitialize(void);
int APS5_VABI sceDepthLoadCalibrationData(void);
int APS5_VABI sceDepthQueryMemory(void);
int APS5_VABI sceDepthSaveInternalBuffers(void);
int APS5_VABI sceDepthSetCalibrationData(void);
int APS5_VABI sceDepthSetCommands(void);
int APS5_VABI sceDepthSetPostFilterParameter(void);
int APS5_VABI sceDepthSetPreFilterParameter(void);
int APS5_VABI sceDepthSetRectificationInformation(void);
int APS5_VABI sceDepthSetRoi(void);
int APS5_VABI sceDepthSetRoiset(void);
int APS5_VABI sceDepthSetSrParameter(void);
int APS5_VABI sceDepthSetSrTnrParameter(void);
int APS5_VABI sceDepthSubmit(void);
int APS5_VABI sceDepthTerminate(void);
int APS5_VABI sceDepthUpdate(void);
int APS5_VABI sceDepthValidateCurrentCalibrationData(void);
int APS5_VABI sceDepthWaitAndExecutePostProcess(void);
int APS5_VABI _sceDepthCalcHeteroAdjustParam();
int APS5_VABI _sceDepthHeadCandidateTrackerGetInformation();
int APS5_VABI _sceDepthHeadCandidateTrackerSetInformation();
int APS5_VABI _sceDepthSetCameraFactoryCalibrationData();
int APS5_VABI _sceDepthSetHeteroAdjustParam();
int APS5_VABI _sceDepthSetHeteroMode();

}
