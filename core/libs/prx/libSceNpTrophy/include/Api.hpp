#pragma once

#include "SceTypes.hpp"

inline constexpr int NP_TROPHY_NUM_MAX = 128;

struct GameData {
    std::uint64_t size;
    std::uint32_t unlockedTrophies;
    std::uint32_t unlockedPlatinum;
    std::uint32_t unlockedGold;
    std::uint32_t unlockedSilver;
    std::uint32_t unlockedBronze;
    std::uint32_t progressPercentage;
};

struct GameDetails {
    std::uint64_t size;
    std::uint32_t numGroups;
    std::uint32_t numTrophies;
    std::uint32_t numPlatinum;
    std::uint32_t numGold;
    std::uint32_t numSilver;
    std::uint32_t numBronze;
    char title[128];
    char description[1024];
};

struct GroupData {
    std::uint64_t size;
    std::int32_t groupId;
    std::uint32_t unlockedTrophies;
    std::uint32_t unlockedPlatinum;
    std::uint32_t unlockedGold;
    std::uint32_t unlockedSilver;
    std::uint32_t unlockedBronze;
    std::uint32_t progressPercentage;
    std::uint8_t reserved[4];
};

struct GroupDetails {
    std::uint64_t size;
    std::int32_t groupId;
    std::uint32_t numTrophies;
    std::uint32_t numPlatinum;
    std::uint32_t numGold;
    std::uint32_t numSilver;
    std::uint32_t numBronze;
    char title[128];
    char description[1024];
};

struct TrophyData {
    std::uint64_t size;
    std::int32_t trophyId;
    bool unlocked;
    std::uint8_t reserved[3];
    std::uint64_t timestamp;
};

struct TrophyDetails {
    std::uint64_t size;
    std::int32_t trophyId;
    std::int32_t trophyGrade;
    std::int32_t groupId;
    bool hidden;
    std::uint8_t reserved[3];
    char name[128];
    char description[1024];
};

struct TrophyFlagArray {
    std::uint32_t bits[NP_TROPHY_NUM_MAX / 32];
};

