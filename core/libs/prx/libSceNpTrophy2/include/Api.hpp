#pragma once

#include "prx/libSceNpTrophy2/include/NpTrophy2Types.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2SystemAbortHandle(int handle);
int APS5_VABI sceNpTrophy2SystemBuildBgImageIconUri(const void* npCommId, int bgImageId, char* uriBuf, size_t bufSize);
int APS5_VABI sceNpTrophy2SystemBuildGroupIconUri(const void* npCommId, int groupId, char* uriBuf, size_t bufSize);
int APS5_VABI sceNpTrophy2SystemBuildRewardIconUri(const void* npCommId, int rewardId, char* uriBuf, size_t bufSize);
int APS5_VABI sceNpTrophy2SystemBuildTitleIconUri(const void* npCommId, char* uriBuf, size_t bufSize);
int APS5_VABI sceNpTrophy2SystemBuildTrophyIconUri(const void* npCommId, int trophyId, char* uriBuf, size_t bufSize);
int APS5_VABI sceNpTrophy2SystemCheckCallback(void);
int APS5_VABI sceNpTrophy2SystemCheckNetSyncTitles(void);
int APS5_VABI sceNpTrophy2SystemCreateContext(int* context, int userId, const void* npCommId, uint64_t options);
int APS5_VABI sceNpTrophy2SystemCreateHandle(int* handle);
int APS5_VABI sceNpTrophy2SystemDebugLockTrophy(void);
int APS5_VABI sceNpTrophy2SystemDebugUnlockTrophy(void);
int APS5_VABI sceNpTrophy2SystemDestroyContext(int context);
int APS5_VABI sceNpTrophy2SystemDestroyHandle(int handle);
int APS5_VABI sceNpTrophy2SystemGetTrophyDetails(void);
int APS5_VABI sceNpTrophy2SystemGetTrophyGroupConf(void);
int APS5_VABI sceNpTrophy2SystemGetTrophySetArray(void);
int APS5_VABI sceNpTrophy2SystemGetTrophyTitleConf(void);
int APS5_VABI sceNpTrophy2SystemGetTrophyTitleDetails(void);
int APS5_VABI sceNpTrophy2SystemGetTrpIconByUri(void);
int APS5_VABI sceNpTrophy2SystemIsServerAvailable(bool* available);
int APS5_VABI sceNpTrophy2SystemRegisterTitleSyncedCallback(void* callback, void* userdata);
int APS5_VABI sceNpTrophy2SystemRegisterTitleUpdateCallback(void* callback, void* userdata);
int APS5_VABI sceNpTrophy2SystemRemoveAll(void);
int APS5_VABI sceNpTrophy2SystemRemoveTitleData(void);
int APS5_VABI sceNpTrophy2SystemRemoveUserData(void);
int APS5_VABI sceNpTrophy2SystemUnregisterTitleSyncedCallback(void);
int APS5_VABI sceNpTrophy2SystemUnregisterTitleUpdateCallback(void);
int APS5_VABI sceNpTrophy2CreateContext(int* context, int userId, uint32_t serviceLabel, uint64_t options);
int APS5_VABI sceNpTrophy2DestroyContext(int context);
int APS5_VABI sceNpTrophy2RegisterContext(int context, int handle, uint64_t options);
int APS5_VABI sceNpTrophy2UnregisterUnlockCallback();
int APS5_VABI sceNpTrophy2GetRewardIcon(int context, int handle, int rewardId, void* buffer, size_t* size);
int APS5_VABI sceNpTrophy2ShowTrophyList(int context);
int APS5_VABI sceNpTrophy2GetGameInfo(int context, int handle, NpTrophy2GameDetails* details, NpTrophy2GameData* data);
int APS5_VABI sceNpTrophy2GetGameIcon(int context, int handle, void* buffer, size_t* size);
int APS5_VABI sceNpTrophy2RegisterUnlockCallback(void* callback, void* userdata);
int APS5_VABI sceNpTrophy2GetGroupInfo(int context, int handle, int groupId, NpTrophy2GroupDetails* details, NpTrophy2GroupData* data);
int APS5_VABI sceNpTrophy2GetGroupInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2GroupDetails* detailsArray, NpTrophy2GroupData* dataArray, uint32_t* count);
int APS5_VABI sceNpTrophy2GetGroupIcon(int context, int handle, int groupId, void* buffer, size_t* size);
int APS5_VABI sceNpTrophy2CreateHandle(int* handle);
int APS5_VABI sceNpTrophy2DestroyHandle(int handle);
int APS5_VABI sceNpTrophy2AbortHandle(int handle);
int APS5_VABI sceNpTrophy2GetTrophyInfo(int context, int handle, int trophyId, NpTrophy2Details* details, NpTrophy2Data* data);
int APS5_VABI sceNpTrophy2GetTrophyInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2Details* detailsArray, NpTrophy2Data* dataArray, uint32_t* count);
int APS5_VABI sceNpTrophy2GetTrophyIcon(int context, int handle, int trophyId, void* buffer, size_t* size);

}
