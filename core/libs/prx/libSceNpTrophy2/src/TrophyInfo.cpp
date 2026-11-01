#include "prx/libSceNpTrophy2/include/Api.hpp"
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

void fillPlaceholderDetails(int trophyId, NpTrophy2Details* details) {
    std::memset(details, 0, sizeof(*details));
    details->trophy_id = trophyId;
    details->trophy_grade = NP_TROPHY2_TROPHY_GRADE_BRONZE;
    details->group_id = NP_TROPHY2_GROUP_ID_BASE;
    details->hidden = false;
    details->has_reward = false;
    details->target.type = NP_TROPHY2_PROGRESS_TYPE_NONE;
    details->target.value = NP_TROPHY2_PROGRESS_VALUE_NONE;
    std::strncpy(details->name, NP_TROPHY2_TROPHY_NAME, sizeof(details->name) - 1);
    std::strncpy(details->description, NP_TROPHY2_TROPHY_DESCRIPTION, sizeof(details->description) - 1);
    std::strncpy(details->reward, NP_TROPHY2_TROPHY_REWARD, sizeof(details->reward) - 1);
}

void fillPlaceholderData(int context, int trophyId, NpTrophy2Data* data) {
    std::memset(data, 0, sizeof(*data));
    const auto tick = NpTrophy2::ContextUnlockTick(context, trophyId);
    data->trophy_id = trophyId;
    data->unlocked = tick.has_value();
    data->progress.type = NP_TROPHY2_PROGRESS_TYPE_NONE;
    data->progress.value = NP_TROPHY2_PROGRESS_VALUE_NONE;
    data->timestamp_tick = tick.value_or(0);
}

void fillDetails(const NpTrophy2::TrophyDefinition& trophy, NpTrophy2Details* details) {
    std::memset(details, 0, sizeof(*details));
    details->trophy_id = trophy.id;
    details->trophy_grade = trophy.grade;
    details->group_id = trophy.groupId;
    details->hidden = trophy.hidden;
    details->has_reward = trophy.hasReward;
    if (const auto target = NpTrophy2::ProgressTarget(trophy)) {
        details->target.type = NP_TROPHY2_PROGRESS_TYPE_VALUE;
        details->target.value = *target;
    }
    NpTrophy2::CopyText(details->name, sizeof(details->name), trophy.name);
    NpTrophy2::CopyText(details->description, sizeof(details->description), trophy.detail);
    NpTrophy2::CopyText(details->reward, sizeof(details->reward), trophy.reward);
}

void fillData(const NpTrophy2::ContextOwner& owner, const NpTrophy2::TrophyDefinition& trophy, NpTrophy2Data* data) {
    std::memset(data, 0, sizeof(*data));
    const auto tick = NpTrophy2::TitleStore().UnlockTick(owner.userId, owner.serviceLabel, trophy.id);
    data->trophy_id = trophy.id;
    data->unlocked = tick.has_value();
    data->timestamp_tick = tick.value_or(0);
    if (!NpTrophy2::ProgressTarget(trophy)) return;
    const auto stat = trophy.statId ? NpTrophy2::TitleStats().Value(owner.userId, owner.serviceLabel, *trophy.statId) : std::nullopt;
    data->progress.type = NP_TROPHY2_PROGRESS_TYPE_VALUE;
    data->progress.value = NpTrophy2::TrophyProgress(trophy, stat, data->unlocked);
}

}

extern "C" {

int APS5_VABI sceNpTrophy2GetTrophyInfo(int context, int handle, int trophy_id, NpTrophy2Details* details, NpTrophy2Data* data) {
    if (details == nullptr && data == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)handle;
    const auto owner = NpTrophy2::FindContext(context);
    const auto* package = owner ? NpTrophy2::Package(owner->serviceLabel) : nullptr;
    if (package == nullptr) {
        if (details != nullptr) fillPlaceholderDetails(trophy_id, details);
        if (data != nullptr) fillPlaceholderData(context, trophy_id, data);
        return SCE_NP_TROPHY2_OK;
    }
    const auto trophy = package->trophies.find(trophy_id);
    if (trophy == package->trophies.end()) return SCE_NP_TROPHY2_ERROR_INVALID_TROPHY_ID;
    if (details != nullptr) fillDetails(trophy->second, details);
    if (data != nullptr) fillData(*owner, trophy->second, data);
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetTrophyInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2Details* details_array, NpTrophy2Data* data_array, uint32_t* count) {
    if (count == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)handle;
    const auto owner = NpTrophy2::FindContext(context);
    const auto* package = owner ? NpTrophy2::Package(owner->serviceLabel) : nullptr;
    if (package == nullptr) {
        const uint32_t out_count = (offset == 0 && limit != 0 ? 1u : 0u);
        *count = out_count;
        if (out_count != 0 && details_array != nullptr) fillPlaceholderDetails(NP_TROPHY2_TROPHY_ID_DEFAULT, details_array);
        if (out_count != 0 && data_array != nullptr) fillPlaceholderData(context, NP_TROPHY2_TROPHY_ID_DEFAULT, data_array);
        return SCE_NP_TROPHY2_OK;
    }
    *count = 0;
    auto trophy = package->trophies.begin();
    std::advance(trophy, std::min<std::size_t>(offset, package->trophies.size()));
    for (; trophy != package->trophies.end() && *count < limit; ++trophy, ++*count) {
        if (details_array != nullptr) fillDetails(trophy->second, &details_array[*count]);
        if (data_array != nullptr) fillData(*owner, trophy->second, &data_array[*count]);
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetTrophyIcon(int context, int handle, int trophy_id, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)trophy_id;
    (void)buffer;
    if (size != nullptr) {
        *size = NP_TROPHY2_ICON_SIZE_NONE;
    }
    throw std::runtime_error(std::string(__func__) + ": icon file not found");
}

}
