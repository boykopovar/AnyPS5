#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVrHandFinalize();
int APS5_VABI sceVrHandGetMesh();
int APS5_VABI sceVrHandGetMeshSize();
int APS5_VABI sceVrHandGetResult();
int APS5_VABI sceVrHandInitialize();
int APS5_VABI sceVrHandQueryMemory();
int APS5_VABI sceVrHandStart();
int APS5_VABI sceVrHandStop();

}
