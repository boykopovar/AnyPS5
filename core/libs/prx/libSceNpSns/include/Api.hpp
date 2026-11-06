#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpSnsFacebookAbortRequest();
int APS5_VABI sceNpSnsIntAbortRequest();
int APS5_VABI sceNpSnsIntCreateRequest();
int APS5_VABI sceNpSnsIntDeleteRequest();
int APS5_VABI sceNpSnsIntFbGetGameAccessToken();
int APS5_VABI sceNpSnsIntFbGetGameAccessTokenAllowed();
int APS5_VABI sceNpSnsIntFbGetSystemAccessToken();
int APS5_VABI sceNpSnsIntFbGetTitleInfo();
int APS5_VABI sceNpSnsIntLinkedStatus();
int APS5_VABI sceNpSnsIntTwGetSystemAccessToken();
int APS5_VABI sceNpSnsIntUnlink();
int APS5_VABI sceNpSnsIntYtGetAccessToken();
int APS5_VABI sceNpSnsIntYtRefreshMasterToken();
int APS5_VABI sceNpSnsTwitchAbortRequest();
int APS5_VABI sceNpSnsTwitchCreateRequest();
int APS5_VABI sceNpSnsTwitchDeleteRequest();
int APS5_VABI sceNpSnsTwitchGetAccessToken();
int APS5_VABI sceNpSnsYouTubeAbortRequest();
int APS5_VABI sceNpSnsYouTubeCreateRequest();
int APS5_VABI sceNpSnsYouTubeDeleteRequest();
int APS5_VABI sceNpSnsYouTubeGetAccessToken();

}
