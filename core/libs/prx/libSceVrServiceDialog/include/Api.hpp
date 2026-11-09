#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVrServiceDialogInitialize(void);
int APS5_VABI sceVrServiceDialogOpen(const void* param);
int APS5_VABI sceVrServiceDialogUpdateStatus(void);
int APS5_VABI sceVrServiceDialogGetStatus(void);
int APS5_VABI sceVrServiceDialogGetResult(void* result);
int APS5_VABI sceVrServiceDialogClose(void);
int APS5_VABI sceVrServiceDialogTerminate(void);

}
