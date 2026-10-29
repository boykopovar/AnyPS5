#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNetCtlApDialogInitialize(void);
int APS5_VABI sceNetCtlApDialogTerminate(void);
int APS5_VABI sceNetCtlApDialogOpen(const NetCtlApDialogParam* param);
int APS5_VABI sceNetCtlApDialogClose(void);
int APS5_VABI sceNetCtlApDialogUpdateStatus(void);
int APS5_VABI sceNetCtlApDialogGetStatus(void);
int APS5_VABI sceNetCtlApDialogGetResult(NetCtlApDialogResult* result);

}
