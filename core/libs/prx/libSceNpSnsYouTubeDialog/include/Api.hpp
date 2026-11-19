#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpSnsYouTubeDialogInitialize(void);
int APS5_VABI sceNpSnsYouTubeDialogOpen(const void* param);
int APS5_VABI sceNpSnsYouTubeDialogUpdateStatus(void);
int APS5_VABI sceNpSnsYouTubeDialogGetStatus(void);
int APS5_VABI sceNpSnsYouTubeDialogGetResult(void* result);
int APS5_VABI sceNpSnsYouTubeDialogClose(void);
int APS5_VABI sceNpSnsYouTubeDialogTerminate(void);

}
