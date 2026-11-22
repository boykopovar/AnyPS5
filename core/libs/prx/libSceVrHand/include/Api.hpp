#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVrHandFinalize(void);
int APS5_VABI sceVrHandGetMesh(void);
int APS5_VABI sceVrHandGetMeshSize(void* size);
int APS5_VABI sceVrHandGetResult(void);
int APS5_VABI sceVrHandInitialize(const void* param);
int APS5_VABI sceVrHandQueryMemory(const void* param, void* result);
int APS5_VABI sceVrHandStart(const void* param);
int APS5_VABI sceVrHandStop(void);

}
