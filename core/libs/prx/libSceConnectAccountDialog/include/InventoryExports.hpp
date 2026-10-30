#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceConnectAccountDialogClose();
int APS5_VABI sceConnectAccountDialogGetResult();
int APS5_VABI sceConnectAccountDialogGetStatus();
int APS5_VABI sceConnectAccountDialogInitialize();
int APS5_VABI sceConnectAccountDialogOpen();
int APS5_VABI sceConnectAccountDialogTerminate();
int APS5_VABI sceConnectAccountDialogUpdateStatus();

}
