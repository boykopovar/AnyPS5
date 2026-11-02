#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceRnpsAppMgrBlockAppInstall();
int APS5_VABI sceRnpsAppMgrCreateUpdateRequest();
int APS5_VABI sceRnpsAppMgrCreateUrlRequest();
int APS5_VABI sceRnpsAppMgrDestroyBundle();
int APS5_VABI sceRnpsAppMgrDestroyUpdateRequest();
int APS5_VABI sceRnpsAppMgrFinishUrlRequest();
int APS5_VABI sceRnpsAppMgrGetAppInfo();
int APS5_VABI sceRnpsAppMgrGetAppInfoDebugString();
int APS5_VABI sceRnpsAppMgrGetAppInfoList();
int APS5_VABI sceRnpsAppMgrGetEvent();
int APS5_VABI sceRnpsAppMgrGetEventForShellUI();
int APS5_VABI sceRnpsAppMgrGetMboInfo();
int APS5_VABI sceRnpsAppMgrGetMboManifestDebugString();
int APS5_VABI sceRnpsAppMgrGetUpdateProgress();
int APS5_VABI sceRnpsAppMgrHasEventForShellUI();
int APS5_VABI sceRnpsAppMgrInitialize();
int APS5_VABI sceRnpsAppMgrIsBgsInstallReady();
int APS5_VABI sceRnpsAppMgrIsCrlUpdated();
int APS5_VABI sceRnpsAppMgrIsMboUpdated();
int APS5_VABI sceRnpsAppMgrPushEventForShellUI();
int APS5_VABI sceRnpsAppMgrRecoverUfsImage();
int APS5_VABI sceRnpsAppMgrRegisterShellUI();
int APS5_VABI sceRnpsAppMgrRemoveApp();
int APS5_VABI sceRnpsAppMgrRemoveAppOnSystemShutdown();
int APS5_VABI sceRnpsAppMgrRemoveUfsImageOnSystemShutdown();
int APS5_VABI sceRnpsAppMgrRequestUpdateCheck();
int APS5_VABI sceRnpsAppMgrSetCrlUrl();
int APS5_VABI sceRnpsAppMgrSetEventFlagForClients();
int APS5_VABI sceRnpsAppMgrSetUpdateUrl();
int APS5_VABI sceRnpsAppMgrStartService();
int APS5_VABI sceRnpsAppMgrTerminate();
int APS5_VABI sceRnpsAppMgrUnblockAppInstall();
int APS5_VABI sceRnpsAppMgrUnregisterShellUI();
int APS5_VABI sceRnpsAppMgrVerifyApp();
int APS5_VABI sceRnpsAppMgrVerifyFileInApp();

}
