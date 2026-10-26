#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceUsbStorageDialogInitialize(void);
int APS5_VABI sceUsbStorageDialogOpen(const UsbStorageDialogParam* param);
int APS5_VABI sceUsbStorageDialogGetStatus(void);
int APS5_VABI sceUsbStorageDialogUpdateStatus(void);
int APS5_VABI sceUsbStorageDialogGetResult(UsbStorageDialogResult* result);
int APS5_VABI sceUsbStorageDialogClose(void);
int APS5_VABI sceUsbStorageDialogTerminate(void);

}
