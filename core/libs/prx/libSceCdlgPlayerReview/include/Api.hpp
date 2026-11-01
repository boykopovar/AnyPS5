#pragma once

#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePlayerReviewDialogInitialize(void);
int APS5_VABI scePlayerReviewDialogTerminate(void);
int APS5_VABI scePlayerReviewDialogOpen(const PlayerReviewDialogParam* param);
int APS5_VABI scePlayerReviewDialogClose(void);
int APS5_VABI scePlayerReviewDialogGetStatus(void);
int APS5_VABI scePlayerReviewDialogUpdateStatus(void);
int APS5_VABI scePlayerReviewDialogGetResult(CommonDialogResultPrefix* result);

}