extern "C" {

int APS5_VABI sceNpTrophyCreateContext(int* context, int userId, std::uint32_t serviceLabel, std::uint64_t options);
int APS5_VABI sceNpTrophyDestroyContext(int context);
int APS5_VABI sceNpTrophyCreateHandle(int* handle);
int APS5_VABI sceNpTrophyDestroyHandle(int handle);
int APS5_VABI sceNpTrophyAbortHandle(int handle);
int APS5_VABI sceNpTrophyRegisterContext(int contextId, int handle, std::uint64_t options);
int APS5_VABI sceNpTrophyGetTrophyUnlockState(int contextId, int handle, TrophyFlagArray* flags, std::uint32_t* count);
int APS5_VABI sceNpTrophyUnlockTrophy(int contextId, int handle, int trophyId, int* platinumId);
int APS5_VABI sceNpTrophyGetGameIcon(int contextId, int handle, void* buffer, std::uint64_t* size);
int APS5_VABI sceNpTrophyGetGroupIcon(int contextId, int handle, int groupId, void* buffer, std::uint64_t* size);
int APS5_VABI sceNpTrophyGetTrophyIcon(int contextId, int handle, int trophyId, void* buffer, std::uint64_t* size);
int APS5_VABI sceNpTrophyGetGameInfo(int contextId, int handle, GameDetails* details, GameData* data);
int APS5_VABI sceNpTrophyGetGroupInfo(int contextId, int handle, int groupId, GroupDetails* details, GroupData* data);
int APS5_VABI sceNpTrophyGetTrophyInfo(int contextId, int handle, int trophyId, TrophyDetails* details, TrophyData* data);

int APS5_VABI sceNpTrophyCaptureScreenshot();
int APS5_VABI sceNpTrophyConfigGetTrophyDetails();
int APS5_VABI sceNpTrophyConfigGetTrophyFlagArray();
int APS5_VABI sceNpTrophyConfigGetTrophyGroupArray();
int APS5_VABI sceNpTrophyConfigGetTrophyGroupDetails();
int APS5_VABI sceNpTrophyConfigGetTrophySetInfo();
int APS5_VABI sceNpTrophyConfigGetTrophySetInfoInGroup();
int APS5_VABI sceNpTrophyConfigGetTrophySetVersion();
int APS5_VABI sceNpTrophyConfigGetTrophyTitleDetails();
int APS5_VABI sceNpTrophyConfigHasGroupFeature();
int APS5_VABI sceNpTrophyGroupArrayGetNum();
int APS5_VABI sceNpTrophyIntAbortHandle();
int APS5_VABI sceNpTrophyIntCheckNetSyncTitles();
int APS5_VABI sceNpTrophyIntCreateHandle();
int APS5_VABI sceNpTrophyIntDestroyHandle();
int APS5_VABI sceNpTrophyIntGetLocalTrophySummary();
int APS5_VABI sceNpTrophyIntGetProgress();
int APS5_VABI sceNpTrophyIntGetRunningTitle();
int APS5_VABI sceNpTrophyIntGetRunningTitles();
int APS5_VABI sceNpTrophyIntGetTrpIconByUri();
int APS5_VABI sceNpTrophyIntNetSyncTitle();
int APS5_VABI sceNpTrophyIntNetSyncTitles();
int APS5_VABI sceNpTrophyNumInfoGetTotal();
int APS5_VABI sceNpTrophySetInfoGetTrophyFlagArray();
int APS5_VABI sceNpTrophySetInfoGetTrophyNum();
int APS5_VABI sceNpTrophyShowTrophyList();
int APS5_VABI sceNpTrophySystemAbortHandle();
int APS5_VABI sceNpTrophySystemBuildGroupIconUri();
int APS5_VABI sceNpTrophySystemBuildNetTrophyIconUri();
int APS5_VABI sceNpTrophySystemBuildTitleIconUri();
int APS5_VABI sceNpTrophySystemBuildTrophyIconUri();
int APS5_VABI sceNpTrophySystemCheckCallback();
int APS5_VABI sceNpTrophySystemCheckNetSyncTitles();
int APS5_VABI sceNpTrophySystemCheckRecoveryRequired();
int APS5_VABI sceNpTrophySystemCloseStorage();
int APS5_VABI sceNpTrophySystemCreateContext();
int APS5_VABI sceNpTrophySystemCreateHandle();
int APS5_VABI sceNpTrophySystemDbgCtl();
int APS5_VABI sceNpTrophySystemDebugLockTrophy();
int APS5_VABI sceNpTrophySystemDebugUnlockTrophy();
int APS5_VABI sceNpTrophySystemDestroyContext();
int APS5_VABI sceNpTrophySystemDestroyHandle();
int APS5_VABI sceNpTrophySystemDestroyTrophyConfig();
int APS5_VABI sceNpTrophySystemGetDbgParam();
int APS5_VABI sceNpTrophySystemGetDbgParamInt();
int APS5_VABI sceNpTrophySystemGetGroupIcon();
int APS5_VABI sceNpTrophySystemGetLocalTrophySummary();
int APS5_VABI sceNpTrophySystemGetNextTitleFileEntryStatus();
int APS5_VABI sceNpTrophySystemGetProgress();
int APS5_VABI sceNpTrophySystemGetTitleFileStatus();
int APS5_VABI sceNpTrophySystemGetTitleIcon();
int APS5_VABI sceNpTrophySystemGetTitleSyncStatus();
int APS5_VABI sceNpTrophySystemGetTrophyConfig();
int APS5_VABI sceNpTrophySystemGetTrophyData();
int APS5_VABI sceNpTrophySystemGetTrophyGroupData();
int APS5_VABI sceNpTrophySystemGetTrophyIcon();
int APS5_VABI sceNpTrophySystemGetTrophyTitleData();
int APS5_VABI sceNpTrophySystemGetUserFileInfo();
int APS5_VABI sceNpTrophySystemGetUserFileStatus();
int APS5_VABI sceNpTrophySystemIsServerAvailable();
int APS5_VABI sceNpTrophySystemNetSyncTitle();
int APS5_VABI sceNpTrophySystemNetSyncTitles();
int APS5_VABI sceNpTrophySystemOpenStorage();
int APS5_VABI sceNpTrophySystemPerformRecovery();
int APS5_VABI sceNpTrophySystemRegisterTitleSyncedCallback();
int APS5_VABI sceNpTrophySystemRegisterTitleUpdateCallback();
int APS5_VABI sceNpTrophySystemRemoveAll();
int APS5_VABI sceNpTrophySystemRemoveTitleData();
int APS5_VABI sceNpTrophySystemRemoveUserData();
int APS5_VABI sceNpTrophySystemSetDbgParam();
int APS5_VABI sceNpTrophySystemSetDbgParamInt();
int APS5_VABI sceNpTrophySystemUnregisterTitleSyncedCallback();
int APS5_VABI sceNpTrophySystemUnregisterTitleUpdateCallback();
int APS5_VABI sceNpTrophySystemWrapDebugLockTrophy();
int APS5_VABI sceNpTrophySystemWrapDebugUnlockTrophy();
int APS5_VABI sceNpTrophySystemWrapGetGroupDetails();
int APS5_VABI sceNpTrophySystemWrapGetPlayedTrophyTitles();
int APS5_VABI sceNpTrophySystemWrapGetTitleDetails();
int APS5_VABI sceNpTrophySystemWrapGetTrophyDetailsArray();
int APS5_VABI sceNpTrophySystemWrapGetTrophyTitleIdsByNpTitleId();
int APS5_VABI sceNpTrophySystemWrapRemoveAll();
int APS5_VABI sceNpTrophySystemWrapRemoveTitleData();
int APS5_VABI sceNpTrophySystemWrapRemoveUserData();
}

