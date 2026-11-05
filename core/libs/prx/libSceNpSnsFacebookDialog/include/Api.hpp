#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpSnsFacebookDialogInitialize(void);
int APS5_VABI sceNpSnsFacebookDialogOpen(const void* param);
int APS5_VABI sceNpSnsFacebookDialogUpdateStatus(void);
int APS5_VABI sceNpSnsFacebookDialogGetStatus(void);
int APS5_VABI sceNpSnsFacebookDialogGetResult(void* result);
int APS5_VABI sceNpSnsFacebookDialogClose(void);
int APS5_VABI sceNpSnsFacebookDialogTerminate(void);

}
