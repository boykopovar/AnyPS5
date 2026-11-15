#include "prx/libSceNpTrophy2/include/Api.hpp"
#include <cstddef>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libSceNpTrophy2/include/TitleTrophies.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2GetGameInfo(int context, int handle, NpTrophy2GameDetails* details, NpTrophy2GameData* data) {
    if (details == nullptr && data == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)handle;
    const auto owner = NpTrophy2::FindContext(context);
    const auto* package = owner ? NpTrophy2::Package(owner->serviceLabel) : nullptr;
    NpTrophy2GameDetails localDetails{};
    NpTrophy2GameData localData{};
    if (package == nullptr) {
        if (details != nullptr) {
            localDetails.num_groups = NP_TROPHY2_NUM_GROUPS;
            localDetails.num_trophies = NP_TROPHY2_NUM_TROPHIES;
            localDetails.num_platinum = NP_TROPHY2_NUM_PLATINUM;
            localDetails.num_gold = NP_TROPHY2_NUM_GOLD;
            localDetails.num_silver = NP_TROPHY2_NUM_SILVER;
            localDetails.num_bronze = NP_TROPHY2_NUM_BRONZE;
            std::strncpy(localDetails.title, NP_TROPHY2_GAME_TITLE, sizeof(localDetails.title) - 1);
        }
        if (data != nullptr) {
            localData.unlocked_trophies = static_cast<std::uint32_t>(NpTrophy2::ContextUnlockCount(context));
            localData.unlocked_platinum = NP_TROPHY2_UNLOCKED_PLATINUM;
            localData.unlocked_gold = NP_TROPHY2_UNLOCKED_GOLD;
            localData.unlocked_silver = NP_TROPHY2_UNLOCKED_SILVER;
            localData.unlocked_bronze = NP_TROPHY2_UNLOCKED_BRONZE;
            localData.progress_percentage = NP_TROPHY2_PROGRESS_PERCENTAGE;
        }
    } else {
        const auto counts = NpTrophy2::CountTrophies(*package, std::nullopt, NpTrophy2::UnlockedBy(owner->userId, owner->serviceLabel));
        if (details != nullptr) {
            localDetails.num_groups = static_cast<std::uint32_t>(package->groups.size());
            localDetails.num_trophies = counts.trophies;
            localDetails.num_platinum = counts.grades[NpTrophy2::GradePlatinum];
            localDetails.num_gold = counts.grades[NpTrophy2::GradeGold];
            localDetails.num_silver = counts.grades[NpTrophy2::GradeSilver];
            localDetails.num_bronze = counts.grades[NpTrophy2::GradeBronze];
            NpTrophy2::CopyText(localDetails.title, sizeof(localDetails.title), package->title);
        }
        if (data != nullptr) {
            localData.unlocked_trophies = counts.unlocked;
            localData.unlocked_platinum = counts.unlockedGrades[NpTrophy2::GradePlatinum];
            localData.unlocked_gold = counts.unlockedGrades[NpTrophy2::GradeGold];
            localData.unlocked_silver = counts.unlockedGrades[NpTrophy2::GradeSilver];
            localData.unlocked_bronze = counts.unlockedGrades[NpTrophy2::GradeBronze];
            localData.progress_percentage = counts.Percentage();
        }
    }
    if (details != nullptr && !GuestCopyOut_nid_no_patch(details, &localDetails, sizeof(localDetails))) {
        APS5_INVALID_ARG_EX;
    }
    if (data != nullptr && !GuestCopyOut_nid_no_patch(data, &localData, sizeof(localData))) {
        APS5_INVALID_ARG_EX;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetGameIcon(int context, int handle, void* buffer, size_t* size) {
    (void)handle;
    return NpTrophy2::CopyIcon(context, NpTrophy2::IconKind::Game, 0, buffer, size, __func__);
}

int APS5_VABI sceNpTrophy2RegisterUnlockCallback(void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    return SCE_NP_TROPHY2_OK;
}

}
