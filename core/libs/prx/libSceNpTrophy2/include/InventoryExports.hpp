#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2SystemBuildBgImageIconUri();
int APS5_VABI sceNpTrophy2SystemBuildTitleIconUri();
int APS5_VABI sceNpTrophy2SystemDestroyHandle();
int APS5_VABI sceNpTrophy2SystemGetTrophyConf();
int APS5_VABI sceNpTrophy2SystemGetTrophyData();
int APS5_VABI sceNpTrophy2SystemGetTrophyGroupData();
int APS5_VABI sceNpTrophy2SystemGetTrophyGroupDetails();
int APS5_VABI sceNpTrophy2SystemGetTrophyTitleData();
int APS5_VABI sceNpTrophy2SystemListAllTitles();
int APS5_VABI sceNpTrophy2SystemListPlayedTitles();
int APS5_VABI sceNpTrophy2SystemListTitlesByNpTitleId();
int APS5_VABI sceNpTrophy2SystemNetSyncTitles();
int APS5_VABI sceNpTrophy2SystemRemoveUserDataWithServerData();
int APS5_VABI sceNpTrophy2SystemUnregisterTitleSyncedCallback();
int APS5_VABI sceNpTrophy2SystemUnregisterTitleUpdateCallback();

}
