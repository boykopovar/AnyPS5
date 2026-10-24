#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceHmdSetupDialogInitialize(void);
int APS5_VABI sceHmdSetupDialogOpen(const void* param);
int APS5_VABI sceHmdSetupDialogUpdateStatus(void);
int APS5_VABI sceHmdSetupDialogGetStatus(void);
int APS5_VABI sceHmdSetupDialogGetResult(void* result);
int APS5_VABI sceHmdSetupDialogClose(void);
int APS5_VABI sceHmdSetupDialogTerminate(void);

}
