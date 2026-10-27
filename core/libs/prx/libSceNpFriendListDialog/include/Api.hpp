#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpFriendListDialogInitialize(void);
int APS5_VABI sceNpFriendListDialogOpenA(const void* param);
int APS5_VABI sceNpFriendListDialogUpdateStatus(void);
int APS5_VABI sceNpFriendListDialogGetStatus(void);
int APS5_VABI sceNpFriendListDialogGetResultA(void* result);
int APS5_VABI sceNpFriendListDialogClose(void);
int APS5_VABI sceNpFriendListDialogTerminate(void);
int APS5_VABI sceNpFriendListDialogGetResult();
int APS5_VABI sceNpFriendListDialogOpen();

}
