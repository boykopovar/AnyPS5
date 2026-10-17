#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libSceNpTrophy2/include/TitleTrophies.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"
#include "prx/libc/include/General.hpp"

namespace {

void fillPlaceholderDetails(std::int32_t groupId, NpTrophy2GroupDetails* details) {
    std::memset(details, 0, sizeof(*details));
    details->group_id = groupId;
    details->num_trophies = NP_TROPHY2_NUM_TROPHIES;
    details->num_platinum = NP_TROPHY2_NUM_PLATINUM;
    details->num_gold = NP_TROPHY2_NUM_GOLD;
    details->num_silver = NP_TROPHY2_NUM_SILVER;
    details->num_bronze = NP_TROPHY2_NUM_BRONZE;
    std::strncpy(details->title, NP_TROPHY2_GROUP_TITLE, sizeof(details->title) - 1);
}

void fillPlaceholderData(std::int32_t groupId, NpTrophy2GroupData* data) {
    std::memset(data, 0, sizeof(*data));
    data->group_id = groupId;
    data->unlocked_trophies = NP_TROPHY2_UNLOCKED_TROPHIES;
    data->unlocked_platinum = NP_TROPHY2_UNLOCKED_PLATINUM;
    data->unlocked_gold = NP_TROPHY2_UNLOCKED_GOLD;
    data->unlocked_silver = NP_TROPHY2_UNLOCKED_SILVER;
    data->unlocked_bronze = NP_TROPHY2_UNLOCKED_BRONZE;
    data->progress_percentage = NP_TROPHY2_PROGRESS_PERCENTAGE;
}

void fillGroup(const NpTrophy2::TrophyPackage& package, const NpTrophy2::ContextOwner& owner, std::int32_t groupId, const std::string& title, NpTrophy2GroupDetails* details,
               NpTrophy2GroupData* data) {
    const auto counts = NpTrophy2::CountTrophies(package, groupId, NpTrophy2::UnlockedBy(owner.userId, owner.serviceLabel));
    if (details != nullptr) {
        std::memset(details, 0, sizeof(*details));
        details->group_id = groupId;
        details->num_trophies = counts.trophies;
        details->num_platinum = counts.grades[NpTrophy2::GradePlatinum];
        details->num_gold = counts.grades[NpTrophy2::GradeGold];
        details->num_silver = counts.grades[NpTrophy2::GradeSilver];
        details->num_bronze = counts.grades[NpTrophy2::GradeBronze];
        NpTrophy2::CopyText(details->title, sizeof(details->title), title);
    }
    if (data != nullptr) {
        std::memset(data, 0, sizeof(*data));
        data->group_id = groupId;
        data->unlocked_trophies = counts.unlocked;
        data->unlocked_platinum = counts.unlockedGrades[NpTrophy2::GradePlatinum];
        data->unlocked_gold = counts.unlockedGrades[NpTrophy2::GradeGold];
        data->unlocked_silver = counts.unlockedGrades[NpTrophy2::GradeSilver];
        data->unlocked_bronze = counts.unlockedGrades[NpTrophy2::GradeBronze];
        data->progress_percentage = counts.Percentage();
    }
}

}

extern "C" {

int APS5_VABI sceNpTrophy2GetGroupInfo(int context, int handle, int group_id, NpTrophy2GroupDetails* details, NpTrophy2GroupData* data) {
    if (details == nullptr && data == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)handle;
    const auto owner = NpTrophy2::FindContext(context);
    const auto* package = owner ? NpTrophy2::Package(owner->serviceLabel) : nullptr;
    if (package == nullptr) {
        const std::int32_t normalized_group_id = (group_id < 0 ? NP_TROPHY2_GROUP_ID_BASE : group_id);
        if (details != nullptr) fillPlaceholderDetails(normalized_group_id, details);
        if (data != nullptr) fillPlaceholderData(normalized_group_id, data);
        return SCE_NP_TROPHY2_OK;
    }
    const auto group = package->groups.find(group_id);
    if (group == package->groups.end()) {
        throw std::invalid_argument(std::string(__func__) + ": group " + std::to_string(group_id) + " is not in " + NpTrophy2::PackagePath(owner->serviceLabel));
    }
    fillGroup(*package, *owner, group->first, group->second, details, data);
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetGroupInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2GroupDetails* details_array, NpTrophy2GroupData* data_array, uint32_t* count) {
    if (count == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)handle;
    const auto owner = NpTrophy2::FindContext(context);
    const auto* package = owner ? NpTrophy2::Package(owner->serviceLabel) : nullptr;
    if (package == nullptr) {
        const uint32_t out_count = (offset == 0 && limit != 0 ? 1u : 0u);
        *count = out_count;
        if (out_count != 0 && details_array != nullptr) fillPlaceholderDetails(NP_TROPHY2_GROUP_ID_BASE, details_array);
        if (out_count != 0 && data_array != nullptr) fillPlaceholderData(NP_TROPHY2_GROUP_ID_BASE, data_array);
        return SCE_NP_TROPHY2_OK;
    }
    *count = 0;
    auto group = package->groups.begin();
    std::advance(group, std::min<std::size_t>(offset, package->groups.size()));
    for (; group != package->groups.end() && *count < limit; ++group, ++*count) {
        fillGroup(*package, *owner, group->first, group->second, details_array != nullptr ? &details_array[*count] : nullptr, data_array != nullptr ? &data_array[*count] : nullptr);
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetGroupIcon(int context, int handle, int group_id, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)group_id;
    (void)buffer;
    if (size != nullptr) {
        *size = NP_TROPHY2_ICON_SIZE_NONE;
    }
    throw std::runtime_error(std::string(__func__) + ": icon file not found");
}

}
