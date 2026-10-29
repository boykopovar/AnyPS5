#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVrSetupDialogInitialize(void);
int APS5_VABI sceVrSetupDialogOpen(const VrSetupDialogParam* param);
int APS5_VABI sceVrSetupDialogUpdateStatus(void);
int APS5_VABI sceVrSetupDialogGetStatus(void);
int APS5_VABI sceVrSetupDialogGetResult(std::int32_t* result);
int APS5_VABI sceVrSetupDialogClose(void);
int APS5_VABI sceVrSetupDialogTerminate(void);

}
