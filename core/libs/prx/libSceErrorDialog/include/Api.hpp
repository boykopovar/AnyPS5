#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceErrorDialogInitialize(void);
int APS5_VABI sceErrorDialogOpen(const void* param);
int APS5_VABI sceErrorDialogUpdateStatus(void);
int APS5_VABI sceErrorDialogGetStatus(void);
int APS5_VABI sceErrorDialogClose(void);
int APS5_VABI sceErrorDialogTerminate(void);
int APS5_VABI sceErrorDialogOpenDetail();
int APS5_VABI sceErrorDialogOpenWithReport();

}
