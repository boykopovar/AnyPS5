#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceShareUtilityAdvanceFromFileWithContentIdList();
int APS5_VABI sceShareUtilityAdvanceFromFileWithTitleIdList();
int APS5_VABI sceShareUtilityAdvanceGetServiceInfo();
int APS5_VABI sceShareUtilityAdvanceInitialize();
int APS5_VABI sceShareUtilityAdvanceQueryServiceInfo();
int APS5_VABI sceShareUtilityAdvanceQueryServiceInfoA();
int APS5_VABI sceShareUtilityAdvanceResetPlayingContent();
int APS5_VABI sceShareUtilityAdvanceSetPlayingContent();
int APS5_VABI sceShareUtilityAdvanceSetUploadContentData();
int APS5_VABI sceShareUtilityAdvanceTerminate();
int APS5_VABI sceShareUtilityAdvanceValidateContents();
int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenu();
int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuBroadcast();
int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuDefault();
int APS5_VABI sceShareUtilityAdvanceWakeUpShareMenuForShareContent();
int APS5_VABI sceShareUtilityInitialize();
int APS5_VABI sceShareUtilityInitializeEx2();
int APS5_VABI sceShareUtilityOpenShareMenu();
int APS5_VABI sceShareUtilityOpenShareMenuBroadcast();
int APS5_VABI sceShareUtilityOpenShareMenuDefault();
int APS5_VABI sceShareUtilityOpenShareMenuForShareContent();
int APS5_VABI sceShareUtilityTerminate();

}
