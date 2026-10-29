#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSaveDataDialogClose(const void* closeParam);
int APS5_VABI sceSaveDataDialogGetResult(void* result);
int APS5_VABI sceSaveDataDialogInitialize(void);
int APS5_VABI sceSaveDataDialogIsReadyToDisplay(void);
int APS5_VABI sceSaveDataDialogOpen(const void* param);
int APS5_VABI sceSaveDataDialogProgressBarInc(int target, uint32_t delta);
int APS5_VABI sceSaveDataDialogProgressBarSetValue(int target, uint32_t rate);
int APS5_VABI sceSaveDataDialogTerminate(void);

}
