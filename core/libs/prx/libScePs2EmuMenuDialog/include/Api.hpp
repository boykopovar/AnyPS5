#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePs2EmuMenuDialogClose();
int APS5_VABI scePs2EmuMenuDialogGetResult();
int APS5_VABI scePs2EmuMenuDialogGetStatus();
int APS5_VABI scePs2EmuMenuDialogInitialize();
int APS5_VABI scePs2EmuMenuDialogOpen();
int APS5_VABI scePs2EmuMenuDialogTerminate();
int APS5_VABI scePs2EmuMenuDialogUpdateStatus();

}
