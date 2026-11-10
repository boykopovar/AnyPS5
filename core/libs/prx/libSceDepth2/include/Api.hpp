#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceDepth2GetImage(void);
int APS5_VABI sceDepth2Initialize(void);
int APS5_VABI sceDepth2QueryMemory(void);
int APS5_VABI sceDepth2SaveInternalBuffers(void);
int APS5_VABI sceDepth2SetCameraFactoryCalibrationData(void);
int APS5_VABI sceDepth2SetCommand(void);
int APS5_VABI sceDepth2SetRoi(void);
int APS5_VABI sceDepth2Submit(void);
int APS5_VABI sceDepth2Terminate(void);
int APS5_VABI sceDepth2WaitAndExecutePostProcess(void);

}
