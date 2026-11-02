#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceLoginDialogInitialize(void);
int APS5_VABI sceLoginDialogOpen(const LoginDialogParam* param);
int APS5_VABI sceLoginDialogUpdateStatus(void);
int APS5_VABI sceLoginDialogGetStatus(void);
int APS5_VABI sceLoginDialogGetResult(LoginDialogResult* result);
int APS5_VABI sceLoginDialogClose(void);
int APS5_VABI sceLoginDialogTerminate(void);

}
