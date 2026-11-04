#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSigninDialogClose(void);
int APS5_VABI sceSigninDialogGetResult(void* result);
int APS5_VABI sceSigninDialogGetStatus(void);
int APS5_VABI sceSigninDialogInitialize(void);
int APS5_VABI sceSigninDialogOpen(const void* param);
int APS5_VABI sceSigninDialogTerminate(void);
int APS5_VABI sceSigninDialogUpdateStatus(void);

}
