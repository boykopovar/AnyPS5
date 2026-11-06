#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceRemoteplayGetConnectionStatus(int userId, int* status);
int APS5_VABI sceRemoteplayInitialize(void* heap, size_t heapSize);
int APS5_VABI sceRemoteplayTerminate(void);
int APS5_VABI sceRemoteplayApprove(void);
int APS5_VABI sceRemoteplayClearAllRegistData(void);
int APS5_VABI sceRemoteplayClearConnectHistory(void);
int APS5_VABI sceRemoteplayConfirmDeviceRegist(void);
int APS5_VABI sceRemoteplayDisconnect(void);
int APS5_VABI sceRemoteplayGeneratePinCode(void);
int APS5_VABI sceRemoteplayGetApMode(void);
int APS5_VABI sceRemoteplayGetConnectHistory(void);
int APS5_VABI sceRemoteplayGetConnectUserId(void);
int APS5_VABI sceRemoteplayGetMbusDeviceInfo(void);
int APS5_VABI sceRemoteplayGetOperationStatus(void);
int APS5_VABI sceRemoteplayGetPrimaryUserId(void);
int APS5_VABI sceRemoteplayGetRemoteplayStatus(void);
int APS5_VABI sceRemoteplayGetRpMode(void);
int APS5_VABI sceRemoteplayImeClose(void);
int APS5_VABI sceRemoteplayImeFilterResult(void);
int APS5_VABI sceRemoteplayImeGetEvent(void);
int APS5_VABI sceRemoteplayImeNotify(void);
int APS5_VABI sceRemoteplayImeNotifyEventResult(void);
int APS5_VABI sceRemoteplayImeOpen(void);
int APS5_VABI sceRemoteplayImeSetCaret(void);
int APS5_VABI sceRemoteplayImeSetText(void);
int APS5_VABI sceRemoteplayIsRemoteOskReady(void);
int APS5_VABI sceRemoteplayIsRemotePlaying(void);
int APS5_VABI sceRemoteplayNotifyMbusDeviceRegistComplete(void);
int APS5_VABI sceRemoteplayNotifyNpPushWakeup(void);
int APS5_VABI sceRemoteplayNotifyPinCodeError(void);
int APS5_VABI sceRemoteplayNotifySubmitCode(void);
int APS5_VABI sceRemoteplayNotifyUserDelete(void);
int APS5_VABI sceRemoteplayPrintAllRegistData(void);
int APS5_VABI sceRemoteplayProhibit(void);
int APS5_VABI sceRemoteplayProhibitStreaming(void);
int APS5_VABI sceRemoteplayServerLock(void);
int APS5_VABI sceRemoteplayServerUnLock(void);
int APS5_VABI sceRemoteplaySetApMode(void);
int APS5_VABI sceRemoteplaySetLogLevel(void);
int APS5_VABI sceRemoteplaySetProhibition(void);
int APS5_VABI sceRemoteplaySetProhibitionForVsh(void);
int APS5_VABI sceRemoteplaySetRpMode(void);

}
