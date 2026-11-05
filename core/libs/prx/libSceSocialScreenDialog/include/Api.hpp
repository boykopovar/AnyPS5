#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSocialScreenDialogInitialize(void);
int APS5_VABI sceSocialScreenDialogOpen(const void* param);
int APS5_VABI sceSocialScreenDialogUpdateStatus(void);
int APS5_VABI sceSocialScreenDialogGetStatus(void);
int APS5_VABI sceSocialScreenDialogGetResult(void* result);
int APS5_VABI sceSocialScreenDialogClose(void);
int APS5_VABI sceSocialScreenDialogTerminate(void);

}
