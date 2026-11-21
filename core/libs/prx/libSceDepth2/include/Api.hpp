#pragma once

#include "SceTypes.hpp"

struct SceDepth2Param {
    uint32_t size;
    uint32_t inputWidth;
    uint32_t inputHeight;
    uint32_t unknown0c;
    uint32_t outputWidth;
    uint32_t outputHeight;
    uint32_t unknown18;
    uint32_t type;
    uint32_t reserved20;
    uint32_t reserved24;
    uint32_t mode;
    uint32_t unknown2c;
    uint32_t unknown30;
    uint32_t unknown34;
    uint32_t unknown38;
    uint32_t unknown3c;
};

struct SceDepth2Memory {
    uint64_t size;
    uint64_t alignment;
};

extern "C" {

int APS5_VABI sceDepth2GetImage(int32_t handle, uint32_t argument1, void* image, uint32_t argument3);
int APS5_VABI sceDepth2Initialize(void);
int APS5_VABI sceDepth2QueryMemory(const SceDepth2Param* param, SceDepth2Memory* mem);
int APS5_VABI sceDepth2SaveInternalBuffers(int32_t handle);
int APS5_VABI sceDepth2SetCameraFactoryCalibrationData(int32_t handle);
int APS5_VABI sceDepth2SetCommand(void);
int APS5_VABI sceDepth2SetRoi(void);
int APS5_VABI sceDepth2Submit(int32_t handle);
int APS5_VABI sceDepth2Terminate(void);
int APS5_VABI sceDepth2WaitAndExecutePostProcess(int32_t handle);

}
