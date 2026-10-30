#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI CreateWebApiLibCtx();
int APS5_VABI CryptographyDecryptAES128CBC();
int APS5_VABI CryptographyDecryptAES128CBC_HS256();
int APS5_VABI CryptographyDecryptAES128GCM();
int APS5_VABI CryptographyEncryptAES128CBC();
int APS5_VABI CryptographyEncryptAES128GCM();
int APS5_VABI CryptographyGenerateHmacSha256();
int APS5_VABI DestroyWebApiLibCtx();
int APS5_VABI GetLibHttpCtxId();
int APS5_VABI GetPs4AppCategoryForTitleId();
int APS5_VABI InetStatGetResultNative();
int APS5_VABI InetStatGetStateNative();
int APS5_VABI InetStatStartTestNative();
int APS5_VABI InitializeNetLib();
int APS5_VABI LoginMgrCreateUser();
int APS5_VABI LoginMgrDestroyUser();
int APS5_VABI LoginMgrInitialize();
int APS5_VABI LoginMgrSetLoginFlag();
int APS5_VABI LoginMgrSetUserStatus();
int APS5_VABI LoginMgrTerminate();
int APS5_VABI NpStateManagerRegisterCallback();
int APS5_VABI NpStateManagerRtcSetTick();
int APS5_VABI NpStateManagerStart();
int APS5_VABI NpStateManagerStop();
int APS5_VABI NpStateManagerUnregisterCallback();
int APS5_VABI PSNowGetStatus();
int APS5_VABI PSNowGetStreamingStatus();
int APS5_VABI PSNowInitialize();
int APS5_VABI PSNowReset();
int APS5_VABI PSNowSendCommand();
int APS5_VABI PSNowSetAuthCodes();
int APS5_VABI PSNowSetSettings();
int APS5_VABI PSNowTerminate();
int APS5_VABI RemotePlayClearConnectHistory();
int APS5_VABI RemotePlayConfirmDeviceRegist();
int APS5_VABI RemotePlayDisconnect();
int APS5_VABI RemotePlayGeneratePinCode();
int APS5_VABI RemotePlayGetConnectHistory();
int APS5_VABI RemotePlayGetConnectUserId();
int APS5_VABI RemotePlayGetConnectionStatus();
int APS5_VABI RemotePlayGetRemoteplayStatus();
int APS5_VABI RemotePlayGetRpMode();
int APS5_VABI RemotePlayIsRemotePlaying();
int APS5_VABI RemotePlayNotifyCanWakeup();
int APS5_VABI RemotePlayNotifyPinCodeError();
int APS5_VABI RemotePlayNotifyUserDelete();
int APS5_VABI RemotePlayProhibitStreaming();
int APS5_VABI RemotePlaySetProhibition();
int APS5_VABI RemotePlaySetProhibitionForVsh();
int APS5_VABI RemotePlaySetRpMode();
int APS5_VABI RtcGetCurrentAdNetworkTickNative();
int APS5_VABI RtcGetCurrentDebugNetworkTickNative();
int APS5_VABI RtcGetCurrentNetworkTickNative();
int APS5_VABI TerminateNetLib();
int APS5_VABI WebBrowserGetEnvelopeData();
int APS5_VABI sceRemotePlayClientAddEventListener();
int APS5_VABI sceRemotePlayClientAttachVirtualMicrophone();
int APS5_VABI sceRemotePlayClientDetachVirtualMicrophone();
int APS5_VABI sceRemotePlayClientGetClientStatus();
int APS5_VABI sceRemotePlayClientInitialize();
int APS5_VABI sceRemotePlayClientRemoveController();
int APS5_VABI sceRemotePlayClientShowHomeScreen();
int APS5_VABI sceRemotePlayClientStartMicCapture();
int APS5_VABI sceRemotePlayClientStopMicCapture();
int APS5_VABI sceRemotePlayClientTerminateStreaming();

}
