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
    if (package == nullptr) {
        if (details != nullptr) {
            std::memset(details, 0, sizeof(*details));
            details->num_groups = NP_TROPHY2_NUM_GROUPS;
            details->num_trophies = NP_TROPHY2_NUM_TROPHIES;
            details->num_platinum = NP_TROPHY2_NUM_PLATINUM;
            details->num_gold = NP_TROPHY2_NUM_GOLD;
            details->num_silver = NP_TROPHY2_NUM_SILVER;
            details->num_bronze = NP_TROPHY2_NUM_BRONZE;
            std::strncpy(details->title, NP_TROPHY2_GAME_TITLE, sizeof(details->title) - 1);
        }
        if (data != nullptr) {
            std::memset(data, 0, sizeof(*data));
            data->unlocked_trophies = static_cast<std::uint32_t>(NpTrophy2::ContextUnlockCount(context));
            data->unlocked_platinum = NP_TROPHY2_UNLOCKED_PLATINUM;
            data->unlocked_gold = NP_TROPHY2_UNLOCKED_GOLD;
            data->unlocked_silver = NP_TROPHY2_UNLOCKED_SILVER;
            data->unlocked_bronze = NP_TROPHY2_UNLOCKED_BRONZE;
            data->progress_percentage = NP_TROPHY2_PROGRESS_PERCENTAGE;
        }
        return SCE_NP_TROPHY2_OK;
    }
    const auto counts = NpTrophy2::CountTrophies(*package, std::nullopt, NpTrophy2::UnlockedBy(owner->userId, owner->serviceLabel));
    if (details != nullptr) {
        std::memset(details, 0, sizeof(*details));
        details->num_groups = static_cast<std::uint32_t>(package->groups.size());
        details->num_trophies = counts.trophies;
        details->num_platinum = counts.grades[NpTrophy2::GradePlatinum];
        details->num_gold = counts.grades[NpTrophy2::GradeGold];
        details->num_silver = counts.grades[NpTrophy2::GradeSilver];
        details->num_bronze = counts.grades[NpTrophy2::GradeBronze];
        NpTrophy2::CopyText(details->title, sizeof(details->title), package->title);
    }
    if (data != nullptr) {
        std::memset(data, 0, sizeof(*data));
        data->unlocked_trophies = counts.unlocked;
        data->unlocked_platinum = counts.unlockedGrades[NpTrophy2::GradePlatinum];
        data->unlocked_gold = counts.unlockedGrades[NpTrophy2::GradeGold];
        data->unlocked_silver = counts.unlockedGrades[NpTrophy2::GradeSilver];
        data->unlocked_bronze = counts.unlockedGrades[NpTrophy2::GradeBronze];
        data->progress_percentage = counts.Percentage();
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetGameIcon(int context, int handle, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)buffer;
    if (size != nullptr) {
        *size = NP_TROPHY2_ICON_SIZE_NONE;
    }
    throw std::runtime_error(std::string(__func__) + ": icon file not found");
}

int APS5_VABI sceNpTrophy2RegisterUnlockCallback(void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    return SCE_NP_TROPHY2_OK;
}

}
