#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpEulaDialogCheckVersion();
int APS5_VABI sceNpEulaDialogCheckVersionAbort();
int APS5_VABI sceNpEulaDialogClose();
int APS5_VABI sceNpEulaDialogGetResult();
int APS5_VABI sceNpEulaDialogGetStatus();
int APS5_VABI sceNpEulaDialogInitialize();
int APS5_VABI sceNpEulaDialogOpen();
int APS5_VABI sceNpEulaDialogTerminate();
int APS5_VABI sceNpEulaDialogUpdateStatus();

}
