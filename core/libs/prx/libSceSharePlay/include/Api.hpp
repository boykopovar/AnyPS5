#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSharePlayInitialize(void* heap, size_t heapSize);
int APS5_VABI sceSharePlayTerminate(void);
int APS5_VABI sceSharePlayGetCurrentConnectionInfoA(SharePlayConnectionInfoA* info);
int APS5_VABI sceSharePlayProhibitController(std::uint8_t prohibited);
int APS5_VABI sceSharePlayCrashDaemon(void);
int APS5_VABI sceSharePlayGetCurrentConnectionInfo(SharePlayConnectionInfo* info);
int APS5_VABI sceSharePlayGetCurrentInfo(void);
int APS5_VABI sceSharePlayGetEvent(void);
int APS5_VABI sceSharePlayNotifyDialogOpen(void);
int APS5_VABI sceSharePlayNotifyForceCloseForCdlg(void);
int APS5_VABI sceSharePlayResumeScreenForCdlg(void);
int APS5_VABI sceSharePlayServerLock(void);
int APS5_VABI sceSharePlayServerUnLock(void);
int APS5_VABI sceSharePlaySetMode(void);
int APS5_VABI sceSharePlaySetProhibition(void);
int APS5_VABI sceSharePlaySetProhibitionModeWithAppId(void);
int APS5_VABI sceSharePlayStartStandby(void);
int APS5_VABI sceSharePlayStartStreaming(void);
int APS5_VABI sceSharePlayStopStandby(void);
int APS5_VABI sceSharePlayStopStreaming(void);
int APS5_VABI sceSharePlaySetShareScreenStatus();

}
