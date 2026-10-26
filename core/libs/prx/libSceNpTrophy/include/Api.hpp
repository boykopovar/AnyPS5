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
int APS5_VABI sceNpTrophyRegisterContext(int contextId, int handle, std::uint64_t options);
int APS5_VABI sceNpTrophyGetTrophyUnlockState(int contextId, int handle, TrophyFlagArray* flags, std::uint32_t* count);
int APS5_VABI sceNpTrophyUnlockTrophy(int contextId, int handle, int trophyId, int* platinumId);
int APS5_VABI sceNpTrophyGetGameIcon(int contextId, int handle, void* buffer, std::uint64_t* size);
int APS5_VABI sceNpTrophyGetGroupIcon(int contextId, int handle, int groupId, void* buffer, std::uint64_t* size);
int APS5_VABI sceNpTrophyGetTrophyIcon(int contextId, int handle, int trophyId, void* buffer, std::uint64_t* size);
int APS5_VABI sceNpTrophyGetGameInfo(int contextId, int handle, GameDetails* details, GameData* data);
int APS5_VABI sceNpTrophyGetGroupInfo(int contextId, int handle, int groupId, GroupDetails* details, GroupData* data);
int APS5_VABI sceNpTrophyGetTrophyInfo(int contextId, int handle, int trophyId, TrophyDetails* details, TrophyData* data);

}
