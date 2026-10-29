#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceLoginMgrServerCheckSafetyNoticeShown();
int APS5_VABI sceLoginMgrServerClearMorpheusRequiredUserId();
int APS5_VABI sceLoginMgrServerConvertToUserId();
int APS5_VABI sceLoginMgrServerCreateUser();
int APS5_VABI sceLoginMgrServerDestroyUser();
int APS5_VABI sceLoginMgrServerDialogGetAppId();
int APS5_VABI sceLoginMgrServerDialogGetOpenParam();
int APS5_VABI sceLoginMgrServerDialogNotifyCloseFinished();
int APS5_VABI sceLoginMgrServerDialogNotifyOpenFinished();
int APS5_VABI sceLoginMgrServerDialogSetResult();
int APS5_VABI sceLoginMgrServerGetLogoutInfo();
int APS5_VABI sceLoginMgrServerGetMorpheusRequiredUserId();
int APS5_VABI sceLoginMgrServerGetSharePlayAllowPadOperation();
int APS5_VABI sceLoginMgrServerInitialize();
int APS5_VABI sceLoginMgrServerInitializeSharePlayAllowPadOperation();
int APS5_VABI sceLoginMgrServerIsRequestedCdlgClose();
int APS5_VABI sceLoginMgrServerLoginServiceGetRequestParam();
int APS5_VABI sceLoginMgrServerLoginServiceNotifyRequestFinished();
int APS5_VABI sceLoginMgrServerNotifyEasySignInFinished();
int APS5_VABI sceLoginMgrServerNotifyEasySignInUserCode();
int APS5_VABI sceLoginMgrServerNotifyHmdConnectInfo();
int APS5_VABI sceLoginMgrServerNotifySafetyNoticeShown();
int APS5_VABI sceLoginMgrServerNotifyShellUIState();
int APS5_VABI sceLoginMgrServerSetLoginFlag();
int APS5_VABI sceLoginMgrServerSetMorpheusAssignEnabledFlag();
int APS5_VABI sceLoginMgrServerSetMorpheusRequiredUserId();
int APS5_VABI sceLoginMgrServerSetSharePlayMode();
int APS5_VABI sceLoginMgrServerSetUserStatus();
int APS5_VABI sceLoginMgrServerTerminate();
int APS5_VABI sceLoginMgrServerUpdateUserIdRalatedToPadUniqueId();

}
