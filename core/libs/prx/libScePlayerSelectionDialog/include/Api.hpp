#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePlayerSelectionDialogInitialize(void);
int APS5_VABI scePlayerSelectionDialogOpen(const PlayerSelectionDialogParam* param);
int APS5_VABI scePlayerSelectionDialogClose(void);
int APS5_VABI scePlayerSelectionDialogUpdateStatus(void);
int APS5_VABI scePlayerSelectionDialogGetStatus(void);
int APS5_VABI scePlayerSelectionDialogGetResult(PlayerSelectionDialogResult* result);
int APS5_VABI scePlayerSelectionDialogParamInitialize(PlayerSelectionDialogParam* param);
int APS5_VABI scePlayerSelectionDialogTerminate(void);

}
