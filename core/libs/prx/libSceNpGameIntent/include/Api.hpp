#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpGameIntentGetPropertyValueString(const NpGameIntentData* intentData, const char* key, char* valueBuf, size_t bufSize);
int APS5_VABI sceNpGameIntentInitialize(const void* initParam);
int APS5_VABI sceNpGameIntentReceiveIntent(NpGameIntentInfo* intentInfo);
int APS5_VABI sceNpGameIntentTerminate(void);
int APS5_VABI sceNpGameIntentInitParamInit(NpGameIntentInitParam* param);
int APS5_VABI sceNpGameIntentInfoInit(NpGameIntentInfo* info);
int APS5_VABI sceNpGameIntentVshInitialize(void);
int APS5_VABI sceNpGameIntentVshTerminate(void);
int APS5_VABI sceNpAppLauncherInitialize(const void* initParam);
int APS5_VABI sceNpAppLauncherLaunchApp(std::int32_t arg0, const char* arg2, const char* arg3);
int APS5_VABI sceNpAppLauncherTerminate();
int APS5_VABI sceNpGameIntentGetUdsNpComId();
int APS5_VABI sceNpGameIntentLaunchApp();
int APS5_VABI sceNpGameIntentLaunchApp2();
int APS5_VABI sceNpGameIntentNotifyAppLaunched();
int APS5_VABI sceNpSessionServiceGetJoinedPartySession();
int APS5_VABI sceNpSessionServiceGetRunningGamePlayerSession();
int APS5_VABI sceNpSessionServiceInitialize();
int APS5_VABI sceNpSessionServiceSendPlayerSessionInvitations();
int APS5_VABI sceNpSessionServiceSendPlayerSessionRequestToJoin();
int APS5_VABI sceNpSessionServiceTerminate();

}
