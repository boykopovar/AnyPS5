#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceDepth2GetImage(int32_t handle, uint32_t argument1, void* image, uint32_t argument3);
int APS5_VABI sceDepth2Initialize(void);
int APS5_VABI sceDepth2QueryMemory(void);
int APS5_VABI sceDepth2SaveInternalBuffers(int32_t handle);
int APS5_VABI sceDepth2SetCameraFactoryCalibrationData(int32_t handle);
int APS5_VABI sceDepth2SetCommand(void);
int APS5_VABI sceDepth2SetRoi(void);
int APS5_VABI sceDepth2Submit(int32_t handle);
int APS5_VABI sceDepth2Terminate(void);
int APS5_VABI sceDepth2WaitAndExecutePostProcess(int32_t handle);

}
