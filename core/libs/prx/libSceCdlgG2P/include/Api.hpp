#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceG2PDialogInitialize(void);
int APS5_VABI sceG2PDialogOpen(const G2PDialogParam* param);
int APS5_VABI sceG2PDialogUpdateStatus(void);
int APS5_VABI sceG2PDialogGetStatus(void);
int APS5_VABI sceG2PDialogGetResult(CommonDialogResultPrefix* result);
int APS5_VABI sceG2PDialogClose(void);
int APS5_VABI sceG2PDialogTerminate(void);

}
