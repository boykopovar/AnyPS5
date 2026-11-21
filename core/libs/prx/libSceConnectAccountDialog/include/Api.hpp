#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceConnectAccountDialogInitialize(void);
int APS5_VABI sceConnectAccountDialogOpen(const void* param);
int APS5_VABI sceConnectAccountDialogUpdateStatus(void);
int APS5_VABI sceConnectAccountDialogGetStatus(void);
int APS5_VABI sceConnectAccountDialogGetResult(void* result);
int APS5_VABI sceConnectAccountDialogClose(void);
int APS5_VABI sceConnectAccountDialogTerminate(void);

}
