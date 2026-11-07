#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceGameCustomDataDialogInitialize(void);
int APS5_VABI sceGameCustomDataDialogOpenA(const void* param);
int APS5_VABI sceGameCustomDataDialogUpdateStatus(void);
int APS5_VABI sceGameCustomDataDialogGetStatus(void);
int APS5_VABI sceGameCustomDataDialogGetResultA(void* result);
int APS5_VABI sceGameCustomDataDialogClose(void);
int APS5_VABI sceGameCustomDataDialogTerminate(void);
int APS5_VABI sceGameCustomDataDialogGetResult();
int APS5_VABI sceGameCustomDataDialogOpen();

}
