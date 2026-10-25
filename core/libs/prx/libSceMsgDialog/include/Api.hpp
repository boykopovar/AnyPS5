#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMsgDialogInitialize(void);
int APS5_VABI sceMsgDialogTerminate(void);
int APS5_VABI sceMsgDialogOpen(const void* param);
int APS5_VABI sceMsgDialogGetStatus(void);
int APS5_VABI sceMsgDialogUpdateStatus(void);
int APS5_VABI sceMsgDialogClose(void);
int APS5_VABI sceMsgDialogProgressBarInc(int target, std::uint32_t delta);
int APS5_VABI sceMsgDialogProgressBarSetMsg(int target, const char* msg);
int APS5_VABI sceMsgDialogProgressBarSetValue(int target, std::uint32_t rate);

}
