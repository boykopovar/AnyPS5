#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePlayGoDialogInitialize(void);
int APS5_VABI scePlayGoDialogTerminate(void);
int APS5_VABI scePlayGoDialogOpen(const void* param);
int APS5_VABI scePlayGoDialogClose(void);
int APS5_VABI scePlayGoDialogUpdateStatus(void);
int APS5_VABI scePlayGoDialogGetResult(PlayGoDialogResult* result);
int APS5_VABI scePlayGoDialogGetStatus(void);

}
