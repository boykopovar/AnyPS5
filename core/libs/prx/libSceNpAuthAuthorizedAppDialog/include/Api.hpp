#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpAuthAuthorizedAppDialogInitialize(void);
int APS5_VABI sceNpAuthAuthorizedAppDialogOpen(const NpAuthAuthorizedAppDialogParam* param);
int APS5_VABI sceNpAuthAuthorizedAppDialogUpdateStatus(void);
int APS5_VABI sceNpAuthAuthorizedAppDialogGetStatus(void);
int APS5_VABI sceNpAuthAuthorizedAppDialogGetResult(std::int32_t* result);
int APS5_VABI sceNpAuthAuthorizedAppDialogClose(void);
int APS5_VABI sceNpAuthAuthorizedAppDialogTerminate(void);

}
