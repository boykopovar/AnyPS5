#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpEulaDialogCheckVersion();
int APS5_VABI sceNpEulaDialogCheckVersionAbort();
int APS5_VABI sceNpEulaDialogInitialize(void);
int APS5_VABI sceNpEulaDialogOpen(const void* param);
int APS5_VABI sceNpEulaDialogUpdateStatus(void);
int APS5_VABI sceNpEulaDialogGetStatus(void);
int APS5_VABI sceNpEulaDialogGetResult(void* result);
int APS5_VABI sceNpEulaDialogClose(void);
int APS5_VABI sceNpEulaDialogTerminate(void);

}
