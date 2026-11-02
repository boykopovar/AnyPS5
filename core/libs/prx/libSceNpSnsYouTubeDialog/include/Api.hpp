#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpSnsYouTubeDialogClose();
int APS5_VABI sceNpSnsYouTubeDialogGetResult();
int APS5_VABI sceNpSnsYouTubeDialogGetStatus();
int APS5_VABI sceNpSnsYouTubeDialogInitialize();
int APS5_VABI sceNpSnsYouTubeDialogOpen();
int APS5_VABI sceNpSnsYouTubeDialogTerminate();
int APS5_VABI sceNpSnsYouTubeDialogUpdateStatus();

}
