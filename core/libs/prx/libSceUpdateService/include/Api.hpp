#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceUpsrvFlushHttpCookie();

int APS5_VABI sceUpsrvGetCompletedTaskId();
int APS5_VABI sceUpsrvGetRebootFlag();
int APS5_VABI sceUpsrvGetSystemExUpdateTaskState();
int APS5_VABI sceUpsrvInitialize();
int APS5_VABI sceUpsrvPostErrorLog();
int APS5_VABI sceUpsrvResumePausedTask();
int APS5_VABI sceUpsrvSetFakeVersion();
int APS5_VABI sceUpsrvStartUpdateSession();
int APS5_VABI sceUpsrvSwitchSystemExBank();
int APS5_VABI sceUpsrvSwitchSystemExBankWithoutVerify();
int APS5_VABI sceUpsrvTerminate();
int APS5_VABI sceUpsrvUpdateCancelBDUpdate();
int APS5_VABI sceUpsrvUpdateCheckCancel();
int APS5_VABI sceUpsrvUpdateCheckCreateHandler();
int APS5_VABI sceUpsrvUpdateCheckDestroyHandler();
int APS5_VABI sceUpsrvUpdateCheckDoCheckSystem();
int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemForDevKit();
int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemForSettings();
int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemWithPupInfo();
int APS5_VABI sceUpsrvUpdateCreateUpdateTask();
int APS5_VABI sceUpsrvUpdateDestroyUpdateTask();
int APS5_VABI sceUpsrvUpdateDoCheckBootUpdate();
int APS5_VABI sceUpsrvUpdateDoUpdateSystemEx();
int APS5_VABI sceUpsrvUpdateDoUpdateWithEntitlement();
int APS5_VABI sceUpsrvUpdateDoUpdateWithPupInfo();
int APS5_VABI sceUpsrvUpdateDoUpdateWithPupPath2();
int APS5_VABI sceUpsrvUpdateGetBetaAgreementUrl();
int APS5_VABI sceUpsrvUpdateGetEulaDisplay();
int APS5_VABI sceUpsrvUpdateGetEulaStr();
int APS5_VABI sceUpsrvUpdateGetEulaStrLen();
int APS5_VABI sceUpsrvUpdateGetFeatureListUrl();
int APS5_VABI sceUpsrvUpdateGetFeatureListUrlForSystemExUpdate();
int APS5_VABI sceUpsrvUpdateGetFeatureListUrlForUpdate();
int APS5_VABI sceUpsrvUpdateGetImageWritingProgress();
int APS5_VABI sceUpsrvUpdateGetProgress();
int APS5_VABI sceUpsrvUpdateGetUpdateTaskInfo();
int APS5_VABI sceUpsrvUpdateGetUpdateTaskMgrInfo();
int APS5_VABI sceUpsrvUpdateSetEulaAccept();
int APS5_VABI sceUpsrvUpdateStartUpdateTask();
int APS5_VABI sceUpsrvVerifySystemExBank();

}
