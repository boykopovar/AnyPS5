#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpSnsFacebookAbortRequest(int reqId);
int APS5_VABI sceNpSnsIntAbortRequest(int reqId);
int APS5_VABI sceNpSnsIntCreateRequest(void);
int APS5_VABI sceNpSnsIntDeleteRequest(int reqId);
int APS5_VABI sceNpSnsIntFbGetGameAccessToken(int reqId, int userId, const void* arg3, const void* arg4, const void* arg5, int* outFlag, void* outBuf, void* outToken);
int APS5_VABI sceNpSnsIntFbGetGameAccessTokenAllowed(int reqId, int userId, const void* arg3, const void* arg4, const void* arg5, void* outAllowed);
int APS5_VABI sceNpSnsIntFbGetSystemAccessToken(int reqId, int userId, void* outToken);
int APS5_VABI sceNpSnsIntFbGetTitleInfo(const void* arg1, const void* arg2, void* outInfo);
int APS5_VABI sceNpSnsIntLinkedStatus(int reqId, int userId, int serviceType, void* outStatus);
int APS5_VABI sceNpSnsIntTwGetSystemAccessToken(int reqId, int userId, void* outToken);
int APS5_VABI sceNpSnsIntUnlink(int reqId, int userId, int serviceType);
int APS5_VABI sceNpSnsIntYtGetAccessToken(int reqId, int userId, const char* arg3, const char* arg4, int* outFlag, void* outBuf, void* outToken);
int APS5_VABI sceNpSnsIntYtRefreshMasterToken(int reqId, int userId, const void* arg3);
int APS5_VABI sceNpSnsTwitchAbortRequest(int reqId);
int APS5_VABI sceNpSnsTwitchCreateRequest(void);
int APS5_VABI sceNpSnsTwitchDeleteRequest(int reqId);
int APS5_VABI sceNpSnsTwitchGetAccessToken(int reqId, const void* param, void* outToken);
int APS5_VABI sceNpSnsYouTubeAbortRequest(int reqId);
int APS5_VABI sceNpSnsYouTubeCreateRequest(void);
int APS5_VABI sceNpSnsYouTubeDeleteRequest(int reqId);
int APS5_VABI sceNpSnsYouTubeGetAccessToken(int reqId, const void* param, void* outToken);

}